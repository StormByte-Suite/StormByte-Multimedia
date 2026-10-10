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

#include <tables/container/table.hxx>

#include <string_view>
#include <unordered_map>

/**
 * @namespace StormByte::Multimedia::Tables::Container
 * @brief Private static container identity and codec-compatibility tables.
 */
namespace StormByte::Multimedia::Tables::Container {
	/**
	 * @class Catalog
	 * @brief Indexed view over container identity and compatibility rows.
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
			 * @brief All identity rows.
			 * @return Span from Identity().
			 */
			std::span<const ContainerDef> All() const noexcept;

			/**
			 * @brief Lookup by StormByte name.
			 * @param name Catalog key.
			 * @return Row, or nullptr.
			 */
			const ContainerDef* Find(std::string_view name) const noexcept;

			/**
			 * @brief Compatibility rows for a StormByte name.
			 * @param name Catalog key.
			 * @return Compat span; empty if unknown.
			 */
			std::span<const CompatDef> Compat(std::string_view name) const noexcept;

			/**
			 * @brief Compatibility rows for an identity row.
			 * @param def Catalog row.
			 * @return Compat span; empty if unknown.
			 */
			std::span<const CompatDef> Compat(const ContainerDef& def) const noexcept;

		private:
			struct NameHash {
				using is_transparent = void;
				std::size_t operator()(std::string_view s) const noexcept {
					return std::hash<std::string_view>{}(s);
				}
				std::size_t operator()(const char* s) const noexcept {
					return std::hash<std::string_view>{}(s ? std::string_view{s} : std::string_view{});
				}
			};

			Catalog() noexcept;

			void Initialize() noexcept;

			std::unordered_map<std::string_view, const ContainerDef*, NameHash, std::equal_to<>> m_byName;
			std::unordered_map<std::string_view, std::span<const CompatDef>, NameHash, std::equal_to<>> m_compat;
	};
}
