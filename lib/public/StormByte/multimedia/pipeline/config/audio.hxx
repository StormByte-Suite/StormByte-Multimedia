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

#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/pipeline/config/base.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/map.hxx>

#include <cstdint>
#include <utility>

/**
 * @namespace StormByte::Multimedia::Pipeline::Config
 * @brief Per-track intention stored by Plan.
 *
 * Not wiring (`operator>>`) and not runtime settled state.
 * An engaged `StormByte::Safe::Optional` is an explicit override. Calling a
 * setter with an empty value is also an explicit override.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline::Config {
	/**
	 * @class Audio
	 * @brief Audio-track encoding intention, or Remux without a codec.
	 *
	 * Boundary use requires a compatible C++ ABI and loaded provider modules.
	 * Codec pointers borrow registry storage, which must outlive this config and
	 * its copies. Further derived providers must supply safe fields, provider-local
	 * lifecycle operations, virtual destruction and Clone/Move overrides that
	 * preserve their dynamic type; this declaration does not certify them.
	 * Optional arrow access yields a read-only full-expression snapshot, not
	 * mutable storage. Const map iteration yields entry copies, not references.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Audio: public Base {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Empty audio config (Remux until @ref Codec is set).
			 */
			Audio();

			/**
			 * @brief Copy constructor.
			 * @param other Source config.
			 */
			Audio(const Audio& other);

			/**
			 * @brief Move constructor.
			 * @param other Config to take.
			 */
			Audio(Audio&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			virtual ~Audio() noexcept override;

			/**
			 * @brief Copy assignment.
			 * @param other Source config.
			 * @return *this.
			 */
			Audio& operator=(const Audio& other);

			/**
			 * @brief Move assignment.
			 * @param other Config to take.
			 * @return *this.
			 */
			Audio& operator=(Audio&& other) noexcept;

			/**
			 * @}
			 */

			/**
			 * @brief Deep copy in Multimedia; the registry codec remains borrowed.
			 * @return Owning pointer to a new @ref Audio.
			 */
			PointerType Clone() const override;

			/**
			 * @brief Move into a new pointer in Multimedia.
			 * @return Owning pointer to the moved @ref Audio.
			 */
			PointerType Move() override;

			/**
			 * @name Encode
			 * @{
			 */

			/**
			 * @brief Destination codec.
			 * @return Registry codec, or `nullptr` if this track is Remux.
			 * @note The registry and its provider must remain alive while used.
			 */
			inline const StormByte::Multimedia::Codec* Codec() const noexcept {
				return m_codec;
			}

			/**
			 * @brief Sets the destination codec (Encode).
			 * @param codec Registry codec. Must be audio at Plan validation.
			 * @note Borrows the codec; its registry must outlive this config and copies.
			 */
			inline void Codec(const StormByte::Multimedia::Codec& codec) noexcept {
				m_codec = &codec;
			}

			/**
			 * @brief Target bitrate.
			 * @return Bits per second, or empty.
			 */
			inline const StormByte::Safe::Optional<std::int64_t>& BitRate() const noexcept {
				return m_bitRate;
			}

			/**
			 * @brief Sets target bitrate.
			 * @param bits_per_second Bits per second.
			 * @note Safe optional assignment may throw on storage failure.
			 */
			void BitRate(std::int64_t bits_per_second);

			/**
			 * @brief Maximum bitrate.
			 * @return Bits per second, or empty.
			 */
			inline const StormByte::Safe::Optional<std::int64_t>& MaxBitRate() const noexcept {
				return m_maxBitRate;
			}

			/**
			 * @brief Sets maximum bitrate.
			 * @param bits_per_second Max bitrate.
			 * @note Safe optional assignment may throw on storage failure.
			 */
			void MaxBitRate(std::int64_t bits_per_second);

			/**
			 * @brief Encoder preset, if the implementation has one.
			 * @return Name, or empty.
			 */
			inline const StormByte::Safe::Optional<StormByte::Safe::String>& Preset() const noexcept {
				return m_preset;
			}

			/**
			 * @brief Sets the preset. Empty clears it.
			 * @param name Preset name.
			 * @note Safe optional assignment may throw on storage failure.
			 */
			void Preset(StormByte::Safe::String name);

			/**
			 * @brief Vendor leftovers. Not bitrate / preset.
			 * @return Key/value map.
			 * @note Iterator dereference returns an entry copy, not a mutable reference.
			 */
			inline const StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>& FineTune() const noexcept {
				return m_fineTune;
			}

			/**
			 * @brief Replaces the vendor dict.
			 * @param options Key/value pairs.
			 */
			void FineTune(StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options) noexcept;

			/**
			 * @}
			 */

		private:
			const StormByte::Multimedia::Codec* m_codec;										///< Borrowed registry codec; nullptr = Remux
			StormByte::Safe::Optional<std::int64_t> m_bitRate;									///< Target bitrate override
			StormByte::Safe::Optional<std::int64_t> m_maxBitRate;								///< Maximum bitrate override
			StormByte::Safe::Optional<StormByte::Safe::String> m_preset;						///< Encoder preset override
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> m_fineTune;	///< Provider-owned vendor options
	};
}

/**
 * @brief Declares Audio conditionally safe under its documented provider contract.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Config::Audio);
