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
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/typedefs.hxx>

#include <functional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

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
		 * @namespace StormByte::Multimedia::Tables
		 * @brief Private codec and container identity table declarations.
		 */
		namespace Tables {
			/**
			 * @namespace StormByte::Multimedia::Tables::Codec
			 * @brief Codec identity table declarations.
			 */
			namespace Codec {
				/**
				 * @struct CodecDef
				 * @brief Codec identity table row.
				 */
				struct CodecDef;
			}
			/**
			 * @namespace StormByte::Multimedia::Tables::Container
			 * @brief Container identity table declarations.
			 */
			namespace Container {
				/**
				 * @struct ContainerDef
				 * @brief Container identity table row.
				 */
				struct ContainerDef;
			}
		}

		/**
		 * @class Registry
		 * @brief Process-wide catalog of codecs and containers.
		 *
		 * First Instance() loads the private tables and probes FFmpeg.
		 * @note Conditional DLL safety requires compatible compiler, standard-library
		 * ABI and provider layout. The singleton and its borrowed identities must not
		 * be destroyed by callers; the multimedia provider must remain loaded during use.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Registry {
			public:
				/**
				 * @brief Copy is disabled.
				 * @param other Source registry.
				 */
				Registry(const Registry& other) = delete;

				/**
				 * @brief Move is disabled.
				 * @param other Source registry.
				 */
				Registry(Registry&& other) = delete;

				/**
				 * @brief Destroys provider-owned storage in the multimedia library.
				 */
				~Registry() noexcept;

				/**
				 * @brief Copy assignment is disabled.
				 * @param other Source registry.
				 * @return *this.
				 */
				Registry& operator=(const Registry& other) = delete;

				/**
				 * @brief Move assignment is disabled.
				 * @param other Source registry.
				 * @return *this.
				 */
				Registry& operator=(Registry&& other) = delete;

				/**
				 * @brief Process-wide instance.
				 * @return The singleton.
				 */
				static Registry& Instance() noexcept;

				/**
				 * @brief Codecs of one Type.
				 * @param type Kind to list.
				 * @return Safe-owned list of nonempty handles into registry storage.
				 * @note Handles do not own codecs and expire at registry teardown or unload.
				 */
				CodecRefs CodecList(Type type) const noexcept;

				/**
				 * @brief All loaded containers.
				 * @return Safe-owned list of nonempty handles into registry storage.
				 * @note Handles do not own containers and expire at registry teardown or unload.
				 */
				ContainerRefs ContainerList() const noexcept;

				/**
				 * @brief Looks up by StormByte name or FFmpeg codec id.
				 * @param name Key (`H.265` or `hevc`).
				 * @return Nonempty borrowed CodecRef or CodecNotFoundException.
				 * @note The codec remains registry-owned and expires at registry teardown or unload.
				 */
				ExpectedCodec FindCodec(std::string_view name) const noexcept;

				/**
				 * @brief Looks up by StormByte name or FFmpeg format id.
				 * @param name Key (`Matroska` or `matroska`).
				 * @return Nonempty borrowed ContainerRef or ContainerNotFoundException.
				 * @note The container remains registry-owned and expires at registry teardown or unload.
				 */
				ExpectedContainer FindContainer(std::string_view name) const noexcept;

			private:
				/**
				 * @struct NameHash
				 * @brief Transparent hasher for string_view map keys.
				 */
				struct NameHash {
					/**
					 * @typedef is_transparent
					 * @brief Enables heterogeneous lookup.
					 */
					using is_transparent = void;

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
				 * @brief Loads tables and probes FFmpeg.
				 */
				Registry() noexcept;

				/**
				 * @brief Reserves storage and loads every table.
				 */
				void Initialize() noexcept;

				/**
				 * @brief Loads codecs and containers.
				 */
				void Load() noexcept;

				/**
				 * @brief Loads every codec identity table.
				 */
				void LoadCodecs() noexcept;

				/**
				 * @brief Inserts every row of @p table as @p type.
				 * @param type Media kind of the table.
				 * @param table Rows to load.
				 */
				void LoadCodecs(Type type, std::span<const Tables::Codec::CodecDef> table) noexcept;

				/**
				 * @brief Inserts one codec and its FFmpeg ids.
				 * @param type Media kind.
				 * @param def Table row.
				 */
				void Add(Type type, const Tables::Codec::CodecDef& def) noexcept;

				/**
				 * @brief Loads every container identity row and its compatibility set.
				 */
				void LoadContainers() noexcept;

				/**
				 * @brief Inserts one container, its FFmpeg ids and allowed codecs.
				 * @param def Identity row.
				 */
				void Add(const Tables::Container::ContainerDef& def) noexcept;

				/**
				 * @brief Probes demuxer and muxer for @p def.
				 * @param def Identity row.
				 * @return Read and optional Write.
				 */
				Access ProbeContainer(const Tables::Container::ContainerDef& def) const noexcept;

				std::vector<Codec> m_codecs;																		///< Owned codec instances
				std::unordered_map<std::string_view, std::size_t, NameHash, std::equal_to<>> m_by_name;				///< Codec name / FFmpeg id → index
				std::unordered_map<Type, std::vector<std::size_t>> m_by_type;										///< Type → codec indices
				std::vector<Container> m_containers;																	///< Owned container instances
				std::unordered_map<std::string_view, std::size_t, NameHash, std::equal_to<>> m_container_by_name;	///< Container name / FFmpeg id → index
		};
	}
}

/**
 * @brief Declare the provider-owned singleton conditionally DLL-safe.
 * @note Its STL storage is private and operated on by the loaded provider;
 * consumers require a compatible compiler, standard-library ABI and layout.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Registry);
