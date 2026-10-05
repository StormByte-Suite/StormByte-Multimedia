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

#include <StormByte/bitmask.hxx>
#include <StormByte/multimedia/feature.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/string.hxx>

#include <string>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public media types: codecs, containers, registry and stream kinds.
	 */
	namespace Multimedia {
		/**
		 * @class Features
		 * @brief Bitmask of Feature flags.
		 * @note Stores only enum flags and a Bitmask vtable, with no owning heap state.
		 * Conditional DLL safety requires a compatible compiler, standard-library ABI
		 * and provider layout, with the multimedia provider loaded during use.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Features: public StormByte::Bitmask<Features, Feature> {
			public:
				/**
				 * @brief Constructs an empty allocation-free mask.
				 */
				constexpr Features() noexcept: Bitmask() {}

				/**
				 * @brief Constructs an allocation-free mask from feature flags.
				 * @param feature Initial flags; implicit conversion preserves Bitmask behavior.
				 */
				constexpr Features(Feature feature) noexcept: Bitmask(feature) {}

				/**
				 * @brief Copy constructor for the allocation-free mask.
				 * @param other Source mask.
				 */
				constexpr Features(const Features& other) noexcept = default;

				/**
				 * @brief Move constructor for the allocation-free mask.
				 * @param other Source mask.
				 */
				constexpr Features(Features&& other) noexcept = default;

				/**
				 * @brief Destroys the allocation-free mask.
				 */
				constexpr ~Features() noexcept override = default;

				/**
				 * @brief Copies the enum flags.
				 * @param other Source mask.
				 * @return This mask.
				 */
				constexpr Features& operator=(const Features& other) noexcept = default;

				/**
				 * @brief Moves the enum flags.
				 * @param other Source mask.
				 * @return This mask.
				 */
				constexpr Features& operator=(Features&& other) noexcept = default;

				/**
				 * @brief Enabled flags as Base-owned "A | B | C" text.
				 * @return Safe string, empty when no flag is set.
				 */
				operator Safe::String() const noexcept;

				/**
				 * @brief Enabled flags as caller-owned "A | B | C" text.
				 * @return Standard string allocated in the calling module.
				 * @note Allocation failure terminates, preserving the noexcept contract.
				 */
				STORMBYTE_FORCE_INLINE operator std::string() const noexcept {
					return static_cast<std::string>(static_cast<Safe::String>(*this));
				}
		};
	}
}

/**
 * @brief Declare the allocation-free flag mask conditionally DLL-safe.
 * @note Its Bitmask vtable and inline operations require a compatible provider ABI.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Features);
