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

#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/subtitle.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/ocr/bitmap.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
	#include <libavutil/avutil.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
}

using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::SideData;
using StormByte::Multimedia::Type;

namespace {
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration> TicksToPts(std::int64_t ticks, StormByte::Multimedia::Property::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks < 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, StormByte::Multimedia::Property::AVRational{1, 1000000000});
		if (ns < 0)
			return std::nullopt;
		return StormByte::Multimedia::Property::Duration{std::chrono::nanoseconds{ns}};
	}

	std::int64_t NsToTicks(const StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration>& value, StormByte::Multimedia::Property::AVRational timeBase) noexcept {
		if (!value.has_value() || timeBase.num <= 0 || timeBase.den <= 0)
			return AV_NOPTS_VALUE;
		return StormByte::Multimedia::Property::AVRational{1, 1000000000}.Rescale(value->Nanoseconds().count(), timeBase);
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Decoder {
	Subtitle::~Subtitle() noexcept = default;

	Subtitle::Subtitle(Subtitle&& other) noexcept = default;

	Subtitle& Subtitle::operator=(Subtitle&& other) noexcept = default;

	Subtitle::Subtitle(StormByte::Multimedia::FFmpeg::AVDecoder decoder, FFmpeg::AVRational timeBase) noexcept
	: m_decoder(std::move(decoder)), m_timeBase(timeBase), m_flushed(false) {}

	bool Subtitle::IsOpen() const noexcept {
		return true;
	}

	bool Subtitle::Send(StormByte::Multimedia::Pipeline::Decoder& owner,
		const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept {
		if (!packet) {
			owner.Fail("empty packet");
			return false;
		}

		StormByte::Multimedia::FFmpeg::AVPacket raw;
		StormByte::Safe::Binary bytes;
		const auto n = packet->Payload().Available();
		const std::uint8_t* data = nullptr;
		if (n > 0) {
			if (!packet->Payload().Extract(n, bytes) || bytes.size() != n) {
				owner.Fail("failed to extract packet payload");
				return false;
			}

			data = reinterpret_cast<const std::uint8_t*>(bytes.data());
		}

		if (!raw.Load(data, static_cast<int>(n), owner.Index(), packet->KeyFrame())) {
			owner.Fail("out of memory copying packet");
			return false;
		}

		const std::int64_t duration = packet->Duration()
			? StormByte::Multimedia::Property::AVRational{1, 1000000000}.Rescale(packet->Duration()->Nanoseconds().count(), m_timeBase)
			: 0;
		raw.Timestamps(NsToTicks(packet->Pts(), m_timeBase), NsToTicks(packet->Dts(), m_timeBase), duration);

		m_packetPts = packet->Pts();
		m_packetDuration = packet->Duration();
		StormByte::Multimedia::FFmpeg::AVSubtitle sub;
		const auto result = m_decoder.DecodeSubtitle(raw, sub);
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::Error) {
			owner.Fail("failed to decode subtitle");
			return false;
		}

		if (result == StormByte::Multimedia::FFmpeg::OperationResult::Success)
			m_pendingSub = std::move(sub);
		return true;
	}

	StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame> Subtitle::Receive(
		StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		if (!m_pendingSub.has_value())
			return {};

		auto sub = std::move(*m_pendingSub);
		m_pendingSub.reset();

		auto text = sub.Text();
		StormByte::Safe::Binary bytes;
		if (!text.empty()) {
			bytes.assign(
				reinterpret_cast<const std::byte*>(text.data()),
				reinterpret_cast<const std::byte*>(text.data()) + text.size());
		}

		else if (auto gray = OCR::GrayFromSubtitle(sub)) {
			const std::size_t bytesN = gray->pixels.size();
			bytes.resize(12 + bytesN);
			auto* p = reinterpret_cast<std::uint8_t*>(bytes.data());
			p[0] = 'O'; p[1] = 'C'; p[2] = 'R'; p[3] = '1';
			const auto put32 = [](std::uint8_t* d, std::int32_t v) {
				d[0] = static_cast<std::uint8_t>(v);
				d[1] = static_cast<std::uint8_t>(v >> 8);
				d[2] = static_cast<std::uint8_t>(v >> 16);
				d[3] = static_cast<std::uint8_t>(v >> 24);
			};
			put32(p + 4, gray->width);
			put32(p + 8, gray->height);
			if (bytesN > 0)
				std::memcpy(p + 12, gray->pixels.data(), bytesN);
		}

		auto pts = TicksToPts(sub.Pts(), StormByte::Multimedia::Property::AVRational{1, AV_TIME_BASE});
		if (!pts)
			pts = m_packetPts;

		StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration> duration;
		if (sub.DisplayDurationMs() > 0)
			duration = StormByte::Multimedia::Property::Duration{std::chrono::milliseconds{sub.DisplayDurationMs()}};
		if (!duration)
			duration = m_packetDuration;
		if (duration && duration->Nanoseconds().count() > 600000000000LL)
			duration.reset();

		auto incoming = StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>::MakePointer<
			StormByte::Multimedia::Pipeline::Frame>(
				owner.Index(),
				Type::Subtitle,
				Producer::Decoder,
				StormByte::Buffer::FIFO{std::move(bytes)},
				std::move(pts),
				std::move(duration),
				std::nullopt,
				StormByte::Safe::Vector<SideData>{},
				std::nullopt,
				0,
				0);
		BindFrame(owner, *incoming, nullptr);

		const bool hasCue = incoming->Payload().Available() > 0;
		if (m_heldSubtitle) {
			if (!m_heldSubtitle->Duration() && m_heldSubtitle->Pts() && incoming->Pts()) {
				const auto delta = incoming->Pts()->Nanoseconds() - m_heldSubtitle->Pts()->Nanoseconds();
				if (delta.count() > 0)
					CloseCue(owner, *m_heldSubtitle, StormByte::Multimedia::Property::Duration{delta});
			}

			auto out = std::move(m_heldSubtitle);
			if (hasCue)
				m_heldSubtitle = std::move(incoming);
			return out;
		}

		if (hasCue && !incoming->Duration()) {
			m_heldSubtitle = std::move(incoming);
			return {};
		}

		if (!hasCue)
			return {};
		return incoming;
	}

	void Subtitle::Flush(StormByte::Multimedia::Pipeline::Decoder&) noexcept {
		m_flushed = true;
	}

	bool Subtitle::Reset(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		if (owner.Failed())
			return false;
		m_decoder.Flush();
		m_pendingSub.reset();
		m_heldSubtitle.reset();
		m_packetPts.reset();
		m_packetDuration.reset();
		m_flushed = false;
		return true;
	}
}
