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

#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/decoder/audio.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <memory>
#include <vector>

extern "C" {
	#include <libavutil/avutil.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
}

using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Type;

namespace {
	std::optional<StormByte::Multimedia::Property::Duration> TicksToPts(std::int64_t ticks, StormByte::Multimedia::Property::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks < 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, StormByte::Multimedia::Property::AVRational{1, 1000000000});
		if (ns < 0)
			return std::nullopt;
		return StormByte::Multimedia::Property::Duration{std::chrono::nanoseconds{ns}};
	}

	std::optional<StormByte::Multimedia::Property::Duration> TicksToDuration(std::int64_t ticks, StormByte::Multimedia::Property::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks <= 0 || timeBase.num <= 0 || timeBase.den <= 0)
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, StormByte::Multimedia::Property::AVRational{1, 1000000000});
		if (ns <= 0)
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
	Audio::~Audio() noexcept = default;

	Audio::Audio(Audio&& other) noexcept = default;

	Audio& Audio::operator=(Audio&& other) noexcept = default;

	Audio::Audio(StormByte::Multimedia::FFmpeg::AVDecoder decoder, FFmpeg::AVRational timeBase,
		StormByte::Safe::Optional<StormByte::Multimedia::Property::Audio> audio) noexcept
	: m_decoder(std::move(decoder)), m_audio(std::move(audio)), m_timeBase(timeBase), m_flushed(false) {}

	bool Audio::IsOpen() const noexcept {
		return true;
	}

	bool Audio::Send(StormByte::Multimedia::Pipeline::Decoder& owner,
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

		const auto result = m_decoder.SendPacket(raw);
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::Error) {
			owner.Fail("failed to send packet");
			return false;
		}

		if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain)
			return false;
		return true;
	}

	StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame> Audio::Receive(
		StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		auto holder = StormByte::Safe::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Frame>();
		const auto result = m_decoder.ReceiveFrame(holder->Handle());
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain
			|| result == StormByte::Multimedia::FFmpeg::OperationResult::EndOfFile)
			return {};
		if (result != StormByte::Multimedia::FFmpeg::OperationResult::Success) {
			owner.Fail("failed to receive frame");
			return {};
		}

		auto frame = StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>::MakePointer<
			StormByte::Multimedia::Pipeline::Frame>(
				owner.Index(),
				Type::Audio,
				Producer::Decoder,
				StormByte::Buffer::FIFO{},
				TicksToPts(holder->Handle().Pts(), m_timeBase),
				TicksToDuration(holder->Handle().DurationTicks(), m_timeBase),
				std::nullopt,
				StormByte::Safe::Vector<StormByte::Multimedia::Pipeline::SideData>{},
				m_audio,
				0,
				0);
		auto* backend = holder.get();
		BindFrame(owner, *frame, std::move(holder));
		backend->BindProperties(*frame);
		return frame;
	}

	void Audio::Flush(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		if (owner.Failed() || m_flushed)
			return;
		for (;;) {
			const auto sent = m_decoder.SetEof();
			if (sent == StormByte::Multimedia::FFmpeg::OperationResult::Success
				|| sent == StormByte::Multimedia::FFmpeg::OperationResult::EndOfFile)
				break;
			if (sent == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain)
				break;
			owner.Fail("failed to signal decoder EOF");
			return;
		}

		m_flushed = true;
	}

	bool Audio::Reset(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept {
		if (owner.Failed())
			return false;
		m_decoder.Flush();
		m_scratch.Unref();
		m_flushed = false;
		return true;
	}
}
