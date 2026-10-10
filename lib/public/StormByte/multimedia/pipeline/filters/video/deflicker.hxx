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
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <string>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Deflicker
	 * @brief Temporal luminance smoother via libavfilter `deflicker`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Deflicker>(log).
	 *
	 * @par What it is for
	 * Lamp flicker on film scans, exposure pumping, and some
	 * compressed masters whose mean luma jumps frame to frame.
	 * It is not a denoise and not a deband. On a stable digital
	 * master it does almost nothing useful and can flatten
	 * intended fades if @a size is large. Use it when the
	 * brightness itself is the defect, not the grain.
	 *
	 * @par Algorithm
	 * Measures mean luma of each picture and scales the whole
	 * frame so that luma tracks a moving average of the last
	 * @a size frames. Mode selects the average (`am` arithmetic,
	 * `gm` geometric, `hm` harmonic, `qm` quadratic, `cm` cubic,
	 * `pm` power, `median`). The graph owns the window; this
	 * leaf does not Hold.
	 *
	 * Early frames may not leave @c buffersink (`EAGAIN`).
	 * @ref Process then returns without @ref Filter::FFmpeg::Save.
	 * Drain the tail in @ref Eof via @c AVFilterGraph::Flush.
	 *
	 * @par HDR
	 * A global luma gain. Primaries, transfer, range, chroma
	 * siting and SAR are copied onto the sink frame. PQ / HLG
	 * mastering metadata is not rewritten. Keep @a size modest
	 * on HDR so a fade-to-black is not “corrected”.
	 *
	 * @par Defaults
	 * Empty arguments: @c size=5, @c mode=am. Same as FFmpeg.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 * Hardware frames Fail. Non-video units are ignored.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 * @par Conditional DLL safety
	 * Requires a compatible C++ ABI and all providers to remain loaded while
	 * objects, handles or callbacks exist. Create the leaf on Base's heap through
	 * a Safe owner. Private storage is created, used and destroyed out of line
	 * by the provider; the private Chain string never transfers ownership to callers.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Deflicker: public Filter::Process {
		public:
			/**
			 * @brief Temporal deflicker (`deflicker`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param size Moving-average window in frames [2, 129]. Empty → 5.
			 * @param mode Average kind: @c am, @c gm, @c hm, @c qm, @c cm,
			 *        @c pm or @c median. Empty → @c am.
			 */
			Deflicker(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<unsigned> size = {},
				Safe::Optional<Safe::String> mode = {}) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Leaf that cannot be copied.
			 */
			Deflicker(const Deflicker& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Leaf that cannot be moved.
			 */
			Deflicker(Deflicker&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Deflicker() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Leaf that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			Deflicker& operator=(const Deflicker& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Leaf that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			Deflicker& operator=(Deflicker&& other) noexcept = delete;

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
			 * @brief Pushes one video frame through `deflicker` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the temporal window.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `deflicker=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			StormByte::Safe::String Chain() const noexcept;

			Safe::Optional<unsigned> m_sizeIn;	///< Caller window, or empty
			Safe::Optional<Safe::String> m_modeIn;	///< Caller mode, or empty
			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused graph
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Deflicker);
