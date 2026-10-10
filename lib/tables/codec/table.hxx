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

#include <StormByte/size.hxx>
#include <StormByte/multimedia/type.hxx>

#include <array>
#include <cstddef>
#include <span>

/**
 * @namespace StormByte::Multimedia::Tables::Codec
 * @brief Private static codec identity tables.
 */
namespace StormByte::Multimedia::Tables::Codec {
	/**
	 * @struct CodecDef
	 * @brief One identity row.
	 */
	struct STORMBYTE_MULTIMEDIA_PRIVATE CodecDef {
		const char* name;						///< StormByte name
		const char* description;				///< Description
		std::array<const char*, 4> ffmpegIds;	///< FFmpeg ids; unused slots nullptr

		/**
		 * @brief Number of non-null FFmpeg ids.
		 * @return Count in `[0, 4]`.
		 */
		constexpr StormByte::Size FfmpegIdCount() const noexcept {
			StormByte::Size n = 0;
			for (const char* id : ffmpegIds) {
				if (!id)
					break;
				++n;
			}
			return n;
		}

		/**
		 * @brief FFmpeg id at @p index.
		 * @param index Zero-based index.
		 * @return Id, or nullptr if out of range.
		 */
		constexpr const char* FfmpegId(StormByte::Size index) const noexcept {
			if (index >= FfmpegIdCount())
				return nullptr;
			return ffmpegIds[index];
		}
	};

	/**
	 * @brief Video identity rows.
	 * @return Span over the video table.
	 */
	std::span<const CodecDef> Video() noexcept;

	/**
	 * @brief Audio identity rows.
	 * @return Span over the audio table.
	 */
	std::span<const CodecDef> Audio() noexcept;

	/**
	 * @brief Subtitle identity rows.
	 * @return Span over the subtitle table.
	 */
	std::span<const CodecDef> Subtitle() noexcept;

	/**
	 * @brief Attachment identity rows.
	 * @return Span over the attachment table.
	 */
	std::span<const CodecDef> Attachment() noexcept;

	/**
	 * @brief Identity rows of one media kind.
	 * @param type Video, Audio, Subtitle or Attachment.
	 * @return Span over that table; empty if @p type has none.
	 */
	std::span<const CodecDef> Identity(Type type) noexcept;
}
