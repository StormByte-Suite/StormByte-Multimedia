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

#include <StormByte/multimedia/metadata/tag.hxx>

#include <cstdint>
#include <string_view>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Backend
		 * @brief Multimedia-owned pipeline stages and unit holders.
		 */
		namespace Backend {
			/**
			 * @struct TagSupport
			 * @brief Tags an FFmpeg muxer stores so that they read back.
			 */
			struct TagSupport {
				std::uint16_t file = 0;		///< Bit per Metadata::FileTag
				std::uint8_t stream = 0;	///< Bit per Metadata::StreamTag
				bool sharedTitle = false;	///< File and stream titles share one field

				/**
				 * @brief Whether @p tag is stored.
				 * @param tag File tag.
				 * @return true if stored.
				 */
				constexpr bool Has(StormByte::Multimedia::Metadata::FileTag tag) const noexcept {
					return (file >> static_cast<unsigned>(tag)) & 1U;
				}

				/**
				 * @brief Whether @p tag is stored.
				 * @param tag Stream tag.
				 * @return true if stored.
				 */
				constexpr bool Has(StormByte::Multimedia::Metadata::StreamTag tag) const noexcept {
					return (stream >> static_cast<unsigned>(tag)) & 1U;
				}
			};

			/**
			 * @brief Tags written by FFmpeg muxer @p formatName.
			 * @param formatName AVOutputFormat name.
			 * @return Support; unknown muxers store nothing.
			 */
			TagSupport SelectTagSupport(std::string_view formatName) noexcept;

			/**
			 * @brief FFmpeg metadata key of @p tag.
			 * @param tag File tag.
			 * @return Key such as "album_artist".
			 */
			const char* FileTagKey(StormByte::Multimedia::Metadata::FileTag tag) noexcept;
		}
	}
}
