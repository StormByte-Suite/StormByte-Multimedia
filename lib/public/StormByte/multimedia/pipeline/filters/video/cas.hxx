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

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Cas
	 * @brief Contrast Adaptive Sharpen via libavfilter `cas`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Cas>(log).
	 *
	 * @par What it is for
	 * Last-mile acutance after denoise, before the encode. Film,
	 * tape and web masters that look soft once grain is gone;
	 * not a restorer of detail that was never there. Put it
	 * after @ref Atadenoise / @ref Bm3d / @ref Fftdnoiz, not
	 * instead of them. Do not stack `unsharp` on the same
	 * stretch. Skip it on an already over-sharpened master.
	 *
	 * @par Algorithm
	 * AMD FidelityFX CAS. Each sample is sharpened against its
	 * 3×3 neighbourhood in proportion to local contrast: flat
	 * grain is almost untouched, edges pick up acutance. One
	 * frame in, one frame out. The graph owns the look; this
	 * leaf does not Hold.
	 *
	 * @par HDR
	 * Spatial only. Primaries, transfer, range, chroma siting
	 * and SAR of the source frame are copied onto the sink
	 * frame before @ref Filter::FFmpeg::Save. PQ / HLG
	 * metadata is not rewritten.
	 *
	 * @par Defaults
	 * Empty @a strength here is @c 0.4. FFmpeg’s own default
	 * is @c 0 (a no-op). Film after denoise rarely wants the
	 * FFmpeg 0.8 ceiling; 0.4 is the documented StormByte
	 * starting point. Range 0–1.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 * Hardware frames Fail. Non-video units are ignored.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 * @par ABI contract
	 * Requires the same compatible C++ ABI and the parent Safe base migration.
	 * Allocation and destruction must remain creator-owned with providers loaded.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Cas: public Filter::Process {
		public:
			/**
			 * @brief Contrast Adaptive Sharpen (`cas`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param strength Sharpen amount in [0, 1]. Empty → 0.4.
			 */
			Cas(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> strength = {}) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Source filter.
			 */
			Cas(const Cas& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Cas(Cas&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Cas() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Cas& operator=(const Cas& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Cas& operator=(Cas&& other) noexcept = delete;

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
			 * @brief Pushes one video frame through `cas` and Save.
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
			 * @return `cas=strength=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_strengthIn;	///< Caller strength, or empty.
			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Cached graph allocated and destroyed through Base's heap.
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Cas);
