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
#include <StormByte/safe/string.hxx>

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
			 * @class Resolution
			 * @brief Frame width and height.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Resolution final {
				public:
					/**
					 * @brief Constructs a zero-sized resolution for Safe value storage.
					 */
					Resolution() noexcept = default;

					/**
					 * @brief Constructs a resolution.
					 * @param width Width in pixels.
					 * @param height Height in pixels.
					 */
					Resolution(std::uint32_t width, std::uint32_t height) noexcept;

					/**
					 * @brief Copy constructor.
					 */
					Resolution(const Resolution&) = default;

					/**
					 * @brief Move constructor.
					 */
					Resolution(Resolution&&) noexcept = default;

					/**
					 * @brief Destructor.
					 */
					~Resolution() noexcept = default;

					/**
					 * @brief Copy assignment.
					 * @return *this.
					 */
					Resolution& operator=(const Resolution&) = default;

					/**
					 * @brief Move assignment.
					 * @return *this.
					 */
					Resolution& operator=(Resolution&&) noexcept = default;

					/**
					 * @brief Width in pixels.
					 * @return Width.
					 */
					std::uint32_t Width() const noexcept;

					/**
					 * @brief Height in pixels.
					 * @return Height.
					 */
					std::uint32_t Height() const noexcept;

					/**
					 * @brief "WIDTHxHEIGHT" string.
					 * @return Size string.
					 * @throws StormByte::Exception If Base-owned text cannot be allocated.
					 * @throws std::bad_alloc If temporary formatting storage cannot be allocated.
					 */
					StormByte::Safe::String Name() const;

					/**
					 * @brief Coarse label (e.g. "1080p", "4K").
					 * @return Standard name.
					 * @throws StormByte::Exception If Base-owned text cannot be allocated.
					 */
					StormByte::Safe::String StandardName() const;

				private:
					std::uint32_t m_width = 0;	///< Width in pixels, initialized to zero.
					std::uint32_t m_height = 0;	///< Height in pixels, initialized to zero.
			};
		}
	}
}

/**
 * @brief Registers completed fixed-width dimensions for Safe value storage.
 *
 * Lifetime operations own no heap storage. Formatted text is returned in a
 * Base-owned String. Compatible provider ABI and provider lifetime are required.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::Resolution);
