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
#include <StormByte/type_traits/safe.hxx>

/**
 * @namespace StormByte::Multimedia::Property
 * @brief Media property value types.
 */
namespace StormByte::Multimedia::Property {
	/**
	 * @class Point
	 * @brief Integer 2D point (chromaticity / luminance pair).
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Point final {
		public:
			/**
			 * @brief Constructs the zero point for Safe value storage.
			 */
			Point() noexcept = default;

			/**
			 * @brief Constructs a point.
			 * @param x X coordinate.
			 * @param y Y coordinate.
			 */
			Point(int x, int y) noexcept;

			/**
			 * @brief Copy constructor.
			 */
			Point(const Point&) = default;

			/**
			 * @brief Move constructor.
			 */
			Point(Point&&) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~Point() noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @return *this.
			 */
			Point& operator=(const Point&) = default;

			/**
			 * @brief Move assignment.
			 * @return *this.
			 */
			Point& operator=(Point&&) noexcept = default;

			/**
			 * @brief X coordinate.
			 * @return X.
			 */
			int X() const noexcept;

			/**
			 * @brief Y coordinate.
			 * @return Y.
			 */
			int Y() const noexcept;

			/**
			 * @brief Builds a point scaled to a common denominator.
			 * @param numerator_x X numerator.
			 * @param denominator_x X denominator.
			 * @param numerator_y Y numerator.
			 * @param denominator_y Y denominator.
			 * @param denominator Target scale denominator.
			 * @return Normalized point.
			 */
			static Point Normalized(int numerator_x, int denominator_x, int numerator_y, int denominator_y, int denominator) noexcept;

		private:
			int m_x = 0;	///< X coordinate, initialized to zero.

			int m_y = 0;	///< Y coordinate, initialized to zero.
	};
}

/**
 * @brief Registers the completed integer pair for Safe value storage.
 *
 * Copy, move and destruction own no heap storage. Consumers must use a compatible
 * integer/class ABI and keep the Multimedia provider loaded for exported operations.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::Point);
