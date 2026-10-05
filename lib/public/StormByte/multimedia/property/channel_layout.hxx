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
				/**
				 * @brief Unlisted or missing
				 */
				Unknown,
				/**
				 * @brief 1.0
				 */
				Mono,
				/**
				 * @brief 2.0
				 */
				Stereo,
				/**
				 * @brief 2.1
				 */
				TwoPointOne,
				/**
				 * @brief 3.0
				 */
				ThreePointZero,
				/**
				 * @brief 4.0
				 */
				FourPointZero,
				/**
				 * @brief Quad
				 */
				Quad,
				/**
				 * @brief 5.0
				 */
				FivePointZero,
				/**
				 * @brief 5.1
				 */
				FivePointOne,
				/**
				 * @brief 6.1
				 */
				SixPointOne,
				/**
				 * @brief 7.1
				 */
				SevenPointOne,
				/**
				 * @brief 7.1 wide
				 */
				SevenPointOneW,
				/**
				 * @brief 8.0 octagonal
				 */
				Octagonal,
				/**
				 * @brief 22.2
				 */
				TwentyTwoPointTwo
			};

			/**
			 * @brief Converts a ChannelLayout to a string.
			 * @param layout Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(ChannelLayout layout) noexcept {
				switch (layout) {
					case ChannelLayout::Unknown:			return "Unknown";
					case ChannelLayout::Mono:				return "Mono";
					case ChannelLayout::Stereo:				return "Stereo";
					case ChannelLayout::TwoPointOne:		return "2.1";
					case ChannelLayout::ThreePointZero:		return "3.0";
					case ChannelLayout::FourPointZero:		return "4.0";
					case ChannelLayout::Quad:				return "Quad";
					case ChannelLayout::FivePointZero:		return "5.0";
					case ChannelLayout::FivePointOne:		return "5.1";
					case ChannelLayout::SixPointOne:		return "6.1";
					case ChannelLayout::SevenPointOne:		return "7.1";
					case ChannelLayout::SevenPointOneW:		return "7.1W";
					case ChannelLayout::Octagonal:			return "Octagonal";
					case ChannelLayout::TwentyTwoPointTwo:	return "22.2";
					default:								return "Invalid";
				}
			}

			/**
			 * @brief Channel count of a standard layout.
			 * @param layout Layout.
			 * @return Channel count, or 0 if unknown.
			 */
			constexpr unsigned ChannelCount(ChannelLayout layout) noexcept {
				switch (layout) {
					case ChannelLayout::Mono:				return 1;
					case ChannelLayout::Stereo:				return 2;
					case ChannelLayout::TwoPointOne:		return 3;
					case ChannelLayout::ThreePointZero:		return 3;
					case ChannelLayout::FourPointZero:		return 4;
					case ChannelLayout::Quad:				return 4;
					case ChannelLayout::FivePointZero:		return 5;
					case ChannelLayout::FivePointOne:		return 6;
					case ChannelLayout::SixPointOne:		return 7;
					case ChannelLayout::SevenPointOne:		return 8;
					case ChannelLayout::SevenPointOneW:		return 8;
					case ChannelLayout::Octagonal:			return 8;
					case ChannelLayout::TwentyTwoPointTwo:	return 24;
					default:								return 0;
				}
			}
		}
	}
}
