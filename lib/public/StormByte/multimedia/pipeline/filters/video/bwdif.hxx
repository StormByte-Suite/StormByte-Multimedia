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
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/type_traits/safe.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Bwdif
	 * @brief Bob Weaver deinterlace. Process leaf. Preferred deinterlace.
	 *
	 * Attach with @c job.Video(in).Filter<Bwdif>(log).
	 *
	 * @par What it is for
	 * True interlaced video (50i / 59.94i cameras, broadcast).
	 * Same frame rate out (one progressive per interlaced
	 * input). This is the StormByte deinterlace; @ref Yadif
	 * exists as the older interpolator.
	 *
	 * @par What it is not
	 * Not IVTC. Hard 3:2 telecine wants
	 * @ref Fieldmatch + @ref Decimate, not this leaf.
	 * Do not run Bwdif **before** Fieldmatch.
	 *
	 * @par Versus Yadif
	 * Same job, different interpolator (cubic spatial +
	 * temporal fallback). Do not stack @ref Yadif and
	 * @ref Bwdif : pick one. With @c onlyInterlaced the
	 * second would be a no-op anyway.
	 *
	 * @par Delay, not Hold
	 * The first frame waits for a neighbouring frame before output;
	 * @ref Eof produces the remaining delayed output.
	 * Progressive input is a no-op when @c onlyInterlaced
	 * is set. No avfilter `bwdif`.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Yadif
	 * @par Boundary ownership
	 * Requires a compatible C++ ABI and the Multimedia, Logger and Base providers
	 * to remain loaded. Own the leaf through Base-heap Safe pointers. Frame
	 * allocation, mutation and destruction stay in out-of-line provider methods;
	 * copying and moving the leaf are disabled.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Bwdif: public Filter::Process {
		public:
			/**
			 * @brief BWDIF, same frame rate.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param onlyInterlaced Skip frames not marked interlaced.
			 */
			explicit Bwdif(Safe::Shared<StormByte::Logger::Log> log,
				bool onlyInterlaced = true) noexcept;

			/**
			 * @brief Copy is not allowed. Each leaf owns delayed looks.
			 * @param other Leaf that cannot be copied.
			 */
			Bwdif(const Bwdif& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Leaf that cannot be moved.
			 */
			Bwdif(Bwdif&& other) noexcept = delete;

			/**
			 * @brief Drops delayed looks.
			 */
			~Bwdif() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Leaf that cannot be copied.
			 * @return Assignment is unavailable.
			 */
			Bwdif& operator=(const Bwdif& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Leaf that cannot be moved.
			 * @return Assignment is unavailable.
			 */
			Bwdif& operator=(Bwdif&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Discards delayed frames and resets deinterlacing history.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Calls @ref Clean. No Hold.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Produces a progressive frame when enough input is available.
			 * @param frame Video unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Weaves the delayed tail and drops it.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Unused. This leaf does not Hold.
			 * @param frame Ignored.
			 */
			void LastChance(const Pipeline::Frame& frame) noexcept override;

		private:
			/**
			 * @brief Writes a progressive frame from prev/cur/next and Save.
			 * @param prev Previous picture, or empty.
			 * @param cur Current picture.
			 * @param next Next picture, or empty.
			 *
			 * Empty prev/next fall back to cubic spatial
			 * interpolation. Progressive @p cur with
			 * @ref m_onlyInterlaced set returns without Save.
			 */
			void Weave(const StormByte::Multimedia::FFmpeg::AVFrame& prev,
				const StormByte::Multimedia::FFmpeg::AVFrame& cur,
				const StormByte::Multimedia::FFmpeg::AVFrame& next) noexcept;

			bool m_onlyInterlaced;				///< Skip progressive input
			StormByte::Multimedia::FFmpeg::AVFrame m_prev;	///< Look t-1
			StormByte::Multimedia::FFmpeg::AVFrame m_cur;	///< Look t
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Bwdif);
