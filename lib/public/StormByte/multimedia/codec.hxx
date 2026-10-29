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

#include <StormByte/multimedia/decoder_info.hxx>
#include <StormByte/multimedia/encoder_info.hxx>
#include <StormByte/multimedia/features.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <string_view>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Public media types: codecs, registry and stream kinds.
	 */
	namespace Multimedia {
		/**
		 * @class Registry
		 * @brief Process-wide catalog of codecs and containers.
		 */
		class Registry;

		/**
		 * @enum SubtitleKind
		 * @brief Semantic subtitle representation, not its on-disk encoding.
		 */
		enum class STORMBYTE_MULTIMEDIA_PUBLIC SubtitleKind {
			Unknown,	///< Subtitle without a reliable representation classification.
			Text,		///< Text-based cues, including binary-packed text formats.
			Bitmap		///< Image-based cues.
		};

		/**
		 * @class Codec
		 * @brief Immutable codec identity owned by Registry.
		 *
		 * Name() is the StormByte key. FFmpeg ids live only in the registry map.
		 */
		class STORMBYTE_MULTIMEDIA_PUBLIC Codec {
			public:
				/**
				 * @brief Copy is disabled; instances are unique in the registry.
				 * @param other Source codec.
				 */
				Codec(const Codec& other) noexcept = delete;

				/**
				 * @brief Move constructor executed by the multimedia provider.
				 * @param other Source codec.
				 */
				Codec(Codec&& other) noexcept;

				/**
				 * @brief Destroy the identity in the multimedia provider.
				 */
				~Codec() noexcept;

				/**
				 * @brief Copy assignment is disabled.
				 * @param other Source codec.
				 * @return *this.
				 */
				Codec& operator=(const Codec& other) noexcept = delete;

				/**
				 * @brief Move assignment executed by the multimedia provider.
				 * @param other Source codec.
				 * @return *this.
				 */
				Codec& operator=(Codec&& other) noexcept;

				/**
				 * @brief Identity equality (same registry slot).
				 * @param other Other codec.
				 * @return true if both refer to the same instance.
				 */
				bool operator==(const Codec& other) const noexcept;

				/**
				 * @brief Identity inequality.
				 * @param other Other codec.
				 * @return true if they are different instances.
				 */
				bool operator!=(const Codec& other) const noexcept;

				/**
				 * @brief Media kind of this codec.
				 * @return Type value.
				 */
				constexpr enum Type Type() const noexcept { return m_type; }

				/**
				 * @brief StormByte codec name.
				 * @return View to a process-lifetime literal.
				 */
				constexpr std::string_view Name() const noexcept { return m_name; }

				/**
				 * @brief Human description.
				 * @return View to a process-lifetime literal.
				 */
				constexpr std::string_view Description() const noexcept { return m_description; }

				/**
				 * @brief Classifies subtitle content independently of codec availability.
				 * @return Empty for non-subtitle codecs; Text, Bitmap or Unknown otherwise.
				 * @note Describes the format, not decoder output or OCR availability.
				 * Binary-packed text such as mov_text is Text, not Bitmap.
				 */
				StormByte::Safe::Optional<enum SubtitleKind> SubtitleKind() const noexcept;

				/**
				 * @brief Tests Read/Write flags.
				 * @param access Flags to test.
				 * @return true if every bit in @p access is set.
				 */
				bool HasAccess(Access access) const noexcept;

				/**
				 * @brief Tests access while preserving all requested features.
				 * @param access Flags to test; Read and Write are checked independently.
				 * @param need Features required from a single implementation per direction.
				 * @return true if the generic access flags are set and each requested
				 * direction has an available implementation whose table row covers @p need.
				 * @note Empty features are identical to HasAccess(access). Nonempty
				 * features use StormByte codec names and FFmpeg implementation names,
				 * never the generic FFmpeg codec-id fallback. Missing rows return false.
				 * This does not probe hardware devices, settings or container compatibility.
				 */
				bool HasAccess(Access access, const Features& need) const noexcept;

				/**
				 * @brief Lists the available encoder implementations for this codec.
				 * @param need Features each listed implementation must preserve; empty lists all.
				 * @return Implementations ordered by preference. The first one is the
				 * implementation Multimedia picks when no encoder is pinned and @p need
				 * is the required feature set. Empty when the codec is not writable.
				 * @note Availability means the linked FFmpeg provides the encoder; hardware
				 * devices are not probed.
				 */
				StormByte::Safe::Vector<EncoderInfo> Encoders(const Features& need = {}) const;

				/**
				 * @brief Lists decoder implementations usable by this codec's backend.
				 * @param need Input capabilities required from each implementation; empty lists all.
				 * @return Matching implementations with FFmpeg's unpinned default first,
				 * followed by table alternatives in stable preference order. Empty when
				 * no matching decoder is available.
				 * @note Names can pin the decoder, not the encoder. Each pin must match
				 * the source stream's codec id. Hardware-only and experimental decoders
				 * are excluded because the backend does not configure their prerequisites.
				 * This includes FFmpeg's native hardware-only `av1` decoder even when
				 * its capability flags do not identify the hardware requirement.
				 * Unclassified generic and subtitle defaults have empty features and only
				 * satisfy empty requirements. Filtering never changes the unpinned backend
				 * default: pin the returned name to select a filtered alternative.
				 * Availability does not guarantee that damaged or unsupported input opens.
				 */
				StormByte::Safe::Vector<DecoderInfo> Decoders(const Features& need = {}) const;

			private:
				/**
				 * @brief Registry creates codec identities from static tables.
				 */
				friend class Registry;

				enum Type m_type;				///< Stream or codec kind.
				std::string_view m_name;			///< Non-owning StormByte name backed by a process-lifetime literal.
				std::string_view m_description;	///< Non-owning description backed by a process-lifetime literal.
				Access m_access;				///< Read and optional write capabilities.

				/**
				 * @brief Registry-only constructor executed by the multimedia provider.
				 * @param type Media kind.
				 * @param name StormByte name (table literal).
				 * @param description Description (table literal).
				 * @param access Capability mask.
				 */
				Codec(enum Type type, std::string_view name, std::string_view description, Access access) noexcept;
		};
	}
}

/**
 * @brief Declare Codec conditionally DLL-safe before use by Safe owners.
 * @note Names and descriptions refer to provider literals; compatible ABI and
 * provider lifetime are required. Codec is noncopyable and is not a Safe value
 * for direct Optional, Map or Vector storage.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Codec);
