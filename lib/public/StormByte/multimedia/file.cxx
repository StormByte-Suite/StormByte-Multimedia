/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Multimedia.
 *
 * StormByte-Multimedia original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Multimedia source in this
 * file. Third-party components — including FFmpeg and embedded trained data —
 * remain under their own licenses and are not covered by the commercial grant.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Multimedia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Multimedia. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/multimedia/backend/file_avio.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/cover.hxx>
#include <StormByte/multimedia/backend/local_file_reader.hxx>
#include <StormByte/multimedia/detail/probe.hxx>
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVStream.hxx>
#include <StormByte/multimedia/ffmpeg/property.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/property/hdr10.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/binary.hxx>
#include <StormByte/safe/unordered_map.hxx>
#include <StormByte/safe/unordered_set.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/size.hxx>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
}

using namespace StormByte::Multimedia;
namespace FFmpeg = StormByte::Multimedia::FFmpeg;
using StormByte::Buffer::IO::BufferedLocationReader;

namespace {
	constexpr int Hdr10PlusVideoPackets = 48;

	StormByte::Safe::Unique<BufferedLocationReader> LocalReader(const StormByte::Safe::String& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileReader(path);
	}

	void ReportProgress(const File::DurationProgress* progress, double percent) noexcept {
		if (!progress || !progress->HasValue())
			return;
		try {
			static_cast<void>(progress->Call(percent));
		}
		catch (...) {}
	}

	ExpectedFile FailOpen(std::string_view label, std::string_view reason) noexcept {
		return Unexpected(FilePathOpenException(label, reason));
	}

	ExpectedContainer ResolveContainer(std::string_view formatName, std::string_view path) noexcept {
		auto& registry = Registry::Instance();
		const auto separator = path.find_last_of("/\\");
		const auto dot = path.find_last_of('.');
		if (dot != std::string_view::npos && dot + 1 < path.size() &&
			(separator == std::string_view::npos || dot > separator)) {
			std::string extension{path.substr(dot + 1)};
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
				return static_cast<char>(std::tolower(character));
			});
			auto byExtension = registry.FindContainer(extension);
			if (byExtension.has_value())
				return byExtension;
		}
		std::string_view rest = formatName;
		while (!rest.empty()) {
			const auto comma = rest.find(',');
			const auto token = rest.substr(0, comma);
			if (!token.empty()) {
				auto found = registry.FindContainer(token);
				if (found.has_value())
					return found;
			}
			if (comma == std::string_view::npos)
				break;
			rest = rest.substr(comma + 1);
		}
		return Unexpected<ContainerNotFoundException>(std::string(formatName));
	}

	ExpectedCodec ResolveCodec(const FFmpeg::AVStream& stream) noexcept {
		const auto params = stream.CodecParameters();
		const char* name = avcodec_get_name(static_cast<AVCodecID>(params.CodecId()));
		if (!name || name[0] == '\0' || std::string_view(name) == "none")
			return Unexpected<CodecNotFoundException>(std::string("unknown"));
		return Registry::Instance().FindCodec(name);
	}

	std::optional<std::chrono::nanoseconds> TicksToNs(std::int64_t ticks, Property::AVRational timeBase) noexcept {
		if (ticks < 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, Property::AVRational{1, 1000000000});
		if (ns < 0)
			return std::nullopt;
		return std::chrono::nanoseconds{ns};
	}

	StormByte::Safe::Binary AttachmentBytes(const FFmpeg::AVStream& stream) noexcept {
		StormByte::Safe::Binary bytes;
		const ::AVStream* raw = stream.Raw();
		if (raw && raw->attached_pic.size > 0 && raw->attached_pic.data) {
			const auto* p = reinterpret_cast<const std::byte*>(raw->attached_pic.data);
			bytes.assign(p, p + raw->attached_pic.size);
			return bytes;
		}
		if (raw && raw->codecpar && raw->codecpar->extradata_size > 0 && raw->codecpar->extradata) {
			const auto* p = reinterpret_cast<const std::byte*>(raw->codecpar->extradata);
			bytes.assign(p, p + raw->codecpar->extradata_size);
		}
		return bytes;
	}

	Attachment MakeAttachment(const FFmpeg::AVStream& stream) noexcept {
		const auto described = Detail::MakeAttachment(stream);
		auto name = described.FileName();
		auto mime = described.MimeType();
		return Attachment(std::move(name), std::move(mime),
			StormByte::Buffer::FIFO{AttachmentBytes(stream)});
	}

	void FillEmptyAttachmentPayloads(FFmpeg::AVFormatContext& ctx,
		StormByte::Safe::Vector<Attachment>& attachments, const StormByte::Safe::Vector<int>& coverIndex) noexcept {
		bool missing = false;
		for (const auto& item : std::as_const(attachments)) {
			if (item.Payload().Available() == 0) {
				missing = true;
				break;
			}
		}
		if (!missing || coverIndex.empty())
			return;

		FFmpeg::AVPacket packet;
		std::size_t filled = 0;
		for (;;) {
			const auto result = ctx.ReadPacket(packet);
			if (result == FFmpeg::OperationResult::EndOfFile)
				break;
			if (result == FFmpeg::OperationResult::TryAgain)
				continue;
			if (result != FFmpeg::OperationResult::Success)
				break;

			const int index = packet.StreamIndex();
			for (std::size_t n = 0; n < coverIndex.size(); ++n) {
				if (coverIndex[n] != index)
					continue;
				const Attachment attachment = std::as_const(attachments)[n];
				if (attachment.Payload().Available() != 0)
					break;
				StormByte::Safe::Binary bytes;
				if (const auto* data = packet.Data(); data && packet.Size() > 0) {
					const auto* raw = reinterpret_cast<const std::byte*>(data);
					bytes.assign(raw, raw + packet.Size());
				}
				attachments[n] = Attachment(
					attachment.FileName(),
					attachment.MimeType(),
					StormByte::Buffer::FIFO{std::move(bytes)});
				++filled;
				break;
			}
			packet.Unref();
			if (filled == coverIndex.size())
				break;
		}
	}

	bool PacketHasHdr10Plus(const FFmpeg::AVPacket& packet) noexcept {
		const int n = packet.SideDataCount();
		for (int i = 0; i < n; ++i) {
			if (packet.SideDataType(i) == AV_PKT_DATA_DYNAMIC_HDR10_PLUS)
				return true;
		}
		return false;
	}

	void ReleaseProbe(::AVFormatContext*& raw) noexcept {
		if (!raw)
			return;
		raw->pb = nullptr;
		avformat_free_context(raw);
		raw = nullptr;
	}

	bool OpenAvio(BufferedLocationReader& reader, ::AVFormatContext*& raw,
		Backend::FileAvio& avio) noexcept {
		if (!reader.IsOpen() && !reader.Open())
			return false;
		if (!reader.Rewind())
			return false;
		if (!avio.Arm() || !avio.Context())
			return false;
		raw = avformat_alloc_context();
		if (!raw)
			return false;
		raw->pb = avio.Context();
		raw->flags |= AVFMT_FLAG_CUSTOM_IO;
		if (avformat_open_input(&raw, nullptr, nullptr, nullptr) < 0) {
			ReleaseProbe(raw);
			return false;
		}
		if (avformat_find_stream_info(raw, nullptr) < 0) {
			ReleaseProbe(raw);
			return false;
		}
		return true;
	}
}

File::File(StormByte::Safe::String path, BufferedLocationReader* reader, const class Container& container,
	StormByte::Safe::Vector<Stream> streams, StormByte::Safe::Vector<Attachment> attachments, Metadata::File metadata,
	StormByte::Safe::Optional<Property::Duration> duration, bool durationResolved) noexcept
: m_path(std::move(path)), m_reader(reader), m_container(container), m_streams(std::move(streams)),
m_attachments(std::move(attachments)), m_metadata(std::move(metadata)),
m_duration(std::move(duration)), m_durationResolved(durationResolved) {}

File::File(File&&) noexcept = default;

File::~File() noexcept = default;

ExpectedFile File::Open(const StormByte::Safe::String& path,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept {
	auto reader = LocalReader(path);
	return Probe(*reader, std::move(duration), path, nullptr);
}

ExpectedFile File::Open(BufferedLocationReader& reader,
	StormByte::Safe::Optional<std::int64_t> duration) noexcept {
	return Probe(reader, std::move(duration), {}, &reader);
}

void File::ScanWithReader(BufferedLocationReader& reader, StormByte::Safe::Vector<Stream>& streams,
	StormByte::Safe::Optional<Property::Duration>& duration, const DurationProgress* progress) noexcept {
	double percent = 0.0;
	Backend::FileAvio avio(reader);
	::AVFormatContext* raw = nullptr;
	if (!OpenAvio(reader, raw, avio)) {
		static_cast<void>(reader.Rewind());
		return;
	}
	auto wrapped = FFmpeg::AVFormatContext::WrapBorrowed(raw, false);
	raw = nullptr;
	ReportProgress(progress, percent);
	const bool complete = ScanDurations(wrapped, streams, duration, reader, progress, percent);
	const bool rewound = reader.Rewind();
	ReportProgress(progress, complete && rewound ? 100.0 : percent);
}

void File::MarkHdr10Plus(Stream& stream) noexcept {
	if (!stream.m_properties.first.has_value())
		return;
	const auto video = stream.m_properties.first.value();
	auto hdr10 = video.HDR10().value_or(Property::HDR10{});
	hdr10.HDR10Plus(true);
	stream.m_properties.first = Property::Video(
		video.Color(), video.Resolution(), std::move(hdr10),
		video.FrameRate(), video.SampleAspectRatio());
}

void File::DetectHdr10Plus(FFmpeg::AVFormatContext& ctx, StormByte::Safe::Vector<Stream>& streams) noexcept {
	StormByte::Safe::UnorderedSet<int> video;
	StormByte::Safe::UnorderedSet<int> found;
	for (const auto& stream : std::as_const(streams)) {
		if (stream.Type() == Multimedia::Type::Video)
			video.insert(stream.Index());
	}
	if (video.empty())
		return;

	FFmpeg::AVPacket packet;
	int seenVideo = 0;
	for (;;) {
		if (found.size() == video.size())
			break;
		if (seenVideo >= Hdr10PlusVideoPackets)
			break;
		const auto result = ctx.ReadPacket(packet);
		if (result == FFmpeg::OperationResult::EndOfFile)
			break;
		if (result == FFmpeg::OperationResult::TryAgain)
			continue;
		if (result != FFmpeg::OperationResult::Success)
			break;

		const int index = packet.StreamIndex();
		if (!video.contains(index)) {
			packet.Unref();
			continue;
		}
		++seenVideo;
		if (PacketHasHdr10Plus(packet))
			found.insert(index);
		packet.Unref();
	}

	for (std::size_t index = 0; index < streams.size(); ++index) {
		auto stream = std::as_const(streams)[index];
		if (found.contains(stream.Index())) {
			MarkHdr10Plus(stream);
			streams[index] = stream;
		}
	}
}

bool File::ScanDurations(FFmpeg::AVFormatContext& ctx, StormByte::Safe::Vector<Stream>& streams,
	StormByte::Safe::Optional<Property::Duration>& container, BufferedLocationReader& reader,
	const DurationProgress* progress, double& percent) noexcept {
	auto published = std::chrono::steady_clock::now();
	std::uint64_t payloadBytes = 0;
	const bool hasPrimaryVideo = Detail::HasPrimaryVideo(ctx);
	StormByte::Safe::UnorderedMap<int, StormByte::Size> byIndex;
	StormByte::Safe::Vector<Property::AVRational> timeBase;
	StormByte::Safe::Vector<std::int64_t> endTick;
	std::size_t i = 0;
	const auto sourceStreams = ctx.Streams();
	for (const auto& stream : sourceStreams) {
		if (Detail::IsContainerAttachment(stream) || Detail::IsCoverStream(stream, hasPrimaryVideo))
			continue;
		byIndex.emplace(stream.Index(), i);
		timeBase.push_back(stream.TimeBase());
		endTick.push_back(AV_NOPTS_VALUE);
		++i;
	}

	FFmpeg::AVPacket packet;
	bool complete = false;
	for (;;) {
		const auto result = ctx.ReadPacket(packet);
		if (result == FFmpeg::OperationResult::EndOfFile) {
			complete = true;
			break;
		}
		if (result == FFmpeg::OperationResult::TryAgain)
			continue;
		if (result != FFmpeg::OperationResult::Success)
			break;
		if (progress && progress->HasValue()) {
			const auto size = reader.Size().value_or(0);
			if (size != 0) {
				const auto payloadSize = static_cast<std::uint64_t>(std::max(packet.Size(), 0));
				payloadBytes += payloadSize;
				const auto position = packet.Position();
				const auto processed = position >= 0
					? static_cast<std::uint64_t>(position) + payloadSize : payloadBytes;
				const auto now = std::chrono::steady_clock::now();
				const double current = static_cast<double>(processed) * 100.0 / static_cast<double>(size);
				const double next = std::max(percent, std::min(current, 99.99));
				if (next - percent >= 1.0 || now - published >= std::chrono::milliseconds(20)) {
					percent = next;
					published = now;
					ReportProgress(progress, percent);
				}
			}
		}
		const auto hit = byIndex.find(packet.StreamIndex());
		if (hit == byIndex.end())
			continue;
		std::int64_t pts = packet.Pts();
		if (pts == AV_NOPTS_VALUE)
			continue;
		const std::int64_t dur = packet.Duration();
		if (dur > 0)
			pts += dur;
		std::int64_t& end = endTick[static_cast<std::size_t>(hit->second)];
		if (end == AV_NOPTS_VALUE || pts > end)
			end = pts;
	}

	StormByte::Safe::Optional<Property::Duration> longest;
	for (std::size_t n = 0; n < endTick.size(); ++n) {
		auto ns = TicksToNs(endTick[n], timeBase[n]);
		if (!ns.has_value())
			continue;
		const Property::Duration measured{*ns};
		if (n < streams.size()) {
			auto stream = std::as_const(streams)[n];
			if (!stream.m_duration.has_value()) {
				stream.m_duration = measured;
				streams[n] = stream;
			}
		}
		if (!longest.has_value() || measured.Nanoseconds() > longest->Nanoseconds())
			longest = measured;
	}
	if (!container.has_value())
		container = longest;
	return complete;
}

ExpectedFile File::Probe(BufferedLocationReader& reader,
	StormByte::Safe::Optional<std::int64_t> knownDuration,
	StormByte::Safe::String path, BufferedLocationReader* borrowed) noexcept {
	const auto label = reader.Path();
	Backend::FileAvio avio(reader);
	::AVFormatContext* raw = nullptr;
	if (!OpenAvio(reader, raw, avio)) {
		static_cast<void>(reader.Rewind());
		return FailOpen(label, "AVIO probe failed");
	}

	auto wrapped = FFmpeg::AVFormatContext::WrapBorrowed(raw);
	raw = nullptr;

	const auto formatName = wrapped.FormatName();
	if (formatName.empty()) {
		static_cast<void>(reader.Rewind());
		return FailOpen(label, "unknown container format");
	}

	auto container = ResolveContainer(formatName, label);
	if (!container.has_value()) {
		static_cast<void>(reader.Rewind());
		return FailOpen(label, container.error()->what());
	}

	const bool hasPrimaryVideo = Detail::HasPrimaryVideo(wrapped);
	StormByte::Safe::Vector<Stream> streams;
	StormByte::Safe::Vector<Attachment> attachments;
	StormByte::Safe::Vector<int> coverIndex;
	const auto sourceStreams = wrapped.Streams();
	for (const auto& stream : sourceStreams) {
		if (Detail::IsContainerAttachment(stream) || Detail::IsCoverStream(stream, hasPrimaryVideo)) {
			attachments.push_back(MakeAttachment(stream));
			coverIndex.push_back(stream.Index());
			continue;
		}
		auto codec = ResolveCodec(stream);
		if (!codec.has_value()) {
			static_cast<void>(reader.Rewind());
			return FailOpen(label, codec.error()->what());
		}
		streams.emplace_back(Stream(
			stream.Index(),
			codec.value().get(),
			Detail::Probe::Stream(stream),
			stream.Duration(),
			FFmpeg::MapProperties(stream)
		));
	}

	FillEmptyAttachmentPayloads(wrapped, attachments, coverIndex);
	DetectHdr10Plus(wrapped, streams);
	auto metadata = Detail::Probe::File(wrapped);
	StormByte::Safe::Optional<Property::Duration> duration;
	bool resolved = false;
	if (knownDuration.has_value()) {
		duration = Property::Duration{std::chrono::nanoseconds{knownDuration.value()}};
		resolved = true;
	}
	else {
		duration = wrapped.Duration();
	}

	if (!reader.Rewind())
		return FailOpen(label, "reader rewind failed");

	File snapshot(std::move(path), borrowed, container.value().get(),
		std::move(streams), std::move(attachments), std::move(metadata),
		std::move(duration), resolved);
	for (const auto& stream : sourceStreams) {
		auto stored = StormByte::Safe::Shared<FFmpeg::AVCodecParameters>::MakePointer<FFmpeg::AVCodecParameters>(nullptr);
		*stored = stream.CodecParameters();
		snapshot.m_codecParameters.emplace(stream.Index(), std::move(stored));
	}
	return snapshot;
}

StormByte::Safe::String File::Path() const {
	return m_reader ? m_reader->Path() : m_path;
}

const StormByte::Safe::Vector<Attachment>& File::Attachments() const noexcept {
	return m_attachments;
}

const StormByte::Safe::Optional<Property::Duration>& File::Duration() const noexcept {
	if (!m_durationResolved)
		ResolveDuration(nullptr);
	return m_duration;
}

const StormByte::Safe::Optional<Property::Duration>& File::Duration(const DurationProgress& progress) const noexcept {
	if (!m_durationResolved)
		ResolveDuration(&progress);
	return m_duration;
}

void File::ResolveDuration(const DurationProgress* progress) const noexcept {
	m_durationResolved = true;
	if (m_reader)
		ScanWithReader(*m_reader, m_streams, m_duration, progress);
	else {
		auto reader = LocalReader(m_path);
		ScanWithReader(*reader, m_streams, m_duration, progress);
	}
}
