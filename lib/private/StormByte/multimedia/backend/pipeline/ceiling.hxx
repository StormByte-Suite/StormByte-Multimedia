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

#pragma once

#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>

#include <cstddef>
#include <memory>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Private tube machinery.
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Track of @p plan whose @ref Multimedia::Pipeline::Track::In equals @p origin.
	 * @param plan Shared plan. May be empty.
	 * @param origin Origin stream index.
	 * @return Track, or `nullptr`.
	 */
	inline const Multimedia::Pipeline::Track* TrackByIn(
		const StormByte::Safe::Shared<const Multimedia::Pipeline::Plan>& plan, int origin) noexcept {
		if (!plan)
			return nullptr;
		for (const auto& held : plan->Tracks()) {
			if (held && held->In() == origin)
				return held.get();
		}
		return nullptr;
	}

	/**
	 * @class Ceiling
	 * @brief Hopper caps for one Step × one Plan track.
	 *
	 * Constructed at bind time. @ref Frames, @ref Packets and
	 * @ref Park are fixed after the constructor. `0` means that
	 * hopper does not exist on this instance; it is not an
	 * unlimited cap.
	 *
	 * @p plan must be non-empty and contain at least one track.
	 * An empty plan is a contract violation: the constructor
	 * terminates the process.
	 *
	 * Weight is by @ref Multimedia::Pipeline::Producer and
	 * @ref Multimedia::Pipeline::Track::Type (encode vs remux
	 * via a non-null destination codec).
	 *
	 * Not a public API. Hidden with the rest of the backend.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class Ceiling {
		public:
			/**
			 * @brief Caps for @p producer on @p track of @p plan.
			 * @param plan Shared job intention. Must not be empty.
			 * @param producer Step that owns the hopper.
			 * @param track Plan track bound to that hopper.
			 *
			 * Terminates if @p plan is empty or has no tracks.
			 */
			Ceiling(StormByte::Safe::Shared<const Multimedia::Pipeline::Plan> plan,
				Multimedia::Pipeline::Producer producer,
				const Multimedia::Pipeline::Track& track) noexcept;

			/**
			 * @brief Copy.
			 * @param other Source.
			 */
			Ceiling(const Ceiling& other) noexcept = default;

			/**
			 * @brief Move.
			 * @param other Source.
			 */
			Ceiling(Ceiling&& other) noexcept = default;

			/**
			 * @brief Copy assign.
			 * @param other Source.
			 * @return This.
			 */
			Ceiling& operator=(const Ceiling& other) noexcept = default;

			/**
			 * @brief Move assign.
			 * @param other Source.
			 * @return This.
			 */
			Ceiling& operator=(Ceiling&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~Ceiling() noexcept = default;

			/**
			 * @brief Hardware thread count, at least `1`.
			 * @return Cached `std::thread::hardware_concurrency()`.
			 */
			static unsigned MaxThreads() noexcept;

			/**
			 * @brief Raw-frame hopper cap.
			 * @return Items, or `0` if this instance has no frame hopper.
			 */
			std::size_t Frames() const noexcept;

			/**
			 * @brief Packet hopper cap.
			 * @return Items, or `0` if this instance has no packet hopper.
			 */
			std::size_t Packets() const noexcept;

			/**
			 * @brief Demux park cap.
			 * @return Packets, or `0` if this instance is not a Demuxer park.
			 */
			std::size_t Park() const noexcept;

		private:
			std::size_t m_frames;	///< Frame hopper, or `0`
			std::size_t m_packets;	///< Packet hopper, or `0`
			std::size_t m_park;		///< Demux park, or `0`
	};
}
