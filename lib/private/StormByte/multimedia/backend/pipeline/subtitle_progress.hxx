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

#pragma once

#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <chrono>
#include <cstdint>
#include <limits>

namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Ordered sparse-track watermark, never passed to a codec or written.
	 *
	 * Empty intervals must travel through the same bounded queues as subtitle
	 * work. A busy OCR lane therefore holds back the reader, while a forced
	 * subtitle lane can advance without waiting for its next actual cue.
	 */
	class SubtitleProgress {
		public:
			explicit SubtitleProgress(std::int64_t boundary) noexcept: m_boundary(boundary) {}
			SubtitleProgress(const SubtitleProgress&) noexcept = default;
			SubtitleProgress(SubtitleProgress&&) noexcept = default;
			virtual ~SubtitleProgress() noexcept = default;
			SubtitleProgress& operator=(const SubtitleProgress&) noexcept = default;
			SubtitleProgress& operator=(SubtitleProgress&&) noexcept = default;

			std::int64_t Boundary() const noexcept { return m_boundary; }

			static constexpr std::int64_t End = std::numeric_limits<std::int64_t>::max();
			static constexpr std::int64_t Interval = 100000000;
			// Retain ordinary source cross-stream skew and video reorder behind the watermark.
			static constexpr std::int64_t SourceLead = 1000000000;

		private:
			std::int64_t m_boundary;
	};

	class SubtitlePacketProgress final: public StormByte::Multimedia::Pipeline::Packet, public SubtitleProgress {
		public:
			SubtitlePacketProgress(int track, StormByte::Multimedia::Pipeline::Producer producer,
				std::int64_t boundary):
				Packet(track, StormByte::Multimedia::Type::Subtitle, producer, {},
					Property::Duration{std::chrono::nanoseconds{boundary}}, std::nullopt, std::nullopt,
					false, {}, nullptr, 0, 0), SubtitleProgress(boundary) {}

			SubtitlePacketProgress(const SubtitlePacketProgress&) = default;
			SubtitlePacketProgress(SubtitlePacketProgress&&) noexcept = default;
			~SubtitlePacketProgress() noexcept override = default;
			SubtitlePacketProgress& operator=(const SubtitlePacketProgress&) = default;
			SubtitlePacketProgress& operator=(SubtitlePacketProgress&&) noexcept = default;

		private:
			Item::PointerType Clone() const override {
				return Item::PointerType::MakePointer<SubtitlePacketProgress>(*this);
			}

			Item::PointerType Move() override {
				return Item::PointerType::MakePointer<SubtitlePacketProgress>(std::move(*this));
			}
	};

	class SubtitleFrameProgress final: public StormByte::Multimedia::Pipeline::Frame, public SubtitleProgress {
		public:
			SubtitleFrameProgress(int track, std::int64_t boundary):
				Frame(track, StormByte::Multimedia::Type::Subtitle, StormByte::Multimedia::Pipeline::Producer::Decoder, {},
					Property::Duration{std::chrono::nanoseconds{boundary}}, std::nullopt, std::nullopt,
					{}, std::nullopt, 0, 0), SubtitleProgress(boundary) {}

			SubtitleFrameProgress(const SubtitleFrameProgress&) = default;
			SubtitleFrameProgress(SubtitleFrameProgress&&) noexcept = default;
			~SubtitleFrameProgress() noexcept override = default;
			SubtitleFrameProgress& operator=(const SubtitleFrameProgress&) = default;
			SubtitleFrameProgress& operator=(SubtitleFrameProgress&&) noexcept = default;

		private:
			Item::PointerType Clone() const override {
				return Item::PointerType::MakePointer<SubtitleFrameProgress>(*this);
			}

			Item::PointerType Move() override {
				return Item::PointerType::MakePointer<SubtitleFrameProgress>(std::move(*this));
			}
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Backend::Pipeline::SubtitlePacketProgress);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Backend::Pipeline::SubtitleFrameProgress);
