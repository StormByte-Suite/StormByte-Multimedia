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

#include <tables/codec/table.hxx>
#include <StormByte/multimedia/type.hxx>

#include <string_view>
#include <unordered_map>

/**
 * @namespace StormByte::Multimedia::Tables::Codec
 * @brief Private static codec identity tables.
 */
namespace StormByte::Multimedia::Tables::Codec {
	/**
	 * @class Catalog
	 * @brief Indexed view over codec identity rows.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Catalog {
		public:
			Catalog(const Catalog&) = delete;
			Catalog(Catalog&&) = delete;
			~Catalog() noexcept = default;
			Catalog& operator=(const Catalog&) = delete;
			Catalog& operator=(Catalog&&) = delete;

			/**
			 * @brief Process-lifetime catalog.
			 * @return Singleton.
			 */
			static const Catalog& Instance() noexcept;

			/**
			 * @brief Identity rows of one media kind.
			 * @param type Video, Audio, Subtitle or Attachment.
			 * @return Span from Identity(@p type).
			 */
			std::span<const CodecDef> All(Type type) const noexcept;

			/**
			 * @brief Lookup by StormByte name or FFmpeg id.
			 * @param name Catalog key (`H.265` or `hevc`).
			 * @return Row, or nullptr.
			 */
			const CodecDef* Find(std::string_view name) const noexcept;

			/**
			 * @brief Media kind of a name or FFmpeg id.
			 * @param name Catalog key.
			 * @return Type, or Type::Unknown.
			 */
			Type Kind(std::string_view name) const noexcept;

		private:
			/**
			 * @struct NameHash
			 * @brief Transparent hasher for string_view keys.
			 */
			struct NameHash {
				using is_transparent = void;	///< Heterogeneous lookup

				/**
				 * @brief Hashes a view.
				 * @param view Key.
				 * @return Hash.
				 */
				std::size_t operator()(std::string_view view) const noexcept {
					return std::hash<std::string_view>{}(view);
				}
			};

			/**
			 * @brief Builds the name maps.
			 */
			Catalog() noexcept;

			/**
			 * @brief Indexes every identity table.
			 */
			void Initialize() noexcept;

			/**
			 * @brief Indexes one table.
			 * @param type Media kind.
			 * @param table Rows.
			 */
			void Index(Type type, std::span<const CodecDef> table) noexcept;

			std::unordered_map<std::string_view, const CodecDef*, NameHash, std::equal_to<>> m_byName;	///< Name / FFmpeg id → row
			std::unordered_map<std::string_view, Type, NameHash, std::equal_to<>> m_kind;			///< StormByte name → kind
	};
}
