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

#include <StormByte/multimedia/property/channel_layout.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Property
 * @brief Media property value types.
 */
namespace StormByte::Multimedia::Property {
	/**
	 * @class Audio
	 * @brief Per-stream audio properties.
	 * @note DLL exchange requires compatible C++ ABIs. Base and Multimedia must
	 *       remain loaded while their values and provider callbacks are in use.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Audio final {
		public:
			/**
			 * @brief Constructs unknown audio properties with no codec profile.
			 * @throws StormByte::Exception Safe storage initialization failed.
			 */
			Audio();

			/**
			 * @brief Constructs audio properties.
			 * @param layout Speaker layout.
			 * @param sample_rate Sample rate in Hz.
			 * @param channels Channel count.
			 * @param bitrate Bitrate in bits per second (0 if unknown).
			 * @param profile Optional codec profile name.
			 */
			Audio(ChannelLayout layout, std::uint32_t sample_rate, std::uint8_t channels,
				std::uint64_t bitrate = 0, StormByte::Safe::Optional<StormByte::Safe::String> profile = std::nullopt) noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Properties to copy.
			 * @throws StormByte::Exception Safe storage copying failed.
			 */
			Audio(const Audio& other);

			/**
			 * @brief Move constructor.
			 * @param other Properties to move.
			 */
			Audio(Audio&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~Audio() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Properties to copy.
			 * @return *this.
			 * @throws StormByte::Exception Safe storage copying failed.
			 */
			Audio& operator=(const Audio& other);

			/**
			 * @brief Move assignment.
			 * @param other Properties to move.
			 * @return *this.
			 */
			Audio& operator=(Audio&& other) noexcept;

			/**
			 * @brief Speaker layout.
			 * @return Layout.
			 */
			ChannelLayout Layout() const noexcept;

			/**
			 * @brief Sample rate in Hz.
			 * @return Sample rate.
			 */
			std::uint32_t SampleRate() const noexcept;

			/**
			 * @brief Channel count.
			 * @return Channel count.
			 */
			std::uint8_t Channels() const noexcept;

			/**
			 * @brief Bitrate in bits per second.
			 * @return Bitrate, or 0 if unknown.
			 */
			std::uint64_t BitRate() const noexcept;

			/**
			 * @brief Codec profile name, if present.
			 * @return Borrowed optional profile, valid while this object is alive.
			 */
			const StormByte::Safe::Optional<StormByte::Safe::String>& Profile() const noexcept;

		private:
			ChannelLayout m_layout = ChannelLayout::Unknown;				///< Speaker layout, initialized to unknown.

			std::uint32_t m_sample_rate = 0;								///< Sample rate in Hz, initialized to zero.

			std::uint8_t m_channels = 0;									///< Channel count, initialized to zero.

			std::uint64_t m_bitrate = 0;									///< Bitrate in bits per second, initialized to zero.

			StormByte::Safe::Optional<StormByte::Safe::String> m_profile;	///< Optional Base-owned codec profile.
	};
}

/**
 * @brief Registers the completed audio description for Safe value storage.
 *
 * Profile text and optional storage use Base's heap and creator callbacks; value
 * lifetime operations are exported by Multimedia. Compatible provider ABI is
 * required and providers and callback creators must outlive their dependent values.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Property::Audio);
