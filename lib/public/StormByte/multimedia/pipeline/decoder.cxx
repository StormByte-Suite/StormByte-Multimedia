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
 * A written StormByte commercial agreement may license this original source
 * on terms other than the LGPL, including specific use, distribution or
 * linking arrangements such as static linking, as stated in that agreement.
 * It does not grant rights to dependencies or waive their license conditions.
 * Enabling WITH_GPL or WITH_NONFREE may include components with separate
 * obligations for modification, linking (static or dynamic), redistribution
 * or works that incorporate them. The person modifying, linking, packaging or
 * distributing the resulting work is responsible for determining and meeting
 * all applicable requirements, including any needed patent permissions.
 * A StormByte commercial agreement does not provide those rights for GPL or
 * nonfree components.
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
#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/backend/pipeline/decoder.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/audio.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/subtitle.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/video.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/pumper/through.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/worker/decode.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/pointers.hxx>

#include <format>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/rational.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using StormByte::Logger::Level;

Decoder::Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	int track, DecoderFlags flags) noexcept
:	Step(std::move(log), Producer::Decoder, Kinds{Kind::Packet}, Kinds{Kind::Frame}),
	m_index(track), m_flags(flags), m_part(0), m_look(false) {
	Mount(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Pumper::Through>(Face()),
		StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Worker::Decode>(*this));
	Launch();
}

Decoder::Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	int track, EncodeLook) noexcept
:	Step(std::move(log), Producer::Decoder, Kinds{Kind::Packet}, Kinds{Kind::Frame}),
	m_index(track), m_flags{}, m_part(0), m_look(true), m_lookStamp(Producer::Encoder) {
	Mount(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Pumper::Through>(Face()),
		StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Worker::Decode>(*this));
	Launch();
}

Decoder::Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	int track, RemuxLook) noexcept
:	Step(std::move(log), Producer::Decoder, Kinds{Kind::Packet}, Kinds{Kind::Frame}),
	m_index(track), m_flags{}, m_part(0), m_look(true), m_lookStamp(Producer::Remuxer) {
	Mount(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Pumper::Through>(Face()),
		StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Worker::Decode>(*this));
	Launch();
}

Decoder::Decoder(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	int track, SourceLook) noexcept
:	Step(std::move(log), Producer::Decoder, Kinds{Kind::Packet}, Kinds{Kind::Frame}),
	m_index(track), m_flags{}, m_part(0), m_look(true) {
	Mount(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Pumper::Through>(Face()),
		StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Worker::Decode>(*this));
	Launch();
}

Decoder::~Decoder() noexcept = default;

Decoder::operator bool() const noexcept {
	return !Failed() && Ready() && m_backend && m_backend->IsOpen();
}

std::size_t Decoder::InputCeiling() const noexcept {
	const StormByte::Safe::Shared<const StormByte::Multimedia::Pipeline::Plan> plan = Plan();
	const auto* track = Backend::Pipeline::TrackByIn(plan, m_index);
	if (!track)
		return 0;
	const Backend::Pipeline::Ceiling cap{plan, Producer::Decoder, *track};
	return cap.Packets() != 0 ? cap.Packets() : cap.Frames();
}

void Decoder::Implementation(StormByte::Safe::String name) noexcept {
	if (name.empty())
		m_implementation.reset();
	else
		m_implementation = std::move(name);
}

void Decoder::Attach(Frame& frame, StormByte::Safe::Unique<Backend::Pipeline::Frame> backend) noexcept {
	frame.m_language = m_language;
	frame.m_title = m_title;
	frame.Bind(std::move(backend));
}

void Decoder::CloseCue(Frame& frame, Property::Duration duration) noexcept {
	frame.m_duration = std::move(duration);
}

void Decoder::StampLineage(Frame& frame) noexcept {
	frame.m_serial = m_serial;
	frame.m_part = m_part++;
	frame.m_dts = m_inDts;
}

void Decoder::BindAnalytics(Filters& filters) noexcept {
	m_analytics = &filters;
}

void Decoder::StampLook(Frame& frame) noexcept {
	if (m_lookStamp)
		frame.m_producer = *m_lookStamp;
	if (!m_lookStamp || !m_analytics)
		return;
	if (const auto& pts = frame.Pts(); pts)
		m_analytics->NoteAnalytics(pts->Nanoseconds().count());
}

void Decoder::Bind(StormByte::Safe::Unique<Backend::Pipeline::Decoder> backend) noexcept {
	m_backend = std::move(backend);
	m_capabilities = Features{};
}

StormByte::Safe::Unique<Backend::Pipeline::Decoder> Decoder::OpenOrigin() noexcept {
	if (!m_origin)
		return {};
	return m_origin->OpenDecoder(*this);
}

void Decoder::Stamp(StormByte::Safe::Optional<StormByte::Safe::String> language,
	StormByte::Safe::Optional<StormByte::Safe::String> title) noexcept {
	if (!language || language->empty())
		m_language.reset();
	else
		m_language = std::move(language);
	if (!title || title->empty())
		m_title.reset();
	else
		m_title = std::move(title);
}

void Decoder::AttachOrigin(Demuxer& demuxer) noexcept {
	m_origin = &demuxer;
	Wake();
}

void Decoder::MeasureSourceClosed() noexcept {
	if (m_look)
		return;
	m_measureClosed.store(true, StormByte::Safe::MemoryOrder::Release);
	Wake();
}

void Decoder::DrainMeasure() noexcept {
	if (m_look)
		return;
	if (!m_measureClosed.load(StormByte::Safe::MemoryOrder::Acquire))
		return;
	bool expected = false;
	if (!m_measureDrained.compare_exchange_strong(expected, true,
			StormByte::Safe::MemoryOrder::AcqRel, StormByte::Safe::MemoryOrder::Acquire))
		return;
	if (Failed())
		return;
	if (m_backend)
		m_backend->Flush(*this);
	if (!ResetAfterMeasure())
		return;
	if (m_origin && m_origin->m_filters)
		m_origin->m_filters->OnMeasureDrained(m_index);
}

bool Decoder::WakeNow() const noexcept {
	if (m_look)
		return false;
	return m_measureClosed.load(StormByte::Safe::MemoryOrder::Acquire)
		&& !m_measureDrained.load(StormByte::Safe::MemoryOrder::Acquire)
		&& !pipe().Ready();
}

void Decoder::AfterWait() noexcept {
	if (m_look)
		return;
	if (m_measureClosed.load(StormByte::Safe::MemoryOrder::Acquire) && !pipe().Ready())
		DrainMeasure();
}

bool Decoder::ResetAfterMeasure() noexcept {
	if (Failed())
		return false;
	if (m_look)
		return true;
	if (!m_origin) {
		Fail("decoder has no origin");
		return false;
	}
	DumpWork();
	m_backend.reset();
	auto opened = OpenOrigin();
	if (!opened) {
		Fail("reopen after measure failed");
		return false;
	}
	Bind(std::move(opened));
	m_serial.reset();
	m_part = 0;
	m_inDts.reset();
	Log(Level::Debug, std::format("reopen after measure t={}", m_index));
	return true;
}

bool Decoder::OpenLook(const Packet& packet) noexcept {
	if (!packet.m_backend || !packet.m_backend->Parameters()) {
		Fail("look packet has no codec parameters");
		return false;
	}

	const auto& params = *packet.m_backend->Parameters();
	const auto* codec = avcodec_find_decoder(static_cast<AVCodecID>(params.CodecId()));
	if (!codec) {
		Fail("look decoder not found");
		return false;
	}

	auto opened = FFmpeg::AVDecoder::Open(
		const_cast<AVCodec*>(codec), params, m_index);
	if (!opened) {
		Fail(opened.error()->what());
		return false;
	}

	const StormByte::Multimedia::FFmpeg::AVRational timeBase{1, 1000000000};
	if (packet.Type() == Type::Video) {
		StormByte::Safe::Optional<Property::Video> video;
		if (const auto& plan = Plan(); plan && static_cast<bool>(*plan)) {
			for (const auto& stream : plan->Snapshot().Streams()) {
				if (stream.Index() == m_index) {
					video = stream.Video();
					break;
				}
			}
		}
		Bind(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Decoder::Video>(
			std::move(*opened), timeBase, std::move(video)));
	}

	else if (packet.Type() == Type::Audio) {
		Bind(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Decoder::Audio>(
			std::move(*opened), timeBase, std::nullopt));
	}

	else {
		Bind(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Decoder::Subtitle>(
			std::move(*opened), timeBase));
	}

	Log(Level::Debug, std::format("look open t={}", m_index));
	return static_cast<bool>(m_backend);
}

StormByte::Safe::String Decoder::Label() const noexcept {
	if (m_look) {
		if (m_lookStamp == Producer::Encoder)
			return StormByte::Safe::String("Decoder(look encode t=" + std::to_string(m_index) + ")");
		if (m_lookStamp == Producer::Remuxer)
			return StormByte::Safe::String("Decoder(look remux t=" + std::to_string(m_index) + ")");
		return StormByte::Safe::String("Decoder(look src t=" + std::to_string(m_index) + ")");
	}

	if (m_implementation && !m_implementation->empty())
		return StormByte::Safe::String(std::format("Decoder({})", std::string_view{*m_implementation}));
	return StormByte::Safe::String("Decoder(t=" + std::to_string(m_index) + ")");
}
