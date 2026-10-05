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

#include <StormByte/multimedia/property/point.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>

/**
 * @namespace StormByte::Multimedia::Property
 * @brief Media property value types.
 */
namespace StormByte::Multimedia::Property {
	/**
	 * @class HDR10
	 * @brief Mastering display and content light level metadata.
	 * @note DLL exchange requires compatible C++ ABIs. Base and Multimedia must
	 *       remain loaded while their values and provider callbacks are in use.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC HDR10 final {
		public:
			/**
			 * @enum Source
			 * @brief Origin of the mastering-display numbers.
			 */
			enum class Source {
				/**
				 * @brief Side data from the container or bitstream.
				 */
				Metadata,

				/**
				 * @brief Default values when HDR10 is signaled without mastering metadata.
				 */
				Heuristics
			};

			/**
			 * @brief DEFAULT primaries / luminance, Source::Heuristics.
			 * @throws StormByte::Exception Safe storage initialization failed.
			 */
			HDR10();

			/**
			 * @brief Full mastering-display description.
			 * @param red Red primary.
			 * @param green Green primary.
			 * @param blue Blue primary.
			 * @param white White point.
			 * @param luminance Min/max luminance pair.
			 * @param light_level Optional MaxCLL/MaxFALL.
			 * @param source Metadata or Heuristics.
			 * @throws StormByte::Exception Safe storage copying failed.
			 */
			HDR10(const Point& red, const Point& green, const Point& blue, const Point& white,
				const Point& luminance, const StormByte::Safe::Optional<Point>& light_level = std::nullopt,
				Source source = Source::Metadata);

			/**
			 * @brief Move overload of the full constructor.
			 * @param red Red primary.
			 * @param green Green primary.
			 * @param blue Blue primary.
			 * @param white White point.
			 * @param luminance Min/max luminance pair.
			 * @param light_level Optional MaxCLL/MaxFALL.
			 * @param source Metadata or Heuristics.
			 */
			HDR10(Point&& red, Point&& green, Point&& blue, Point&& white,
				Point&& luminance, StormByte::Safe::Optional<Point>&& light_level = std::nullopt,
				Source source = Source::Metadata) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Metadata to copy.
			 * @throws StormByte::Exception Safe storage copying failed.
			 */
			HDR10(const HDR10& other);

			/**
			 * @brief Move constructor.
			 * @param other Metadata to move.
			 */
			HDR10(HDR10&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~HDR10() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Metadata to copy.
			 * @return *this.
			 * @throws StormByte::Exception Safe storage copying failed.
			 */
			HDR10& operator=(const HDR10& other);

			/**
			 * @brief Move assignment.
			 * @param other Metadata to move.
			 * @return *this.
			 */
			HDR10& operator=(HDR10&& other) noexcept;

			/**
			 * @brief Red primary.
			 * @return Red.
			 */
			const Point& Red() const noexcept;

			/**
			 * @brief Green primary.
			 * @return Green.
			 */
			const Point& Green() const noexcept;

			/**
			 * @brief Blue primary.
			 * @return Blue.
			 */
			const Point& Blue() const noexcept;

			/**
			 * @brief White point.
			 * @return White.
			 */
			const Point& White() const noexcept;

			/**
			 * @brief Luminance (min, max).
			 * @return Luminance.
			 */
			const Point& Luminance() const noexcept;

			/**
			 * @brief Optional content light level (MaxCLL, MaxFALL).
			 * @return Borrowed optional light level, valid while this object is alive.
			 */
			const StormByte::Safe::Optional<Point>& LightLevel() const noexcept;

			/**
			 * @brief Origin of the numbers.
			 * @return Metadata or Heuristics.
			 */
			Source Origin() const noexcept;

			/**
			 * @brief HDR10+ dynamic metadata flag.
			 * @return true if HDR10+ was detected.
			 */
			bool IsHDR10Plus() const noexcept;

			/**
			 * @brief Sets the HDR10+ flag.
			 * @param hdrplus New value.
			 */
			void HDR10Plus(bool hdrplus) noexcept;

			static const HDR10 DEFAULT;	///< Fallback mastering display with heuristic origin.

		private:
			Point m_red;									///< Red primary.

			Point m_green;									///< Green primary.

			Point m_blue;									///< Blue primary.

			Point m_white;									///< White point.

			Point m_luminance;								///< Minimum and maximum luminance.

			StormByte::Safe::Optional<Point> m_light_level;	///< Optional MaxCLL and MaxFALL values.

			Source m_source;								///< Origin of the mastering display values.

			bool m_hdr10plus;								///< Whether HDR10+ metadata is present.
	};
}

/**
 * @brief Registers completed mastering metadata after its Point dependency.
 *
 * Optional light-level storage uses Base's heap and creator callbacks; copying,
 * movement and destruction are exported by Multimedia. Consumers must use a
 * compatible provider ABI and keep both providers and callback creators loaded.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::HDR10);
