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

#include <StormByte/safe/clonable.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>

#include <optional>
#include <string>
#include <utility>

/**
 * @namespace StormByte::Multimedia::Pipeline::Config
 * @brief Per-track intention stored by Plan.
 *
 * Not wiring (`operator>>`) and not runtime settled state.
 * An engaged `std::optional` is an explicit override. Calling a
 * setter with an empty value is also an explicit override.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline::Config {
	/**
	 * @struct Implementation
	 * @brief Optional decoder / encoder names for this track.
	 *
	 * Either side may be absent: the corresponding Step picks
	 * its default. Remux reads neither side. This is a pin, not
	 * a Feature mask (that lives on Transcode).
	 *
	 * @ingroup multimedia_pipeline
	 */
	struct STORMBYTE_MULTIMEDIA_PUBLIC Implementation {
		std::optional<std::string> Decoder;	///< Decode pin; empty = default
		std::optional<std::string> Encoder;	///< Encode pin; empty = default
	};

	/**
	 * @class Base
	 * @brief Common destination-stream identity.
	 *
	 * Polymorphic store root (`StormByte::Safe::Clonable` with
	 * `StormByte::Safe::Unique`). Holds tags and implementation pins that
	 * apply to every media `StormByte::Multimedia::Type` used
	 * in a Plan slot. `Type` is stored at construction; it is
	 * not virtual. Oficio knobs live on the leaves.
	 *
	 * A null destination codec on a leaf means Remux.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Base:
		public StormByte::Safe::Clonable<Base, StormByte::Safe::Unique<Base>> {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Copy constructor.
			 * @param other Source config.
			 */
			Base(const Base& other) noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param other Config to take.
			 */
			Base(Base&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			virtual ~Base() noexcept override = default;

			/**
			 * @brief Copy assignment.
			 * @param other Source config.
			 * @return *this.
			 */
			Base& operator=(const Base& other) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param other Config to take.
			 * @return *this.
			 */
			Base& operator=(Base&& other) noexcept = default;

			/**
			 * @}
			 */

			/**
			 * @name Identity
			 * @{
			 */

			/**
			 * @brief Media stamped by the leaf constructor.
			 * @return Value passed to @ref Base(Type).
			 */
			inline constexpr enum StormByte::Multimedia::Type Type() const noexcept {
				return m_type;
			}

			/**
			 * @}
			 */

			/**
			 * @name Tags
			 * @{
			 */

			/**
			 * @brief Language override.
			 * @return ISO tag, or empty to inherit.
			 */
			inline const std::optional<std::string>& Language() const noexcept {
				return m_language;
			}

			/**
			 * @brief Sets the language override.
			 * @param language ISO tag. Empty clears the override.
			 */
			void Language(std::string language) noexcept;

			/**
			 * @brief Title override.
			 * @return Title, or empty to inherit.
			 */
			inline const std::optional<std::string>& Title() const noexcept {
				return m_title;
			}

			/**
			 * @brief Sets the title override.
			 * @param title Title. Empty clears the override.
			 */
			void Title(std::string title) noexcept;

			/**
			 * @brief Default-disposition override.
			 * @return Engaged value, or empty to inherit.
			 */
			inline const std::optional<bool>& Default() const noexcept {
				return m_default;
			}

			/**
			 * @brief Sets or clears the default-disposition override.
			 * @param value `true` / `false` to stamp, empty to inherit.
			 */
			inline void Default(std::optional<bool> value) noexcept {
				m_default = value;
			}

			/**
			 * @brief Forced-disposition override.
			 * @return Engaged value, or empty to inherit.
			 */
			inline const std::optional<bool>& Forced() const noexcept {
				return m_forced;
			}

			/**
			 * @brief Sets or clears the forced-disposition override.
			 * @param value `true` / `false` to stamp, empty to inherit.
			 */
			inline void Forced(std::optional<bool> value) noexcept {
				m_forced = value;
			}

			/**
			 * @}
			 */

			/**
			 * @name Implementation
			 * @{
			 */

			/**
			 * @brief Decoder / encoder pins.
			 * @return Pair; each side empty means Step default.
			 */
			inline const struct Implementation& Implementation() const noexcept {
				return m_implementation;
			}

			/**
			 * @brief Replaces both pins.
			 * @param implementation Decoder / encoder names. Empty sides = default.
			 */
			inline void Implementation(struct Implementation implementation) noexcept {
				m_implementation = std::move(implementation);
			}

			/**
			 * @}
			 */

		protected:
			/**
			 * @brief Empty overrides; media fixed for the leaf lifetime.
			 * @param type `StormByte::Multimedia::Type` of the derived class.
			 */
			explicit constexpr Base(enum StormByte::Multimedia::Type type) noexcept
			: m_type(type) {}

		private:
			enum StormByte::Multimedia::Type m_type;	///< Media of the leaf
			std::optional<std::string> m_language;		///< Language tag override
			std::optional<std::string> m_title;			///< Title override
			std::optional<bool> m_default;				///< Default disposition override
			std::optional<bool> m_forced;				///< Forced disposition override
			struct Implementation m_implementation;		///< Decode / encode pins
	};
}
