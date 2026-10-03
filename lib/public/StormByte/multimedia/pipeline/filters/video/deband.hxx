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

#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <memory>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Deband
	 * @brief Spatial deband + deterministic grain. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Deband>(log).
	 *
	 * @par What it is for
	 * Contouring on gradients (sky, walls, 8-bit or
	 * crushed 10-bit masters). It is not a denoise: grain
	 * here is only dither so the flatten does not re-band
	 * in the next encode. Run **after** denoise, **before**
	 * @ref Cas and @ref Scale.
	 *
	 * @par Do not stack
	 * Do not run two Deband leaves. Do not use it as a
	 * substitute for @ref Hqdn3d / @ref Bm3d. Heavy grain
	 * on a clean master just adds noise.
	 *
	 * @par Algorithm
	 * Processes each frame independently without Hold. The configured
	 * range and threshold control smoothing of nearby colour differences;
	 * the grain setting adds texture to the result.
	 *
	 * @par Mutation
	 * Writes a new @ref StormByte::Multimedia::FFmpeg::AVFrame
	 * and @ref Filter::FFmpeg::Save.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Deband: public Filter::Process {
		public:
			/**
			 * @brief Spatial deband.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param range Neighbour distance in luma pixels. 0 uses 16.
			 * @param threshold Max |delta| in 8-bit units. 0 uses 4.
			 *        10/12-bit planes scale this to their peak.
			 * @param grain Dither amplitude in 8-bit units. 0 uses 2.
			 */
			Deband(std::shared_ptr<StormByte::Logger::Log> log,
				unsigned range = 0, unsigned threshold = 0,
				unsigned grain = 0) noexcept;

			Deband(const Deband& other) = delete;
			Deband(Deband&& other) noexcept = delete;
			~Deband() noexcept override = default;
			Deband& operator=(const Deband& other) = delete;
			Deband& operator=(Deband&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Nothing to drop between runs.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Nothing to acquire beyond construction.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Debands the current video unit and Save.
			 * @param frame Video unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			unsigned m_range;		///< Neighbour distance
			unsigned m_threshold;	///< 8-bit |delta| cap
			unsigned m_grain;		///< 8-bit dither amplitude
	};
}
