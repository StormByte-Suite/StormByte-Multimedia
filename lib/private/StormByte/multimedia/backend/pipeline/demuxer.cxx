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

#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVDecoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVStream.hxx>
#include <StormByte/multimedia/ffmpeg/property.hxx>
#include <StormByte/multimedia/backend/file_avio.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/audio.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/subtitle.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/video.hxx>
#include <StormByte/multimedia/backend/pipeline/demuxer.hxx>
#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/video.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
}

using namespace StormByte::Multimedia;
namespace FFmpeg = StormByte::Multimedia::FFmpeg;

namespace {
	std::optional<Property::Duration> TicksToPts(std::int64_t ticks, Property::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks < 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, Property::AVRational{1, 1000000000});
		if (ns < 0)
			return std::nullopt;
		return Property::Duration{std::chrono::nanoseconds{ns}};
	}

	std::optional<Property::Duration> TicksToDuration(std::int64_t ticks, Property::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks <= 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, Property::AVRational{1, 1000000000});
		if (ns <= 0)
			return std::nullopt;
		return Property::Duration{std::chrono::nanoseconds{ns}};
	}

	enum Type KindOf(FFmpeg::AVFormatContext& ctx, int index) noexcept {
		for (const auto& stream : ctx.Streams()) {
			if (stream.Index() != index)
				continue;
			const auto mapped = FFmpeg::MapProperties(stream);
			if (std::holds_alternative<Property::Video>(mapped))
				return Multimedia::Type::Video;
			if (std::holds_alternative<Property::Audio>(mapped))
				return Multimedia::Type::Audio;
			return Multimedia::Type::Subtitle;
		}

		return Multimedia::Type::Unknown;
	}
}

class StormByte::Multimedia::Backend::Pipeline::Demuxer::Context {
	public:
		std::optional<StormByte::Multimedia::Backend::FileAvio> avio;
		std::optional<FFmpeg::AVFormatContext> format;
		FFmpeg::AVPacket scratch;
		std::unordered_map<int, FFmpeg::AVRational> timeBase;
		std::unordered_set<int> wanted;
		StormByte::ByteSize measureReadAhead{0};
		StormByte::ByteSize measureMaxMemory{0};
		bool measureReaderPolicySaved{false};
};

StormByte::Multimedia::Backend::Pipeline::Demuxer::Demuxer() noexcept
: m_ctx(std::make_unique<Context>()) {}

StormByte::Multimedia::Backend::Pipeline::Demuxer::~Demuxer() noexcept {
	Close();
}

bool StormByte::Multimedia::Backend::Pipeline::Demuxer::IsOpen() const noexcept {
	return m_ctx && m_ctx->format.has_value();
}

void StormByte::Multimedia::Backend::Pipeline::Demuxer::BeginMeasure(
	StormByte::Buffer::IO::BufferedLocationReader& reader) noexcept {
	if (!m_ctx || m_ctx->measureReaderPolicySaved
			|| reader.Location() != StormByte::Buffer::IO::Location::Local)
		return;
	m_ctx->measureReadAhead = reader.ReadAhead();
	m_ctx->measureMaxMemory = reader.MaxMemory();
	m_ctx->measureReaderPolicySaved = true;
	reader.ReadAhead(StormByte::ByteSize{0});
	reader.MaxMemory(StormByte::ByteSize{0});
}

void StormByte::Multimedia::Backend::Pipeline::Demuxer::RestoreReaderPolicy(
	StormByte::Buffer::IO::BufferedLocationReader& reader) noexcept {
	if (!m_ctx || !m_ctx->measureReaderPolicySaved)
		return;
	reader.MaxMemory(m_ctx->measureMaxMemory);
	reader.ReadAhead(m_ctx->measureReadAhead);
	m_ctx->measureReaderPolicySaved = false;
}

bool StormByte::Multimedia::Backend::Pipeline::Demuxer::Open(
	StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept {
	if (!m_ctx)
		m_ctx = std::make_unique<Context>();
	if (m_ctx->format) {
		owner.Fail("demuxer is already open");
		return false;
	}

	auto& reader = owner.Origin();
	if (!reader.IsOpen() && !reader.Open()) {
		owner.Fail("reader open failed");
		return false;
	}

	m_ctx->avio.emplace(reader);
	if (!m_ctx->avio->Arm() || !m_ctx->avio->Context()) {
		owner.Fail("file AVIO alloc failed");
		m_ctx->avio.reset();
		return false;
	}

	::AVFormatContext* raw = avformat_alloc_context();
	if (!raw) {
		owner.Fail("avformat_alloc_context failed");
		m_ctx->avio.reset();
		return false;
	}

	raw->pb = m_ctx->avio->Context();
	raw->flags |= AVFMT_FLAG_CUSTOM_IO;

	int ret = avformat_open_input(&raw, nullptr, nullptr, nullptr);
	if (ret < 0) {
		if (raw) {
			raw->pb = nullptr;
			avformat_free_context(raw);
		}
		owner.Fail("avformat_open_input failed");
		m_ctx->avio.reset();
		return false;
	}

	ret = avformat_find_stream_info(raw, nullptr);
	if (ret < 0) {
		raw->pb = nullptr;
		avformat_close_input(&raw);
		owner.Fail("avformat_find_stream_info failed");
		m_ctx->avio.reset();
		return false;
	}

	bool restored = static_cast<bool>(owner.Plan());
	if (restored) {
		const auto& parameters = owner.Plan()->Snapshot().m_codecParameters;
		for (unsigned index = 0; index < raw->nb_streams; ++index) {
			auto* stream = raw->streams[index];
			if (!stream->codecpar || stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
				continue;
			const auto found = parameters.find(stream->index);
			if (found == parameters.end() || found->second.CodecId() != stream->codecpar->codec_id
				|| !found->second.Export(stream->codecpar)) {
				restored = false;
				break;
			}
		}
	}
	m_ctx->format = FFmpeg::AVFormatContext::WrapBorrowed(raw, !restored);

	m_ctx->timeBase.clear();
	m_ctx->wanted.clear();
	if (const auto& plan = owner.Plan(); plan) {
		for (const auto& slot : plan->Tracks()) {
			if (slot)
				m_ctx->wanted.insert(slot->In());
		}
	}

	for (const auto& stream : m_ctx->format->Streams())
		m_ctx->timeBase[stream.Index()] = stream.TimeBase();

	if (*m_ctx->format)
		m_ctx->format->DiscardUnwanted(m_ctx->wanted);

	return true;
}

StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>
StormByte::Multimedia::Backend::Pipeline::Demuxer::Read(
	StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept {
	if (!m_ctx || !m_ctx->format) {
		owner.Fail("demuxer is not open");
		return {};
	}
	if (owner.Measuring())
		BeginMeasure(owner.Origin());

	for (;;) {
		const auto result = m_ctx->format->ReadPacket(m_ctx->scratch);
		if (result == FFmpeg::OperationResult::EndOfFile)
			return {};

		if (result == FFmpeg::OperationResult::TryAgain)
			continue;
		if (result != FFmpeg::OperationResult::Success) {
			owner.Fail("failed to read packet");
			return {};
		}

		const int index = m_ctx->scratch.StreamIndex();
		if (!m_ctx->wanted.contains(index)) {
			m_ctx->scratch.Unref();
			continue;
		}

		if (owner.Measuring()) {
			const auto& tracks = owner.m_measureTracks;
			if (std::find(tracks.begin(), tracks.end(), index) == tracks.end()) {
				m_ctx->scratch.Unref();
				continue;
			}
		}

		FFmpeg::AVRational tb{0, 1};
		if (const auto found = m_ctx->timeBase.find(index); found != m_ctx->timeBase.end())
			tb = found->second;

		StormByte::BinaryData bytes;
		const auto* data = m_ctx->scratch.Data();
		const int size = m_ctx->scratch.Size();
		if (data && size > 0) {
			const auto* rawb = reinterpret_cast<const std::byte*>(data);
			bytes.assign(rawb, rawb + size);
		}

		auto holder = std::make_unique<StormByte::Multimedia::Backend::Pipeline::Packet>();
		for (const auto& stream : m_ctx->format->Streams()) {
			if (stream.Index() != index)
				continue;
			holder->Parameters(stream.CodecParameters());
			break;
		}

		auto packet = owner.Wrap(
			index,
			KindOf(*m_ctx->format, index),
			StormByte::Buffer::FIFO{std::move(bytes)},
			TicksToPts(m_ctx->scratch.Pts(), tb),
			TicksToPts(m_ctx->scratch.Dts(), tb),
			TicksToDuration(m_ctx->scratch.Duration(), tb),
			(m_ctx->scratch.Flags() & AV_PKT_FLAG_KEY) != 0,
			std::move(holder)
		);
		m_ctx->scratch.Unref();
		if (!packet)
			continue;
		return packet;
	}
}

bool StormByte::Multimedia::Backend::Pipeline::Demuxer::Rewind(
	StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept {
	if (!m_ctx || !m_ctx->format) {
		owner.Fail("demuxer is not open");
		return false;
	}
	if (m_ctx->format->SeekStart() != FFmpeg::OperationResult::Success) {
		owner.Fail("failed to rewind origin");
		return false;
	}
	return true;
}

std::unique_ptr<StormByte::Multimedia::Backend::Pipeline::Decoder>
StormByte::Multimedia::Backend::Pipeline::Demuxer::OpenDecoder(
	StormByte::Multimedia::Pipeline::Demuxer&,
	StormByte::Multimedia::Pipeline::Decoder& decoder) noexcept {
	if (!m_ctx || !m_ctx->format) {
		decoder.Fail("demuxer is not open");
		return {};
	}

	std::optional<FFmpeg::AVCodecParameters> params;
	std::optional<Stream::Properties> mapped;
	FFmpeg::AVRational timeBase{0, 1};
	bool found = false;
	for (const auto& stream : m_ctx->format->Streams()) {
		if (stream.Index() != decoder.Index())
			continue;
		params = stream.CodecParameters();
		mapped = FFmpeg::MapProperties(stream);
		timeBase = stream.TimeBase();
		found = true;
		break;
	}

	if (!found || !params.has_value()) {
		decoder.Fail("stream index out of range");
		return {};
	}

	const ::AVCodec* codec = avcodec_find_decoder(static_cast<::AVCodecID>(params->CodecId()));
	auto opened = FFmpeg::AVDecoder::Open(
		const_cast<::AVCodec*>(codec), *params, *m_ctx->format, decoder.Index());
	if (!opened.has_value()) {
		decoder.Fail(opened.error()->what());
		return {};
	}

	if (mapped.has_value() && std::holds_alternative<Property::Video>(*mapped)) {
		return std::make_unique<Detail::Decoder::Video>(
			std::move(opened.value()), timeBase,
			std::get<Property::Video>(std::move(*mapped)));
	}

	if (mapped.has_value() && std::holds_alternative<Property::Audio>(*mapped)) {
		return std::make_unique<Detail::Decoder::Audio>(
			std::move(opened.value()), timeBase,
			std::get<Property::Audio>(std::move(*mapped)));
	}

	return std::make_unique<Detail::Decoder::Subtitle>(
		std::move(opened.value()), timeBase);
}

bool StormByte::Multimedia::Backend::Pipeline::Demuxer::CloneStream(
	int index, ::AVCodecParameters*& params, FFmpeg::AVRational& timeBase) noexcept {
	params = nullptr;
	timeBase = FFmpeg::AVRational{0, 1};
	if (!m_ctx || !m_ctx->format)
		return false;
	for (const auto& stream : m_ctx->format->Streams()) {
		if (stream.Index() != index)
			continue;
		auto wrapped = stream.CodecParameters();
		if (!wrapped)
			return false;
		params = avcodec_parameters_alloc();
		if (!params)
			return false;
		if (!wrapped.Export(params)) {
			avcodec_parameters_free(&params);
			return false;
		}

		timeBase = stream.TimeBase();
		return true;
	}

	return false;
}

void StormByte::Multimedia::Backend::Pipeline::Demuxer::Close() noexcept {
	if (!m_ctx)
		return;
	m_ctx->format.reset();
	m_ctx->avio.reset();
	m_ctx->timeBase.clear();
	m_ctx->wanted.clear();
}
