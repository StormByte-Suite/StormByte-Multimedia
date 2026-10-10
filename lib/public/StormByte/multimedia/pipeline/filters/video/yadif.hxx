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
	 * @class Yadif
	 * @brief YADIF deinterlace. Process leaf. Legacy alternative to @ref Bwdif.
	 *
	 * Attach with @c job.Video(in).Filter<Yadif>(log).
	 *
	 * @par What it is for
	 * Same problem as @ref Bwdif : true interlaced video at
	 * the same output rate. Prefer Bwdif on new jobs. Keep
	 * Yadif when a pipeline was signed off on it or when
	 * residual comb after Fieldmatch needs an interlaced-only
	 * pass (`onlyInterlaced=true`) **after** the match, before
	 * Decimate.
	 *
	 * @par Do not stack
	 * Not with @ref Bwdif. Not before @ref Fieldmatch.
	 * Not on native 24p.
	 *
	 * @par Delay, not Hold
	 * Output waits for a neighbouring frame; @ref Eof produces
	 * the remaining delayed output. Field-rate doubling is out of scope.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Bwdif
	 * @note Conditional boundary safety requires compatible ABI and provider-managed FFmpeg lifetimes.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Yadif: public Filter::Process {
		public:
			/**
			 * @brief YADIF, same frame rate.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param onlyInterlaced Skip frames not marked interlaced.
			 */
			explicit Yadif(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				bool onlyInterlaced = true) noexcept;

			/**
			 * @brief Copy construction is unavailable.
			 * @param other Filter that cannot be copied.
			 */
			Yadif(const Yadif& other) = delete;

			/**
			 * @brief Move construction is unavailable.
			 * @param other Filter that cannot be moved.
			 */
			Yadif(Yadif&& other) noexcept = delete;

			/**
			 * @brief Drops delayed looks.
			 */
			~Yadif() noexcept override;

			/**
			 * @brief Copy assignment is unavailable.
			 * @param other Filter that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			Yadif& operator=(const Yadif& other) = delete;

			/**
			 * @brief Move assignment is unavailable.
			 * @param other Filter that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			Yadif& operator=(Yadif&& other) noexcept = delete;

			/**
			 * @brief Discards delayed frames and resets deinterlacing history.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Weaves the delayed tail and drops it.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Unused. This leaf does not Hold.
			 * @param frame Ignored.
			 */
			void LastChance(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Produces a progressive frame when enough input is available.
			 * @param frame Video unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Calls @ref Clean. No Hold.
			 */
			void Setup() noexcept override;

		private:
			/**
			 * @brief Writes a progressive frame from prev/cur/next and Save.
			 * @param prev Previous picture, or empty.
			 * @param cur Current picture.
			 * @param next Next picture, or empty.
			 *
			 * Empty prev/next fall back to spatial interpolation.
			 * Progressive @p cur with @ref m_onlyInterlaced set
			 * returns without Save.
			 */
			void Weave(const StormByte::Multimedia::FFmpeg::AVFrame& prev,
				const StormByte::Multimedia::FFmpeg::AVFrame& cur,
				const StormByte::Multimedia::FFmpeg::AVFrame& next) noexcept;

			bool m_onlyInterlaced;				///< Skip progressive input
			StormByte::Multimedia::FFmpeg::AVFrame m_prev;	///< Look t-1
			StormByte::Multimedia::FFmpeg::AVFrame m_cur;	///< Look t
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Yadif);
