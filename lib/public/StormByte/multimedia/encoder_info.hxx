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
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <array>
#include <cstddef>
#include <string_view>

namespace StormByte::Multimedia {
	class Codec;

	/**
	 * @enum EncoderSetting
	 * @brief Rate and style settings an encoder implementation may accept.
	 *
	 * Each value maps to a `StormByte::Multimedia::Pipeline::Encoder` setter
	 * (and the matching `Transcoder::Track` setter). Using an unsupported
	 * setting makes the encoder fail instead of being ignored.
	 */
	enum class STORMBYTE_MULTIMEDIA_PUBLIC EncoderSetting: unsigned char {
		CRF,		///< Constant quality (CRF, CQ, QP, global_quality or audio qscale).
		BitRate,	///< Target bitrate.
		MaxBitRate,	///< VBV / maximum bitrate.
		Preset,		///< Speed / efficiency preset.
		Tune,		///< Content tuning.
		Params		///< Packed vendor parameters (`x265-params`, ...). When present, FineTune
					///< keys go into that blob; otherwise each FineTune key is an FFmpeg option.
	};

	/**
	 * @enum EncoderOptionType
	 * @brief Value type of an FFmpeg encoder option.
	 */
	enum class STORMBYTE_MULTIMEDIA_PUBLIC EncoderOptionType: unsigned char {
		Integer,	///< Integer value.
		Float,		///< Floating point value.
		Boolean,	///< Boolean value.
		String,		///< Free text.
		Other		///< Any other FFmpeg type (dictionary, rational, size, ...).
	};

	/**
	 * @struct EncoderOption
	 * @brief Description of one FFmpeg option of an encoder implementation.
	 * @note Values come from the linked FFmpeg build (bundled or system).
	 */
	struct STORMBYTE_MULTIMEDIA_PUBLIC EncoderOption {
		/**
		 * @brief Constructs an empty description in Multimedia.
		 */
		EncoderOption();

		/**
		 * @brief Copies a description in Multimedia.
		 * @param other Source description.
		 */
		EncoderOption(const EncoderOption& other);

		/**
		 * @brief Transfers a description.
		 * @param other Source description, left empty.
		 */
		EncoderOption(EncoderOption&& other) noexcept;

		/**
		 * @brief Releases the description through its creator callbacks.
		 */
		~EncoderOption() noexcept;

		/**
		 * @brief Copies a description in Multimedia.
		 * @param other Source description.
		 * @return This object.
		 */
		EncoderOption& operator=(const EncoderOption& other);

		/**
		 * @brief Transfers a description.
		 * @param other Source description, left empty.
		 * @return This object.
		 */
		EncoderOption& operator=(EncoderOption&& other) noexcept;

		StormByte::Safe::String Name;						///< FFmpeg option name.
		StormByte::Safe::String Help;						///< FFmpeg help text (English).
		EncoderOptionType Type;								///< Value type.
		double Minimum;										///< Lowest numeric value FFmpeg accepts.
		double Maximum;										///< Highest numeric value FFmpeg accepts.
		StormByte::Safe::String Default;					///< FFmpeg default as text; empty when unknown.
		StormByte::Safe::Vector<StormByte::Safe::String> Choices;	///< Named constants accepted as values.
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::EncoderOption);

namespace StormByte::Multimedia {
	/**
	 * @class EncoderInfo
	 * @brief Read-only description of one encoder implementation of a codec.
	 *
	 * Obtained from `Codec::Encoders()`. Describes which settings the
	 * implementation accepts and the native option behind each of them.
	 * Names are process-lifetime literals owned by Multimedia.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC EncoderInfo {
		public:
			/**
			 * @brief Constructs an empty description.
			 */
			constexpr EncoderInfo() noexcept = default;

			/**
			 * @brief Copy constructor.
			 * @param other Source description.
			 */
			constexpr EncoderInfo(const EncoderInfo& other) noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param other Source description.
			 */
			constexpr EncoderInfo(EncoderInfo&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~EncoderInfo() noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @param other Source description.
			 * @return This object.
			 */
			constexpr EncoderInfo& operator=(const EncoderInfo& other) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param other Source description.
			 * @return This object.
			 */
			constexpr EncoderInfo& operator=(EncoderInfo&& other) noexcept = default;

			/**
			 * @brief FFmpeg implementation name, usable as an encoder pin.
			 * @return Name, or empty for an empty description.
			 */
			constexpr std::string_view Name() const noexcept { return m_name; }

			/**
			 * @brief Human description.
			 * @return Description.
			 */
			constexpr std::string_view Description() const noexcept { return m_description; }

			/**
			 * @brief Features this implementation preserves or offers.
			 * @return Feature set.
			 */
			constexpr const StormByte::Multimedia::Features& Features() const noexcept { return m_features; }

			/**
			 * @brief Tests whether a setting is accepted.
			 * @param setting Setting to test.
			 * @return true when the matching setter is valid for this implementation.
			 */
			constexpr bool Supports(EncoderSetting setting) const noexcept {
				return !Key(setting).empty();
			}

			/**
			 * @brief Native FFmpeg option behind a setting.
			 * @param setting Setting to query.
			 * @return Option name (`crf`, `cq`, `qp`, `global_quality`, `b`, `x265-params`, ...),
			 * or empty when unsupported.
			 * @note Audio CRF on `global_quality` takes the encoder's native quality scale
			 * (libvorbis -1..10, libmp3lame 0..9); Multimedia applies the qscale conversion.
			 */
			constexpr std::string_view Key(EncoderSetting setting) const noexcept {
				const auto index = static_cast<std::size_t>(setting);
				return index < m_keys.size() ? m_keys[index] : std::string_view{};
			}

			/**
			 * @brief Describes an FFmpeg option of this implementation.
			 * @param name Private or generic codec option name.
			 * @return Description, or empty when the option or encoder does not exist.
			 * @note Packed parameters (inside `x265-params`, ...) are not FFmpeg options.
			 */
			StormByte::Safe::Optional<EncoderOption> Option(std::string_view name) const;

		private:
			/**
			 * @brief Codec builds descriptions from the encoder tables.
			 */
			friend class Codec;

			std::string_view m_name;								///< FFmpeg implementation name.
			std::string_view m_description;							///< Human description.
			StormByte::Multimedia::Features m_features;				///< Offered features.
			std::array<std::string_view, 6> m_keys{};				///< Native option per EncoderSetting.
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::EncoderInfo);
