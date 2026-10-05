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
#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <cstdint>
#include <string>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Interpolate
	 * @brief Synthesize in-between frames via `minterpolate` (MCI). Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Interpolate>(log, 60).
	 *
	 * @par What it is for
	 * Smooth 24/30 → 50/60 when fluidity matters more than
	 * a 1:1 temporal match. It **invents** motion. That is
	 * not @ref Fps (drop/dup, no new pictures).
	 *
	 * @par When not to use it
	 * Film cadence you want to keep, credits, heavy grain,
	 * sports with occlusions. Do not stack with @ref Fps.
	 * Hardware frames Fail. Slow; artefacts on text.
	 *
	 * @par Algorithm
	 * @c minterpolate=fps=&lt;num&gt;/&lt;den&gt;:mi_mode=mci:
	 * @c mc_mode=aobmc:me_mode=bidir:vsbmc=1.
	 * num = 0 Fails. EAGAIN while the estimator fills;
	 * @ref Eof flushes.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Fps
	 * @par Conditional DLL safety
	 * Requires a compatible C++ ABI and all providers to remain loaded while
	 * objects, handles or callbacks exist. Create the leaf on Base's heap through
	 * a Safe owner. Private storage is created, used and destroyed out of line
	 * by the provider; the private Chain string never transfers ownership to callers.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Interpolate: public Filter::Process {
		public:
			/**
			 * @brief Motion-compensated rate @p num / @p den.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param num Rate numerator (e.g. 60).
			 * @param den Rate denominator. 0 is treated as 1.
			 */
			Interpolate(Safe::Shared<StormByte::Logger::Log> log,
				std::uint32_t num, std::uint32_t den = 1) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Leaf that cannot be copied.
			 */
			Interpolate(const Interpolate& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Leaf that cannot be moved.
			 */
			Interpolate(Interpolate&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph in the provider.
			 */
			~Interpolate() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Leaf that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			Interpolate& operator=(const Interpolate& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Leaf that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			Interpolate& operator=(Interpolate&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops the cached graph.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets the graph before the first frame.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Pushes one video frame through `minterpolate` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the estimator.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `minterpolate=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			std::string Chain() const noexcept;

			std::uint32_t m_num;													///< Rate numerator
			std::uint32_t m_den;													///< Rate denominator
			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;		///< Reused graph
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Interpolate);
