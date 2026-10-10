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

#include <StormByte/multimedia/visibility.h>

/**
 * @namespace StormByte
 * @brief StormByte library root namespace.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia classes and helpers.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Property
		 * @brief Media property value types.
		 */
		namespace Property {
			/**
			 * @enum ChannelLayout
			 * @brief Curated speaker layouts. Unknown covers unlisted AVChannelLayout values.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC ChannelLayout {
				Unknown,			///< Unlisted or missing
				Mono,				///< 1.0
				Stereo,				///< 2.0
				TwoPointOne,			///< 2.1
				ThreePointZero,			///< 3.0
				FourPointZero,			///< 4.0
				Quad,				///< Quad
				FivePointZero,			///< 5.0
				FivePointOne,			///< 5.1
				SixPointOne,			///< 6.1
				SevenPointOne,			///< 7.1
				SevenPointOneW,			///< 7.1 wide
				Octagonal,			///< 8.0 octagonal
				TwentyTwoPointTwo		///< 22.2
			};

			/**
			 * @brief Converts a ChannelLayout to a string.
			 * @param layout Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(ChannelLayout layout) noexcept {
				switch (layout) {
					case ChannelLayout::Unknown:			return "Unknown";
					case ChannelLayout::Mono:			return "Mono";
					case ChannelLayout::Stereo:			return "Stereo";
					case ChannelLayout::TwoPointOne:		return "2.1";
					case ChannelLayout::ThreePointZero:		return "3.0";
					case ChannelLayout::FourPointZero:		return "4.0";
					case ChannelLayout::Quad:			return "Quad";
					case ChannelLayout::FivePointZero:		return "5.0";
					case ChannelLayout::FivePointOne:		return "5.1";
					case ChannelLayout::SixPointOne:		return "6.1";
					case ChannelLayout::SevenPointOne:		return "7.1";
					case ChannelLayout::SevenPointOneW:		return "7.1W";
					case ChannelLayout::Octagonal:			return "Octagonal";
					case ChannelLayout::TwentyTwoPointTwo:		return "22.2";
					default:					return "Invalid";
				}
			}

			/**
			 * @brief Channel count of a standard layout.
			 * @param layout Layout.
			 * @return Channel count, or 0 if unknown.
			 */
			constexpr unsigned ChannelCount(ChannelLayout layout) noexcept {
				switch (layout) {
					case ChannelLayout::Mono:			return 1;
					case ChannelLayout::Stereo:			return 2;
					case ChannelLayout::TwoPointOne:		return 3;
					case ChannelLayout::ThreePointZero:		return 3;
					case ChannelLayout::FourPointZero:		return 4;
					case ChannelLayout::Quad:			return 4;
					case ChannelLayout::FivePointZero:		return 5;
					case ChannelLayout::FivePointOne:		return 6;
					case ChannelLayout::SixPointOne:		return 7;
					case ChannelLayout::SevenPointOne:		return 8;
					case ChannelLayout::SevenPointOneW:		return 8;
					case ChannelLayout::Octagonal:			return 8;
					case ChannelLayout::TwentyTwoPointTwo:		return 24;
					default:					return 0;
				}
			}
		}
	}
}
