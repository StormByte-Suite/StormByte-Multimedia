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

#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/pointers.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video leaves (Process and Analytics).
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class CountFrames
	 * @brief Counts dest-look units (Encoder / Remuxer).
	 *
	 * Attach per track or with @c job.Filter. Report keys follow
	 * VMAF: flat @c frames when one track, @c N.frames when several.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC CountFrames: public Analytics {
		public:
			/**
			 * @brief Named counter.
			 * @param log Shared logger. Empty pointer means no log.
			 */
			explicit CountFrames(Safe::Shared<StormByte::Logger::Log> log) noexcept;

			/**
			 * @brief Copy is not allowed.
			 * @param other Source leaf.
			 */
			CountFrames(const CountFrames& other) = delete;

			/**
			 * @brief Move is not allowed.
			 * @param other Source leaf.
			 */
			CountFrames(CountFrames&& other) noexcept = delete;

			/**
			 * @brief Releases counts in the provider module.
			 */
			~CountFrames() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source leaf.
			 * @return This leaf.
			 */
			CountFrames& operator=(const CountFrames& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source leaf.
			 * @return This leaf.
			 */
			CountFrames& operator=(CountFrames&& other) noexcept = delete;

			/**
			 * @brief Media this leaf reports under.
			 * @return Video. Process still accepts Audio and Subtitle.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops per-run counts.
			 */
			void Clean() noexcept override;

			/**
			 * @brief No-op.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Counts one dest-look frame.
			 * @param frame Unit in the tube. Never null.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Logs the per-track totals.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Flat or prefixed frame counts.
			 * @return Ok with frames, or Failed if none arrived.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			Safe::Map<int, std::uint64_t> m_frames;	///< Per-track destination counts with provider-owned nodes.
	};
}

/**
 * @brief Conditional DLL safety requires the Analytics provider contract and
 * compatible C++ ABI. Keep Multimedia, Base and logger providers loaded until
 * this leaf and all shared owners have been destroyed.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::CountFrames);
