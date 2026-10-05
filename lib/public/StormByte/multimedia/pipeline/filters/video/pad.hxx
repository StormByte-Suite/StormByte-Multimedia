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

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Pad
	 * @brief Add a black canvas around a picture via libavfilter `pad`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Pad>(log, w, h).
	 *
	 * @par What it is for
	 * Put a smaller picture on a larger frame (4:3 into 16:9,
	 * a cropped window back onto a delivery size). It does
	 * **not** scale and does **not** invent detail. Pair after
	 * @ref Crop when you need a fixed output size.
	 *
	 * @par Do not stack
	 * One Pad. Destination smaller than the source Fails
	 * (that is @ref Crop or @ref Scale). Same size is a no-op
	 * (no graph, no Save). Hardware frames Fail.
	 *
	 * @par Algorithm
	 * @c pad=width=:height=:x=(ow-iw)/2:y=(oh-ih)/2:color=black.
	 * Picture is centred. Colour metadata of the source is
	 * copied onto the sink frame.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Crop
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 * @par ABI contract
	 * Requires the same compatible C++ ABI and a Safe-migrated parent base.
	 * Allocation and destruction must remain creator-owned; graph storage
	 * is created and released by out-of-line Multimedia functions.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Pad: public Filter::Process {
		public:
			/**
			 * @brief Pad to @p width × @p height, centred, black.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param width Destination width in pixels.
			 * @param height Destination height in pixels.
			 */
			Pad(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				std::uint32_t width, std::uint32_t height) noexcept;

			/**
			 * @brief Copy construction is not allowed for a mounted leaf.
			 * @param other Source leaf.
			 */
			Pad(const Pad& other) = delete;

			/**
			 * @brief Move construction is not allowed for a mounted leaf.
			 * @param other Source leaf.
			 */
			Pad(Pad&& other) noexcept = delete;

			/**
			 * @brief Releases graph storage in the creating module.
			 */
			~Pad() noexcept override;

			/**
			 * @brief Copy assignment is not allowed for a mounted leaf.
			 * @param other Source leaf.
			 * @return This leaf.
			 */
			Pad& operator=(const Pad& other) = delete;

			/**
			 * @brief Move assignment is not allowed for a mounted leaf.
			 * @param other Source leaf.
			 * @return This leaf.
			 */
			Pad& operator=(Pad&& other) noexcept = delete;

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
			 * @brief Pushes one video frame through `pad` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains a frame still held by the graph, if any.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `pad=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			StormByte::Safe::String Chain() const noexcept;

			std::uint32_t m_width;																	///< Destination width in pixels
			std::uint32_t m_height;																	///< Destination height in pixels
			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Creator-owned graph
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Pad);
