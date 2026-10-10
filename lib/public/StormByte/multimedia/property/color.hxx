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

#include <StormByte/multimedia/property/pixel_format.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/type_traits/safe.hxx>

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
			 * @enum Range
			 * @brief Luma/chroma sample range.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC Range {
				Unspecified,		///< Not signaled.
				TV,			///< Limited / MPEG.
				Full,			///< Full / JPEG.
				Unknown			///< Unlisted backend value.
			};

			/**
			 * @enum Space
			 * @brief YUV / RGB matrix coefficients.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC Space {
				Unspecified,			///< Not signaled.
				RGB,				///< RGB.
				BT709,				///< BT.709.
				FCC,				///< FCC.
				BT470BG,			///< BT.470 BG.
				SMPTE170M,			///< SMPTE 170M.
				SMPTE240M,			///< SMPTE 240M.
				YCgCo,				///< YCgCo.
				BT2020NCL,			///< BT.2020 non-constant luminance.
				BT2020CL,			///< BT.2020 constant luminance.
				SMPTE2085,			///< SMPTE 2085.
				ChromaDerivedNCL,		///< Chroma-derived NCL.
				ChromaDerivedCL,		///< Chroma-derived CL.
				ICtCp,				///< ICtCp.
				Unknown,			///< Unlisted backend value.
				IPTC2				///< IPT-PQ-C2 (SMPTE ST 2128).
			};

			/**
			 * @enum Primaries
			 * @brief Chromaticity primaries.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC Primaries {
				Unspecified,		///< Not signaled.
				BT709,			///< BT.709.
				BT470M,			///< BT.470 M.
				BT470BG,		///< BT.470 BG.
				SMPTE170M,		///< SMPTE 170M.
				SMPTE240M,		///< SMPTE 240M.
				Film,			///< Film.
				BT2020,			///< BT.2020.
				SMPTE428,		///< SMPTE 428.
				SMPTE431,		///< SMPTE 431 (DCI-P3).
				SMPTE432,		///< SMPTE 432 (Display P3).
				EBU3213,		///< EBU 3213.
				Unknown			///< Unlisted backend value.
			};

			/**
			 * @enum Transfer
			 * @brief Transfer characteristics.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC Transfer {
				Unspecified,		///< Not signaled.
				BT709,			///< BT.709.
				Gamma22,		///< Gamma 2.2.
				Gamma28,		///< Gamma 2.8.
				SMPTE170M,		///< SMPTE 170M.
				SMPTE240M,		///< SMPTE 240M.
				Linear,			///< Linear.
				Log,			///< Log.
				LogSqrt,		///< Log sqrt.
				IEC61966_2_4,		///< IEC 61966-2-4.
				BT1361,			///< BT.1361.
				IEC61966_2_1,		///< IEC 61966-2-1 (sRGB).
				BT2020_10,		///< BT.2020 10-bit.
				BT2020_12,		///< BT.2020 12-bit.
				SMPTE2084,		///< PQ.
				SMPTE428,		///< SMPTE 428.
				ARIB_B67,		///< HLG.
				Unknown			///< Unlisted backend value.
			};

			/**
			 * @brief Converts a Range to a string.
			 * @param range Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(Range range) noexcept {
				switch (range) {
					case Range::Unspecified:	return "Unspecified";
					case Range::TV:			return "TV";
					case Range::Full:		return "Full";
					case Range::Unknown:		return "Unknown";
					default:			return "Invalid";
				}
			}

			/**
			 * @brief Converts a Space to a string.
			 * @param space Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(Space space) noexcept {
				switch (space) {
					case Space::Unspecified:	return "Unspecified";
					case Space::RGB:		return "RGB";
					case Space::BT709:		return "BT709";
					case Space::FCC:		return "FCC";
					case Space::BT470BG:		return "BT470BG";
					case Space::SMPTE170M:		return "SMPTE170M";
					case Space::SMPTE240M:		return "SMPTE240M";
					case Space::YCgCo:		return "YCgCo";
					case Space::BT2020NCL:		return "BT2020NCL";
					case Space::BT2020CL:		return "BT2020CL";
					case Space::SMPTE2085:		return "SMPTE2085";
					case Space::ChromaDerivedNCL:	return "ChromaDerivedNCL";
					case Space::ChromaDerivedCL:	return "ChromaDerivedCL";
					case Space::ICtCp:		return "ICtCp";
					case Space::Unknown:		return "Unknown";
					case Space::IPTC2:		return "IPT-C2";
					default:			return "Invalid";
				}
			}

			/**
			 * @brief Converts Primaries to a string.
			 * @param primaries Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(Primaries primaries) noexcept {
				switch (primaries) {
					case Primaries::Unspecified:	return "Unspecified";
					case Primaries::BT709:		return "BT709";
					case Primaries::BT470M:		return "BT470M";
					case Primaries::BT470BG:	return "BT470BG";
					case Primaries::SMPTE170M:	return "SMPTE170M";
					case Primaries::SMPTE240M:	return "SMPTE240M";
					case Primaries::Film:		return "Film";
					case Primaries::BT2020:		return "BT2020";
					case Primaries::SMPTE428:	return "SMPTE428";
					case Primaries::SMPTE431:	return "SMPTE431";
					case Primaries::SMPTE432:	return "SMPTE432";
					case Primaries::EBU3213:	return "EBU3213";
					case Primaries::Unknown:	return "Unknown";
					default:			return "Invalid";
				}
			}

			/**
			 * @brief Converts Transfer to a string.
			 * @param transfer Value to convert.
			 * @return Null-terminated string literal.
			 */
			constexpr const char* ToString(Transfer transfer) noexcept {
				switch (transfer) {
					case Transfer::Unspecified:	return "Unspecified";
					case Transfer::BT709:		return "BT709";
					case Transfer::Gamma22:		return "Gamma22";
					case Transfer::Gamma28:		return "Gamma28";
					case Transfer::SMPTE170M:	return "SMPTE170M";
					case Transfer::SMPTE240M:	return "SMPTE240M";
					case Transfer::Linear:		return "Linear";
					case Transfer::Log:		return "Log";
					case Transfer::LogSqrt:		return "LogSqrt";
					case Transfer::IEC61966_2_4:	return "IEC61966_2_4";
					case Transfer::BT1361:		return "BT1361";
					case Transfer::IEC61966_2_1:	return "IEC61966_2_1";
					case Transfer::BT2020_10:	return "BT2020_10";
					case Transfer::BT2020_12:	return "BT2020_12";
					case Transfer::SMPTE2084:	return "SMPTE2084";
					case Transfer::SMPTE428:	return "SMPTE428";
					case Transfer::ARIB_B67:	return "ARIB_B67";
					case Transfer::Unknown:		return "Unknown";
					default:			return "Invalid";
				}
			}

			/**
			 * @class Color
			 * @brief Pixel format plus colorimetry.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Color final {
				public:
					/**
					 * @brief Constructs unknown pixel format and unspecified colorimetry.
					 */
					Color() noexcept = default;

					/**
					 * @brief Full color description.
					 * @param format Pixel format.
					 * @param range Sample range.
					 * @param space Matrix coefficients.
					 * @param primaries Chromaticity primaries.
					 * @param transfer Transfer characteristics.
					 */
					Color(PixelFormat format, Range range, Space space, Primaries primaries, Transfer transfer) noexcept;

					/**
					 * @brief Copies the source color description.
					 * @par Source
					 * The unnamed parameter is the color description to copy.
					 */
					Color(const Color&) = default;

					/**
					 * @brief Moves the source color description.
					 * @par Source
					 * The unnamed parameter is the color description to move.
					 */
					Color(Color&&) noexcept = default;

					/**
					 * @brief Destroys the color description.
					 */
					~Color() noexcept = default;

					/**
					 * @brief Copies the source color description into this object.
					 * @par Source
					 * The unnamed parameter is the color description to copy.
					 * @return This object.
					 */
					Color& operator=(const Color&) = default;

					/**
					 * @brief Moves the source color description into this object.
					 * @par Source
					 * The unnamed parameter is the color description to move.
					 * @return This object.
					 */
					Color& operator=(Color&&) noexcept = default;

					/**
					 * @brief Pixel format.
					 * @return Pixel format.
					 */
					enum PixelFormat PixelFormat() const noexcept;

					/**
					 * @brief Sample range.
					 * @return Sample range.
					 */
					enum Range Range() const noexcept;

					/**
					 * @brief Matrix coefficients.
					 * @return Matrix coefficients.
					 */
					enum Space Space() const noexcept;

					/**
					 * @brief Chromaticity primaries.
					 * @return Chromaticity primaries.
					 */
					enum Primaries Primaries() const noexcept;

					/**
					 * @brief Transfer characteristics.
					 * @return Transfer characteristics.
					 */
					enum Transfer Transfer() const noexcept;

					/**
					 * @brief HDR10 colorimetry (10-bit+, BT.2020, PQ).
					 * @return true if the combination matches HDR10.
					 */
					bool IsHDR10() const noexcept;

					/**
					 * @brief HLG colorimetry (10-bit+, BT.2020, ARIB B67).
					 * @return true if the combination matches HLG.
					 */
					bool IsHLG() const noexcept;

				private:
					enum PixelFormat m_format = PixelFormat::Unknown;	///< Pixel format, initialized to unknown.
					enum Range m_range = Range::Unspecified;		///< Sample range, initialized to unspecified.
					enum Space m_space = Space::Unspecified;		///< Matrix coefficients, initialized to unspecified.
					enum Primaries m_primaries = Primaries::Unspecified;	///< Chromaticity primaries, initialized to unspecified.
					enum Transfer m_transfer = Transfer::Unspecified;	///< Transfer characteristics, initialized to unspecified.
			};
		}
	}
}

/**
 * @brief Registers the completed color description for Safe value storage.
 *
 * Enum-valued fields own no heap storage and value lifetime operations allocate
 * nothing. Compatible provider ABI and provider lifetime are required.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::Color);
