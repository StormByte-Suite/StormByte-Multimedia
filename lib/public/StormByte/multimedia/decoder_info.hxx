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

#include <StormByte/multimedia/features.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <string_view>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public media types and implementation descriptions.
	 */
	namespace Multimedia {
		/**
		 * @class Codec
		 * @brief Immutable codec identity and implementation discovery.
		 */
		class Codec;

		/**
		 * @class DecoderInfo
		 * @brief Read-only description of one decoder implementation of a codec.
		 *
		 * Obtained from StormByte::Multimedia::Codec::Decoders(). Names and
		 * descriptions refer to process-lifetime Multimedia or FFmpeg literals;
		 * the provider must remain loaded and use a compatible ABI.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC DecoderInfo {
			public:
				/**
				 * @brief Constructs an empty description.
				 */
				constexpr DecoderInfo() noexcept = default;

				/**
				 * @brief Copy constructor.
				 * @param other Source description.
				 */
				constexpr DecoderInfo(const DecoderInfo& other) noexcept = default;

				/**
				 * @brief Move constructor.
				 * @param other Source description.
				 */
				constexpr DecoderInfo(DecoderInfo&& other) noexcept = default;

				/**
				 * @brief Destructor.
				 */
				~DecoderInfo() noexcept = default;

				/**
				 * @brief Copy assignment.
				 * @param other Source description.
				 * @return This object.
				 */
				constexpr DecoderInfo& operator=(const DecoderInfo& other) noexcept = default;

				/**
				 * @brief Move assignment.
				 * @param other Source description.
				 * @return This object.
				 */
				constexpr DecoderInfo& operator=(DecoderInfo&& other) noexcept = default;

				/**
				 * @brief FFmpeg implementation name usable as a decoder pin.
				 * @return Name, or empty for an empty description.
				 */
				constexpr std::string_view Name() const noexcept { return m_name; }

				/**
				 * @brief Human description.
				 * @return Description.
				 */
				constexpr std::string_view Description() const noexcept { return m_description; }

				/**
				 * @brief Known decoder capabilities from Multimedia's implementation tables.
				 * @return Feature set; empty for an unclassified backend fallback.
				 */
				constexpr const StormByte::Multimedia::Features& Features() const noexcept { return m_features; }

			private:
				/**
				 * @brief Codec constructs implementation descriptions.
				 */
				friend class Codec;

				std::string_view m_name;						///< FFmpeg implementation name.
				std::string_view m_description;				///< Human description.
				StormByte::Multimedia::Features m_features;	///< Known decoder capabilities.
		};
	}
}

/**
 * @brief Declare DecoderInfo conditionally DLL-safe for Safe value containers.
 * @note Borrowed literals require the provider lifetime and a compatible ABI.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::DecoderInfo);
