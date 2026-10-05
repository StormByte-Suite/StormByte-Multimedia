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
#include <StormByte/multimedia/visibility.h>

#include <cstdint>

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
		 * @enum Type
		 * @brief Kind of media stream or codec.
		 */
		enum class STORMBYTE_MULTIMEDIA_PUBLIC Type {
			Audio,			///< Audio stream or codec
			Video,			///< Video stream or codec
			Subtitle,		///< Subtitle stream or codec
			Attachment,		///< Attachment / ancillary data
			Unknown			///< Unclassified type
		};

		/**
		 * @brief Converts a Type to a string literal.
		 * @param type Value to convert.
		 * @return Null-terminated name, or `"Invalid"`.
		 */
		constexpr const char* ToString(Type type) noexcept {
			switch (type) {
				case Type::Audio:		return "Audio";			///< Audio
				case Type::Video:		return "Video";			///< Video
				case Type::Subtitle:	return "Subtitle";		///< Subtitle
				case Type::Attachment:	return "Attachment";	///< Attachment
				case Type::Unknown:		return "Unknown";		///< Unknown
				default:				return "Invalid";		///< Out of range
			}
		}

		/**
		 * @enum Operation
		 * @brief Read / write capability flags for a codec.
		 */
		enum class STORMBYTE_MULTIMEDIA_PUBLIC Operation: std::uint8_t {
			None	= 0,		///< No access
			Read	= 1 << 0,	///< Decode / demux is available
			Write	= 1 << 1,	///< At least one encoder / muxer exists
			Attach	= 1 << 2	///< Container can hold real attachments (covers, fonts)
		};

		/**
		 * @class Access
		 * @brief Bitmask of Operation flags for a codec.
		 * @note Stores only enum flags and a Bitmask vtable, with no owning heap state.
		 * Conditional DLL safety requires compatible compiler, standard-library ABI
		 * and provider layout, with the multimedia provider loaded during use.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Access: public StormByte::Bitmask<Access, Operation> {
			public:
				/**
				 * @brief Empty mask.
				 */
				constexpr Access() noexcept
				: StormByte::Bitmask<Access, Operation>() {}

				/**
				 * @brief Mask from a single operation.
				 * @param op Initial flag.
				 */
				constexpr Access(Operation op) noexcept
				: StormByte::Bitmask<Access, Operation>(op) {}

				/**
				 * @brief Copy constructor.
				 * @param access Source mask.
				 */
				constexpr Access(const Access& access) noexcept = default;

				/**
				 * @brief Move constructor.
				 * @param access Source mask.
				 */
				constexpr Access(Access&& access) noexcept = default;

				/**
				 * @brief Destructor.
				 */
				constexpr ~Access() noexcept override = default;

				/**
				 * @brief Copy assignment.
				 * @param access Source mask.
				 * @return *this.
				 */
				constexpr Access& operator=(const Access& access) noexcept = default;

				/**
				 * @brief Move assignment.
				 * @param access Source mask.
				 * @return *this.
				 */
				constexpr Access& operator=(Access&& access) noexcept = default;
		};
	}
}

/**
 * @brief Declare the allocation-free access mask conditionally DLL-safe.
 * @note Its Bitmask vtable and inline operations require a compatible provider ABI.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Access);
