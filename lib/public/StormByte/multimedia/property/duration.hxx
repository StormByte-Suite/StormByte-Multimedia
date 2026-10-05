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
#include <StormByte/safe/string.hxx>

#include <chrono>

/**
 * @namespace StormByte::Multimedia::Property
 * @brief Media property value types.
 */
namespace StormByte::Multimedia::Property {
	/**
	 * @class Duration
	 * @brief Media duration stored as nanoseconds.
	 * @note DLL exchange requires compatible C++ ABIs. Base and Multimedia must
	 *       remain loaded while their values and provider callbacks are in use.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Duration final {
		public:
			/**
			 * @brief Constructs a zero duration for Safe value storage.
			 */
			Duration() noexcept = default;

			/**
			 * @brief Constructs from nanoseconds.
			 * @param value Duration.
			 */
			explicit Duration(std::chrono::nanoseconds value) noexcept;

			/**
			 * @brief Copy constructor.
			 */
			Duration(const Duration&) = default;

			/**
			 * @brief Move constructor.
			 */
			Duration(Duration&&) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~Duration() noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @return *this.
			 */
			Duration& operator=(const Duration&) = default;

			/**
			 * @brief Move assignment.
			 * @return *this.
			 */
			Duration& operator=(Duration&&) noexcept = default;

			/**
			 * @brief Three-way compare.
			 */
			auto operator<=>(const Duration&) const noexcept = default;

			/**
			 * @brief Duration in nanoseconds.
			 * @return Stored value.
			 */
			std::chrono::nanoseconds Nanoseconds() const noexcept;

			/**
			 * @brief `[HH:][MM:]SS.mmm` (hours/minutes omitted when zero).
			 * @return Human-readable text.
			 * @throws StormByte::Exception If Base-owned text cannot be allocated.
			 * @throws std::bad_alloc If temporary formatting storage cannot be allocated.
			 */
			StormByte::Safe::String ToString() const;

		private:
			std::chrono::nanoseconds m_value{};	///< Duration value, initialized to zero.
	};
}

/**
 * @brief Registers the completed duration under a compatible-toolchain ABI contract.
 *
 * The chrono representation owns no heap storage but retains its standard-library
 * ABI. Formatting returns Base-owned text. Consumers must use a compatible chrono
 * representation and keep the Multimedia and Base providers loaded.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::Duration);
