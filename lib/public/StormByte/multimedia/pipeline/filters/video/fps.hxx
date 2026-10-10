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
	 * @class Fps
	 * @brief Constant frame rate by drop/dup via libavfilter `fps`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Fps>(log, 24000, 1001).
	 *
	 * @par What it is for
	 * Force CFR without inventing pictures: 24000/1001, 24,
	 * 25, 30. Duration stays; frames are copied, duplicated
	 * or dropped. For 24/30 → 50/60 *smooth* motion use
	 * @ref Interpolate instead.
	 *
	 * @par Do not stack
	 * One rate policy: Fps **or** Interpolate, not both.
	 * Not a speed change. Hardware frames Fail.
	 * num = 0 Fails.
	 *
	 * @par Algorithm
	 * @c fps=fps=&lt;num&gt;/&lt;den&gt;:round=near.
	 * The graph owns the timing window. EAGAIN = wait.
	 * @ref Eof flushes.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Interpolate
	 * @par Conditional DLL safety
	 * Requires a compatible C++ ABI and all providers to remain loaded while
	 * objects, handles or callbacks exist. Create the leaf on Base's heap through
	 * a Safe owner. Private storage is created, used and destroyed out of line
	 * by the provider; the private Chain string never transfers ownership to callers.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Fps: public Filter::Process {
		public:
			/**
			 * @brief Constant rate @p num / @p den (`fps=`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param num Rate numerator (e.g. 24000).
			 * @param den Rate denominator (e.g. 1001). 0 is treated as 1.
			 */
			Fps(Safe::Shared<StormByte::Logger::Log> log,
				std::uint32_t num, std::uint32_t den = 1) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Leaf that cannot be copied.
			 */
			Fps(const Fps& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Leaf that cannot be moved.
			 */
			Fps(Fps&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph in the provider.
			 */
			~Fps() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Leaf that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			Fps& operator=(const Fps& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Leaf that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			Fps& operator=(Fps&& other) noexcept = delete;

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
			 * @brief Pushes one video frame through `fps` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the timing window.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `fps=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			std::string Chain() const noexcept;

			std::uint32_t m_num;	///< Rate numerator
			std::uint32_t m_den;	///< Rate denominator
			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused graph
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Fps);
