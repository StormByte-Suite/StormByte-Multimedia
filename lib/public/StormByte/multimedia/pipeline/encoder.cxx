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

#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/encoder/audio.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/encoder/subtitle.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/encoder/video.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/pumper/through.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/worker/encode.hxx>
#include <StormByte/multimedia/backend/pipeline/encoder.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/rational.h>
	#include <libavutil/samplefmt.h>
}

#include <StormByte/multimedia/ffmpeg/convert.hxx>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using StormByte::Logger::Level;

namespace {
	std::string Ns(const std::optional<Property::Duration>& value) noexcept {
		if (!value)
			return "-";
		return std::format("{}", value->Nanoseconds().count());
	}
}

Encoder::Encoder(std::shared_ptr<StormByte::Logger::Log> log,
	int output_index, const Codec& codec) noexcept
: Step(std::move(log), Producer::Encoder, Kinds{Kind::Frame}, Kinds{Kind::Packet}),
	m_index(output_index), m_codec(&codec),
	m_encoderTag("StormByte-Multimedia " STORMBYTE_MULTIMEDIA_VERSION), m_part(0) {
	switch (codec.Type()) {
		case Type::Video:
			m_backend = std::make_unique<Backend::Pipeline::Detail::Encoder::Video>();
			break;
		case Type::Audio:
			m_backend = std::make_unique<Backend::Pipeline::Detail::Encoder::Audio>();
			break;
		case Type::Subtitle:
			m_backend = std::make_unique<Backend::Pipeline::Detail::Encoder::Subtitle>();
			break;
		default:
			Fail("encoder destination type is not video, audio or subtitle");
			return;
	}

	Mount(std::make_unique<Backend::Pipeline::Detail::Pumper::Through>(Face()),
		std::make_unique<Backend::Pipeline::Detail::Worker::Encode>(*this));
	Launch();
}

Encoder::~Encoder() noexcept = default;

Encoder::operator bool() const noexcept {
	return !Failed() && Ready() && m_backend && m_backend->IsOpen();
}

bool Encoder::Opened() const noexcept {
	return !Failed() && m_backend && m_backend->IsOpen();
}

std::size_t Encoder::InputCeiling() const noexcept {
	const auto* track = Backend::Pipeline::TrackByIn(Plan(), m_index);
	if (!track && Plan() && m_index >= 0
		&& static_cast<std::size_t>(m_index) < Plan()->Tracks().size())
		track = &Plan()->Tracks()[static_cast<std::size_t>(m_index)];
	if (!track)
		return 0;
	const Backend::Pipeline::Ceiling cap{Plan(), Producer::Encoder, *track};
	return cap.Frames() != 0 ? cap.Frames() : cap.Packets();
}

std::optional<int> Encoder::AudioChannels() const noexcept {
	const auto* ctx = (m_backend && m_backend->IsOpen()) ? m_backend->Context() : nullptr;
	if (!ctx || ctx->ch_layout.nb_channels <= 0)
		return std::nullopt;
	return ctx->ch_layout.nb_channels;
}

std::optional<int> Encoder::AudioSampleRate() const noexcept {
	const auto* ctx = (m_backend && m_backend->IsOpen()) ? m_backend->Context() : nullptr;
	if (!ctx || ctx->sample_rate <= 0)
		return std::nullopt;
	return ctx->sample_rate;
}

std::optional<int> Encoder::AudioFrameSize() const noexcept {
	const auto* ctx = (m_backend && m_backend->IsOpen()) ? m_backend->Context() : nullptr;
	if (!ctx || ctx->frame_size <= 0)
		return std::nullopt;
	return ctx->frame_size;
}

std::optional<int> Encoder::AudioSampleFormat() const noexcept {
	const auto* ctx = (m_backend && m_backend->IsOpen()) ? m_backend->Context() : nullptr;
	if (!ctx || ctx->sample_fmt == AV_SAMPLE_FMT_NONE)
		return std::nullopt;
	return static_cast<int>(ctx->sample_fmt);
}

void Encoder::Implementation(std::string name) noexcept {
	if (name.empty())
		m_implementation.reset();
	else
		m_implementation = std::move(name);
}

void Encoder::Preset(std::string name) noexcept {
	if (Failed())
		return;
	if (name.empty()) {
		m_preset.reset();
		return;
	}

	m_preset = std::move(name);
}

void Encoder::Tune(std::string name) noexcept {
	if (Failed())
		return;
	if (m_codec->Type() != Type::Video) {
		Fail("Tune is not valid for this codec");
		return;
	}

	if (name.empty()) {
		m_tune.reset();
		return;
	}

	m_tune = std::move(name);
}

void Encoder::Emit(Packet::PointerType packet) noexcept {
	Step::Emit(std::move(packet));
}

Packet::PointerType Encoder::Wrap(
	enum StormByte::Multimedia::Type type, int index,
	StormByte::Buffer::FIFO payload,
	std::optional<Property::Duration> pts,
	std::optional<Property::Duration> dts,
	std::optional<Property::Duration> duration,
	bool keyFrame,
	std::vector<SideData> attachments,
	std::unique_ptr<Backend::Pipeline::Packet> backend) noexcept {
	if (!m_serial) {
		Fail("encoder packet has no serial");
		return {};
	}

	Log(Level::LowLevel, std::format("out t={} {}:{} pts={} dts={} dur={} key={}",
		index, *m_serial, m_part, Ns(pts), Ns(dts), Ns(duration), keyFrame ? 1 : 0));
	auto packet = Packet::PointerType::MakePointer<Packet>(
		index, type, Producer::Encoder,
		std::move(payload),
		std::move(pts), std::move(dts), std::move(duration),
		keyFrame, std::move(attachments),
		m_codec,
		*m_serial, m_part);
	packet->Bind(std::move(backend));
	return packet;
}

bool Encoder::MuxBindStream(void* avStream) noexcept {
	if (!m_backend || !avStream)
		return false;
	auto* stream = static_cast<::AVStream*>(avStream);
	const auto* ctx = m_backend->Context();
	if (!ctx)
		return false;
	if (avcodec_parameters_from_context(stream->codecpar, ctx) < 0)
		return false;
	auto tb = m_backend->TimeBase();
	if (tb.num <= 0 || tb.den <= 0)
		tb = StormByte::Multimedia::FFmpeg::FromRaw(ctx->time_base);
	if (tb.num <= 0 || tb.den <= 0)
		tb = StormByte::Multimedia::FFmpeg::AVRational{1, 1000};
	stream->time_base = ::AVRational{tb.num, tb.den};
	if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		if (ctx->framerate.num > 0 && ctx->framerate.den > 0) {
			stream->avg_frame_rate = ctx->framerate;
			stream->r_frame_rate = ctx->framerate;
		}

		else if (tb.num > 0 && tb.den > 0) {
			stream->avg_frame_rate = ::AVRational{tb.den, tb.num};
			stream->r_frame_rate = stream->avg_frame_rate;
		}

		int delay = ctx->has_b_frames;
		if (delay <= 0 && (ctx->codec_id == AV_CODEC_ID_HEVC || ctx->codec_id == AV_CODEC_ID_H264))
			delay = 2;
		if (delay > 0)
			stream->codecpar->video_delay = delay;
		if (ctx->sample_aspect_ratio.num > 0 && ctx->sample_aspect_ratio.den > 0) {
			stream->sample_aspect_ratio = ctx->sample_aspect_ratio;
			if (stream->codecpar)
				stream->codecpar->sample_aspect_ratio = ctx->sample_aspect_ratio;
		}
	}

	return true;
}

void* Encoder::FrameHandle(Frame& frame) noexcept {
	return frame.m_backend ? &frame.m_backend->Handle() : nullptr;
}

const void* Encoder::FrameHandle(const Frame& frame) noexcept {
	return frame.m_backend ? &frame.m_backend->Handle() : nullptr;
}

std::string Encoder::Label() const noexcept {
	if (m_implementation && !m_implementation->empty())
		return "Encoder(" + *m_implementation + ")";
	return "Encoder(" + std::string(m_codec->Name()) + ")";
}
