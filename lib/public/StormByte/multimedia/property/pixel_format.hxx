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

#include <cstdint>

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
			 * @enum PixelFormat
			 * @brief Curated pixel formats. Unknown covers unlisted AVPixelFormat values.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC PixelFormat {
				/**
				 * @brief Unlisted or missing
				 */
				Unknown,
				/**
				 * @brief Planar YUV 4:2:0 8-bit
				 */
				YUV420P,
				/**
				 * @brief Planar YUV 4:2:2 8-bit
				 */
				YUV422P,
				/**
				 * @brief Planar YUV 4:4:4 8-bit
				 */
				YUV444P,
				/**
				 * @brief Planar YUV 4:2:0 10-bit
				 */
				YUV420P10,
				/**
				 * @brief Planar YUV 4:2:2 10-bit
				 */
				YUV422P10,
				/**
				 * @brief Planar YUV 4:4:4 10-bit
				 */
				YUV444P10,
				/**
				 * @brief Planar YUV 4:2:0 12-bit
				 */
				YUV420P12,
				/**
				 * @brief Planar YUV 4:2:2 12-bit
				 */
				YUV422P12,
				/**
				 * @brief Planar YUV 4:4:4 12-bit
				 */
				YUV444P12,
				/**
				 * @brief Semi-planar YUV 4:2:0 8-bit
				 */
				NV12,
				/**
				 * @brief Semi-planar YUV 4:2:0 8-bit (VU)
				 */
				NV21,
				/**
				 * @brief Semi-planar YUV 4:2:0 10-bit
				 */
				P010,
				/**
				 * @brief Packed RGB 8-bit
				 */
				RGB24,
				/**
				 * @brief Packed BGR 8-bit
				 */
				BGR24,
				/**
				 * @brief Packed RGBA 8-bit
				 */
				RGBA,
				/**
				 * @brief Packed BGRA 8-bit
				 */
				BGRA,
				/**
				 * @brief Gray 8-bit
				 */
				GRAY8,
				/**
				 * @brief Gray 10-bit
				 */
				GRAY10,
				/**
				 * @brief Gray 16-bit
				 */
				GRAY16
			};

			/**
			 * @brief Converts a PixelFormat to a string.
			 * @param format Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(PixelFormat format) noexcept {
				switch (format) {
					case PixelFormat::Unknown:		return "Unknown";
					case PixelFormat::YUV420P:		return "YUV420P";
					case PixelFormat::YUV422P:		return "YUV422P";
					case PixelFormat::YUV444P:		return "YUV444P";
					case PixelFormat::YUV420P10:	return "YUV420P10";
					case PixelFormat::YUV422P10:	return "YUV422P10";
					case PixelFormat::YUV444P10:	return "YUV444P10";
					case PixelFormat::YUV420P12:	return "YUV420P12";
					case PixelFormat::YUV422P12:	return "YUV422P12";
					case PixelFormat::YUV444P12:	return "YUV444P12";
					case PixelFormat::NV12:			return "NV12";
					case PixelFormat::NV21:			return "NV21";
					case PixelFormat::P010:			return "P010";
					case PixelFormat::RGB24:		return "RGB24";
					case PixelFormat::BGR24:		return "BGR24";
					case PixelFormat::RGBA:			return "RGBA";
					case PixelFormat::BGRA:			return "BGRA";
					case PixelFormat::GRAY8:		return "GRAY8";
					case PixelFormat::GRAY10:		return "GRAY10";
					case PixelFormat::GRAY16:		return "GRAY16";
					default:						return "Invalid";
				}
			}

			/**
			 * @brief Component bit depth of @p format.
			 * @param format Pixel format.
			 * @return Bits per component, or 0 if unknown.
			 */
			constexpr std::uint8_t BitDepth(PixelFormat format) noexcept {
				switch (format) {
					case PixelFormat::YUV420P:
					case PixelFormat::YUV422P:
					case PixelFormat::YUV444P:
					case PixelFormat::NV12:
					case PixelFormat::NV21:
					case PixelFormat::RGB24:
					case PixelFormat::BGR24:
					case PixelFormat::RGBA:
					case PixelFormat::BGRA:
					case PixelFormat::GRAY8:
						return 8;
					case PixelFormat::YUV420P10:
					case PixelFormat::YUV422P10:
					case PixelFormat::YUV444P10:
					case PixelFormat::P010:
					case PixelFormat::GRAY10:
						return 10;
					case PixelFormat::YUV420P12:
					case PixelFormat::YUV422P12:
					case PixelFormat::YUV444P12:
						return 12;
					case PixelFormat::GRAY16:
						return 16;
					default:
						return 0;
				}
			}
		}
	}
}
