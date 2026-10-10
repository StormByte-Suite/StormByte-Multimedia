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
	 * @class Fieldmatch
	 * @brief Inverse-telecine field matcher via libavfilter `fieldmatch`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Fieldmatch>(log).Filter<Decimate>(log).
	 *
	 * @par What it is for
	 * Hard 3:2 telecine (film at 24p stuffed into 29.97i/p).
	 * Rebuilds the original progressive frames by pairing
	 * fields. It does **not** drop the leftover duplicate:
	 * that is @ref Decimate on the same stretch. Native
	 * 23.976/24 progressive film must not use this pair.
	 * Soft/hybrid cadence is a worse fit than a clean 3:2.
	 *
	 * @par What it is not
	 * Not a deinterlacer. Do not stack @ref Bwdif *before*
	 * this leaf on a hard telecine: that destroys the field
	 * relationship fieldmatch needs. Residual comb after
	 * the match can go through a deinterlacer in
	 * interlaced-only mode, then Decimate.
	 *
	 * @par Algorithm
	 * Matches fields across neighbouring pictures (p/c/n,
	 * optional u/b). The graph owns the look-ahead; this
	 * leaf does not Hold. Early frames may not leave
	 * @c buffersink (`EAGAIN`). @ref Process then returns
	 * without @ref Filter::FFmpeg::Save. Drain the tail in
	 * @ref Eof via @c AVFilterGraph::Flush.
	 *
	 * @par HDR
	 * Geometry is unchanged. Primaries, transfer, range,
	 * chroma siting and SAR of the source are copied onto
	 * the sink frame.
	 *
	 * @par Defaults
	 * Empty arguments: @c order=auto, @c mode=pc_n,
	 * @c combmatch=sc. Same as FFmpeg. Dirty edits: pass
	 * @c combmatch=full.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 * Hardware frames Fail. Non-video units are ignored.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Decimate
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 * @par Conditional DLL safety
	 * Requires a compatible C++ ABI and all providers to remain loaded while
	 * objects, handles or callbacks exist. Create the leaf on Base's heap through
	 * a Safe owner. Private storage is created, used and destroyed out of line
	 * by the provider; the private Chain string never transfers ownership to callers.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Fieldmatch: public Filter::Process {
		public:
			/**
			 * @brief Field matcher (`fieldmatch`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param order Field order: @c auto, @c tff or @c bff. Empty → @c auto.
			 * @param mode Match strategy: @c pc, @c pc_n, @c pc_u, @c pc_n_ub,
			 *        @c pcn or @c pcn_ub. Empty → @c pc_n.
			 * @param combmatch Comb score use: @c none, @c sc or @c full.
			 *        Empty → @c sc.
			 */
			Fieldmatch(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<Safe::String> order = {},
				Safe::Optional<Safe::String> mode = {},
				Safe::Optional<Safe::String> combmatch = {}) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Leaf that cannot be copied.
			 */
			Fieldmatch(const Fieldmatch& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Leaf that cannot be moved.
			 */
			Fieldmatch(Fieldmatch&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Fieldmatch() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Leaf that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			Fieldmatch& operator=(const Fieldmatch& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Leaf that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			Fieldmatch& operator=(Fieldmatch&& other) noexcept = delete;

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
			 * @brief Pushes one video frame through `fieldmatch` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the match window.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `fieldmatch=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			StormByte::Safe::String Chain() const noexcept;

			Safe::Optional<Safe::String> m_orderIn;	///< Caller order, or empty
			Safe::Optional<Safe::String> m_modeIn;	///< Caller mode, or empty
			Safe::Optional<Safe::String> m_combIn;	///< Caller combmatch, or empty
			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused graph
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Fieldmatch);
