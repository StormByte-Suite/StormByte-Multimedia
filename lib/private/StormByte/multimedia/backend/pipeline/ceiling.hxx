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

#include <StormByte/size.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/pipeline/resource_usage.hxx>

#include <cstddef>
#include <memory>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Private tube machinery.
 */
namespace StormByte::Multimedia::Backend::Pipeline {
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
			 * @brief Resolve a requested level against available process CPUs.
			 * @param usage Resource policy.
			 * @param available Process-available CPU count; zero is treated as one.
			 * @return Full count, half rounded down (minimum one), or at most two.
			 */
			static unsigned ResourceThreads(Multimedia::Pipeline::ResourceUsage usage,
				unsigned available) noexcept;

			/**
			 * @brief Divide a policy budget across concurrently active decode/encode stages.
			 * @param usage Resource policy.
			 * @param available Process-available CPU count.
			 * @param stages Number of concurrently active codec stages.
			 * @return Zero for Full (keep historical FFmpeg defaults), otherwise at least one.
			 */
			static unsigned StageThreads(Multimedia::Pipeline::ResourceUsage usage,
				unsigned available, std::size_t stages) noexcept;

			/**
			 * @brief Resolve the per-stage thread target from a complete plan.
			 * @param plan Closed job intention.
			 * @param available Process-available CPU count.
			 * @return Zero for Full; otherwise the budget divided across encode lanes.
			 */
			static unsigned StageThreads(const Multimedia::Pipeline::Plan& plan,
				unsigned available) noexcept;

			/**
			 * @brief Translate a thread target to SVT-AV1's parallelism level.
			 * @param threads Per-stage best-effort thread target.
			 * @return SVT-AV1 parallelism level in the range [1, 6].
			 *
			 * SVT-AV1 levels are not thread counts or hard CPU limits. The
			 * thresholds mirror SVT-AV1 4.2's default level selection for a
			 * given processor count; they select a comparable parallelism and
			 * buffering level only.
			 */
			static unsigned SvtParallelismLevel(unsigned threads) noexcept;

			/**
			 * @brief Scale a video-frame queue while retaining its safe minimum.
			 * @param usage Resource policy.
			 * @param fullFrames Existing full-policy capacity.
			 * @return Full capacity, half for Medium, or one quarter for Low; never below two.
			 */
			static std::size_t ResourceFrames(Multimedia::Pipeline::ResourceUsage usage,
				std::size_t fullFrames) noexcept;

			/**
			 * @brief Finds a track in the plan's published track list.
			 * @param plan Shared plan. May be empty.
			 * @param origin Origin stream index.
			 * @return Borrowed track, or nullptr. The plan must outlive its use.
			 */
			static const Multimedia::Pipeline::Track* TrackByIn(
				const StormByte::Safe::Shared<const Multimedia::Pipeline::Plan>& plan, int origin) noexcept;

			/**
			 * @brief Raw-frame hopper cap.
			 * @return Items, or `0` if this instance has no frame hopper.
			 */
			StormByte::Size Frames() const noexcept;

			/**
			 * @brief Packet hopper cap.
			 * @return Items, or `0` if this instance has no packet hopper.
			 */
			StormByte::Size Packets() const noexcept;

			/**
			 * @brief Demux park cap.
			 * @return Packets, or `0` if this instance is not a Demuxer park.
			 */
			StormByte::Size Park() const noexcept;

		private:
			StormByte::Size m_frames;	///< Frame hopper, or `0`
			StormByte::Size m_packets;	///< Packet hopper, or `0`
			StormByte::Size m_park;		///< Demux park, or `0`
	};

	/**
	 * @brief Forwards origin lookup to the plan's friend-owned capacity helper.
	 * @param plan Shared plan. May be empty.
	 * @param origin Origin stream index.
	 * @return Borrowed track, or nullptr. The plan must outlive its use.
	 */
	inline const Multimedia::Pipeline::Track* TrackByIn(
		const StormByte::Safe::Shared<const Multimedia::Pipeline::Plan>& plan, int origin) noexcept {
		return Ceiling::TrackByIn(plan, origin);
	}
}
