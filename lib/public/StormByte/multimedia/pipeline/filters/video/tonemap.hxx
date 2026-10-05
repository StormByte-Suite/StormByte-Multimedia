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
	 * @class Tonemap
	 * @brief HDR (PQ / HLG) to BT.709 SDR via zscale + `tonemap`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Tonemap>(log).
	 *
	 * @par What it is for
	 * Deliver an HDR10 / HLG master to SDR (web, disc preview,
	 * a BT.709 encode). One destination: Rec.709 / limited /
	 * yuv420p. Not ACES, not libplacebo, not a display-referred
	 * grade.
	 *
	 * @par When not to use it
	 * Source transfer is not PQ (`smpte2084`) and not HLG
	 * (`arib-std-b67`): no-op, no graph. Do not run it on an
	 * already-SDR master “to be safe”. Do not stack two
	 * Tonemap leaves. Geometry is unchanged; use @ref Scale
	 * separately.
	 *
	 * @par Algorithm
	 * @c zscale=t=linear:npl=100[:tin=arib-std-b67],format=gbrpf32le,
	 * @c zscale=p=bt709,tonemap=tonemap=&lt;op&gt;:desat=0,
	 * @c zscale=t=bt709:m=bt709:r=tv,format=yuv420p.
	 * Empty operator is @c hable. Hardware frames Fail.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 * Colour tags on the sink are set to BT.709 / TV.
	 *
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Tonemap: public Filter::Process {
		public:
			/**
			 * @brief HDR → BT.709 SDR (`tonemap`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param op Tone curve: @c hable, @c mobius, @c reinhard,
			 *        @c gamma, @c clip or @c linear. Empty → @c hable.
			 */
			Tonemap(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				StormByte::Safe::Optional<StormByte::Safe::String> op = {}) noexcept;

			/**
			 * @brief Copy construction is unavailable.
			 * @param other Filter that cannot be copied.
			 */
			Tonemap(const Tonemap& other) = delete;
			/**
			 * @brief Move construction is unavailable.
			 * @param other Filter that cannot be moved.
			 */
			Tonemap(Tonemap&& other) noexcept = delete;
			/**
			 * @brief Releases owned options and the cached graph in the provider module.
			 */
			~Tonemap() noexcept override;
			/**
			 * @brief Copy assignment is unavailable.
			 * @param other Filter that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			Tonemap& operator=(const Tonemap& other) = delete;
			/**
			 * @brief Move assignment is unavailable.
			 * @param other Filter that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			Tonemap& operator=(Tonemap&& other) noexcept = delete;

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
			 * @brief Tone-maps one HDR frame to SDR and Save, or no-op.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains a frame still held by the graph, if any.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief PQ (`smpte2084`) or HLG (`arib-std-b67`).
			 * @param src Video frame.
			 * @return true when a tone-map is required.
			 */
			static bool NeedsMap(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Builds the avfilter chain for @p src.
			 * @param src Model frame (HLG adds @c tin=arib-std-b67).
			 * @return Filterchain for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			StormByte::Safe::String Chain(const StormByte::Multimedia::FFmpeg::AVFrame& src) const noexcept;

			/**
			 * @brief Resolved operator name.
			 * @return Owned operator name, falling back to hable for empty or invalid input.
			 */
			StormByte::Safe::String Operator() const noexcept;

			StormByte::Safe::Optional<StormByte::Safe::String> m_opIn;	///< Caller operator, or empty
			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Cached graph with provider-managed FFmpeg resources
	};
}

/**
 * @brief Conditional boundary safety requires compatible ABI and provider-managed FFmpeg lifetimes.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Tonemap);
