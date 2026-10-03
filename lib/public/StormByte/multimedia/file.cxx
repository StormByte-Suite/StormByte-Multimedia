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
#include <StormByte/safe/wstring.hxx>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

	StormByte::Safe::String LocationText(const std::filesystem::path& path) {
		const auto native = path.wstring();
		return StormByte::Safe::String{StormByte::Safe::WString{std::wstring_view{native}}};
	}

	StormByte::Safe::Unique<BufferedLocationReader> LocalReader(const std::filesystem::path& path) {
		return StormByte::Multimedia::Backend::MakeLocalFileReader(LocationText(path));
	}

	ExpectedFile FailOpen(std::string_view label, std::string_view reason) noexcept {
		return Unexpected(FilePathOpenException(label, reason));
	}

	ExpectedContainer ResolveContainer(std::string_view formatName) noexcept {
		auto& registry = Registry::Instance();
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

	std::optional<Property::Duration> WrapDuration(const std::optional<std::chrono::nanoseconds>& ns) noexcept {
		if (!ns.has_value())
			return std::nullopt;
		return Property::Duration{*ns};
	}

	StormByte::BinaryData AttachmentBytes(const FFmpeg::AVStream& stream) noexcept {
		StormByte::BinaryData bytes;
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
		std::optional<std::string> name;
		std::optional<std::string> mime;
		if (const char* filename = stream.Tag("filename"))
			name = filename;
		if (const char* mimeType = stream.Tag("mimetype"))
			mime = mimeType;
		return Attachment(std::move(name), std::move(mime),
			StormByte::Buffer::FIFO{AttachmentBytes(stream)});
	}

	void FillEmptyAttachmentPayloads(FFmpeg::AVFormatContext& ctx,
		Multimedia::Attachments& attachments, const std::vector<int>& coverIndex) noexcept {
		bool missing = false;
		for (const auto& item : attachments) {
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
				if (attachments[n].Payload().Available() != 0)
					break;
				StormByte::BinaryData bytes;
				if (const auto* data = packet.Data(); data && packet.Size() > 0) {
					const auto* raw = reinterpret_cast<const std::byte*>(data);
					bytes.assign(raw, raw + packet.Size());
				}
				attachments[n] = Attachment(
					attachments[n].FileName(),
					attachments[n].MimeType(),
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

File::File(Origin origin, const class Container& container,
	Multimedia::Streams streams, Multimedia::Attachments attachments, Metadata::File metadata,
	std::optional<Property::Duration> duration, bool durationResolved) noexcept
: m_origin(std::move(origin)), m_container(container), m_streams(std::move(streams)),
m_attachments(std::move(attachments)), m_metadata(std::move(metadata)),
m_duration(duration), m_durationResolved(durationResolved) {}

File::File(File&&) noexcept = default;
File::~File() noexcept = default;

ExpectedFile File::Open(const std::filesystem::path& path,
	std::optional<std::chrono::nanoseconds> duration) noexcept {
	auto reader = LocalReader(path);
	return Probe(*reader, duration, Origin{path});
}

ExpectedFile File::Open(BufferedLocationReader& reader,
	std::optional<std::chrono::nanoseconds> duration) noexcept {
	return Probe(reader, duration, Origin{std::ref(reader)});
}

void File::ScanWithReader(BufferedLocationReader& reader, Multimedia::Streams& streams,
	std::optional<Property::Duration>& duration) noexcept {
	Backend::FileAvio avio(reader);
	::AVFormatContext* raw = nullptr;
	if (!OpenAvio(reader, raw, avio)) {
		static_cast<void>(reader.Rewind());
		return;
	}
	auto wrapped = FFmpeg::AVFormatContext::WrapBorrowed(raw);
	raw = nullptr;
	ScanDurations(wrapped, streams, duration);
	static_cast<void>(reader.Rewind());
}

void File::MarkHdr10Plus(Stream& stream) noexcept {
	auto* video = std::get_if<Property::Video>(&stream.m_properties);
	if (!video)
		return;
	if (video->HDR10().has_value()) {
		const_cast<Property::HDR10&>(*video->HDR10()).HDR10Plus(true);
		return;
	}
	stream.m_properties = Property::Video(
		video->Color(), video->Resolution(), Property::HDR10{},
		video->FrameRate(), video->SampleAspectRatio());
	if (auto* updated = std::get_if<Property::Video>(&stream.m_properties)) {
		if (updated->HDR10().has_value())
			const_cast<Property::HDR10&>(*updated->HDR10()).HDR10Plus(true);
	}
}

void File::DetectHdr10Plus(FFmpeg::AVFormatContext& ctx, Multimedia::Streams& streams) noexcept {
	std::unordered_set<int> video;
	std::unordered_set<int> found;
	for (const auto& stream : streams) {
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

	for (auto& stream : streams) {
		if (found.contains(stream.Index()))
			MarkHdr10Plus(stream);
	}
}

void File::ScanDurations(FFmpeg::AVFormatContext& ctx, Multimedia::Streams& streams,
	std::optional<Property::Duration>& container) noexcept {
	const bool hasPrimaryVideo = Detail::HasPrimaryVideo(ctx);
	std::unordered_map<int, std::size_t> byIndex;
	std::vector<Property::AVRational> timeBase;
	std::vector<std::int64_t> endTick;
	std::size_t i = 0;
	for (const auto& stream : ctx.Streams()) {
		if (Detail::IsContainerAttachment(stream) || Detail::IsCoverStream(stream, hasPrimaryVideo))
			continue;
		byIndex.emplace(stream.Index(), i);
		timeBase.push_back(stream.TimeBase());
		endTick.push_back(AV_NOPTS_VALUE);
		++i;
	}

	FFmpeg::AVPacket packet;
	for (;;) {
		const auto result = ctx.ReadPacket(packet);
		if (result == FFmpeg::OperationResult::EndOfFile)
			break;
		if (result == FFmpeg::OperationResult::TryAgain)
			continue;
		if (result != FFmpeg::OperationResult::Success)
			break;
		const auto hit = byIndex.find(packet.StreamIndex());
		if (hit == byIndex.end())
			continue;
		std::int64_t pts = packet.Pts();
		if (pts == AV_NOPTS_VALUE)
			continue;
		const std::int64_t dur = packet.Duration();
		if (dur > 0)
			pts += dur;
		std::int64_t& end = endTick[hit->second];
		if (end == AV_NOPTS_VALUE || pts > end)
			end = pts;
	}

	std::optional<Property::Duration> longest;
	for (std::size_t n = 0; n < endTick.size(); ++n) {
		auto ns = TicksToNs(endTick[n], timeBase[n]);
		if (!ns.has_value())
			continue;
		const Property::Duration measured{*ns};
		if (n < streams.size() && !streams[n].m_duration.has_value())
			streams[n].m_duration = measured;
		if (!longest.has_value() || measured.Nanoseconds() > longest->Nanoseconds())
			longest = measured;
	}
	if (!container.has_value())
		container = longest;
}

ExpectedFile File::Probe(BufferedLocationReader& reader,
	std::optional<std::chrono::nanoseconds> knownDuration,
	Origin origin) noexcept {
	const auto label = reader.Path();
	Backend::FileAvio avio(reader);
	::AVFormatContext* raw = nullptr;
	if (!OpenAvio(reader, raw, avio)) {
		static_cast<void>(reader.Rewind());
		return FailOpen(label, "AVIO probe failed");
	}

	auto wrapped = FFmpeg::AVFormatContext::WrapBorrowed(raw);
	raw = nullptr;

	const char* formatName = wrapped.FormatName();
	if (!formatName) {
		static_cast<void>(reader.Rewind());
		return FailOpen(label, "unknown container format");
	}

	auto container = ResolveContainer(formatName);
	if (!container.has_value()) {
		static_cast<void>(reader.Rewind());
		return FailOpen(label, container.error()->what());
	}

	const bool hasPrimaryVideo = Detail::HasPrimaryVideo(wrapped);
	Multimedia::Streams streams;
	Multimedia::Attachments attachments;
	std::vector<int> coverIndex;
	for (const auto& stream : wrapped.Streams()) {
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
			codec.value(),
			Detail::Probe::Stream(stream),
			WrapDuration(stream.Duration()),
			FFmpeg::MapProperties(stream)
		));
	}

	FillEmptyAttachmentPayloads(wrapped, attachments, coverIndex);
	DetectHdr10Plus(wrapped, streams);
	auto metadata = Detail::Probe::File(wrapped);
	std::optional<Property::Duration> duration;
	bool resolved = false;
	if (knownDuration.has_value()) {
		duration = Property::Duration{*knownDuration};
		resolved = true;
	}
	else {
		duration = WrapDuration(wrapped.Duration());
	}

	if (!reader.Rewind())
		return FailOpen(label, "reader rewind failed");

	return File(std::move(origin), container.value(),
		std::move(streams), std::move(attachments), std::move(metadata),
		std::move(duration), resolved);
}

StormByte::Safe::String File::Path() const {
	if (const auto* path = std::get_if<std::filesystem::path>(&m_origin))
		return LocationText(*path);
	return std::get<std::reference_wrapper<BufferedLocationReader>>(m_origin).get().Path();
}

const Multimedia::Attachments& File::Attachments() const noexcept {
	return m_attachments;
}

const std::optional<Property::Duration>& File::Duration() const noexcept {
	if (!m_durationResolved)
		ResolveDuration();
	return m_duration;
}

void File::ResolveDuration() const noexcept {
	m_durationResolved = true;
	std::visit([&](auto& held) {
		using Held = std::decay_t<decltype(held)>;
		if constexpr (std::is_same_v<Held, std::filesystem::path>) {
			auto reader = LocalReader(held);
			ScanWithReader(*reader, m_streams, m_duration);
		}
		else {
			ScanWithReader(held.get(), m_streams, m_duration);
		}
	}, m_origin);
}
