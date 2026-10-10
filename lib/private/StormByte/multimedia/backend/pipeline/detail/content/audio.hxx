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

#include <StormByte/multimedia/backend/pipeline/content.hxx>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Content
 * @brief Per-media payload extra policies.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Content {
	/**
	 * @class Audio
	 * @brief Layout-coupled extras after an audio payload swap.
	 *
	 * If sample rate or channel count change, drops
	 * @c AV_FRAME_DATA_DOWNMIX_INFO,
	 * @c AV_FRAME_DATA_MATRIXENCODING and
	 * @c AV_FRAME_DATA_AUDIO_SERVICE_TYPE. ReplayGain is kept:
	 * it is content loudness, not a layout. Does not Fail the tube.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Audio final: public StormByte::Multimedia::Backend::Pipeline::Content {
		public:
			/**
			 * @brief Empty audio strategy.
			 */
			Audio() noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~Audio() noexcept override = default;

			/**
			 * @brief Copy constructor.
			 * @param other Unused.
			 */
			Audio(const Audio& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Unused.
			 */
			Audio(Audio&& other) noexcept = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Unused.
			 * @return *this.
			 */
			Audio& operator=(const Audio& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Unused.
			 * @return *this.
			 */
			Audio& operator=(Audio&& other) noexcept = delete;

			/**
			 * @brief Drop layout-coupled extras on @p after when the card changed.
			 * @param before Raw frame that was in the holder. May be nullptr.
			 * @param after Raw frame the leaf produced. May be nullptr.
			 */
			void Put(const ::AVFrame* before, ::AVFrame* after) noexcept override;
	};
}
