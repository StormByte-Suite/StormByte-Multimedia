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

#include <StormByte/multimedia/property/av_rational.hxx>
#include <StormByte/multimedia/property/color.hxx>
#include <StormByte/multimedia/property/dovi.hxx>
#include <StormByte/multimedia/property/hdr10.hxx>
#include <StormByte/multimedia/property/resolution.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>

/**
 * @namespace StormByte
 * @brief StormByte library root namespace.
 */
namespace StormByte {
	/**
	 * @namespace Multimedia
	 * @brief Multimedia classes and helpers.
	 */
	namespace Multimedia {
		/**
		 * @namespace Property
		 * @brief Media property value types.
		 */
		namespace Property {
			/**
			 * @class Video
			 * @brief Per-stream video properties.
			 * @note DLL exchange requires compatible C++ ABIs. Base and Multimedia must
			 *       remain loaded while their values and provider callbacks are in use.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Video final {
				public:
					/**
					 * @brief Constructs unknown video properties without optional metadata.
					 * @throws StormByte::Exception Safe storage initialization failed.
					 */
					Video();

					/**
					 * @brief Constructs video properties.
					 * @param color Colorimetry and pixel format.
					 * @param resolution Frame size.
					 * @param hdr10 Optional mastering-display metadata.
					 * @param frameRate Frames per second (`{num, den}` like libav). Empty if unknown.
					 * @param sampleAspectRatio Pixel aspect (`sample_aspect_ratio`). Empty if unknown.
					 * @param dovi Optional Dolby Vision configuration and decoded metadata.
					 */
					Video(Color color, Resolution resolution,
						StormByte::Safe::Optional<HDR10> hdr10 = std::nullopt,
						StormByte::Safe::Optional<AVRational> frameRate = std::nullopt,
						StormByte::Safe::Optional<AVRational> sampleAspectRatio = std::nullopt,
						StormByte::Safe::Optional<class DOVI> dovi = std::nullopt) noexcept;

					/**
					 * @brief Copy constructor.
					 * @param other Properties to copy.
					 * @throws StormByte::Exception Safe storage copying failed.
					 */
					Video(const Video& other);

					/**
					 * @brief Move constructor.
					 * @param other Properties to move.
					 */
					Video(Video&& other) noexcept;

					/**
					 * @brief Destructor.
					 */
					~Video() noexcept;

					/**
					 * @brief Copy assignment.
					 * @param other Properties to copy.
					 * @return *this.
					 * @throws StormByte::Exception Safe storage copying failed.
					 */
					Video& operator=(const Video& other);

					/**
					 * @brief Move assignment.
					 * @param other Properties to move.
					 * @return *this.
					 */
					Video& operator=(Video&& other) noexcept;

					/**
					 * @brief Colorimetry and pixel format.
					 * @return Color.
					 */
					const class Color& Color() const noexcept;

					/**
					 * @brief Frame size.
					 * @return Resolution.
					 */
					const class Resolution& Resolution() const noexcept;

					/**
					 * @brief Mastering-display metadata, if present.
					 * @return Borrowed optional HDR10, valid while this object is alive.
					 */
					const StormByte::Safe::Optional<class HDR10>& HDR10() const noexcept;

					/**
					 * @brief Dolby Vision configuration and decoded metadata, if present.
					 * @return Borrowed optional DOVI, valid while this object is alive.
					 */
					const StormByte::Safe::Optional<class DOVI>& DOVI() const noexcept;

					/**
					 * @brief Stream frame rate, if the container exposed one.
					 * @return Borrowed optional fps (`{24000, 1001}` for 23.976), valid while this object is alive.
					 */
					const StormByte::Safe::Optional<AVRational>& FrameRate() const noexcept;

					/**
					 * @brief Pixel aspect ratio (`sample_aspect_ratio`).
					 * @return Borrowed optional SAR (`{8, 9}` anamorphic NTSC, `{1, 1}` square), valid while this object is alive.
					 */
					const StormByte::Safe::Optional<AVRational>& SampleAspectRatio() const noexcept;

				private:
					class Color m_color;					///< Colorimetry and pixel format.
					class Resolution m_resolution;				///< Frame size.
					StormByte::Safe::Optional<class HDR10> m_hdr10;		///< Optional mastering-display metadata.
					StormByte::Safe::Optional<AVRational> m_frameRate;	///< Optional frames per second.
					StormByte::Safe::Optional<AVRational> m_sar;		///< Optional sample aspect ratio.
					StormByte::Safe::Optional<class DOVI> m_dovi;		///< Optional Dolby Vision properties.
			};
		}
	}
}

/**
 * @brief Registers completed video properties after all contained value types.
 *
 * Optional metadata uses Base's heap and creator callbacks; copying, movement and
 * destruction are exported by Multimedia. Compatible provider ABI is required
 * and providers and callback creators must outlive their dependent values.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::Video);
