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

#include <span>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::Tables::Encoder
 * @brief Handcrafted encoder implementations: preference, features, FineTune key and HDR signaling.
 */
namespace StormByte::Multimedia::Tables::Encoder {
	/**
	 * @struct EncoderDef
	 * @brief One FFmpeg encoder implementation for a StormByte codec identity.
	 *
	 * tune_key empty: FineTune pairs go to av_opt_set one by one.
	 * tune_key set: FineTune is packed into that private-options blob.
	 * signal_hdr10 / signal_hdr10plus: library-owned k=v[:k=v] when the Frame
	 * carries that metadata. Never quality knobs.
	 * Empty crf_key / bitrate_key / maxrate_key / bufsize_key / preset_key /
	 * style_key: that setter is not valid for the row.
	 */
	struct EncoderDef {
		const char* codec;
		const char* name;
		const char* description;
		int preference;
		Features features;
		const char* tune_key;
		const char* signal_hdr10;
		const char* signal_hdr10plus;
		const char* crf_key;
		const char* bitrate_key;
		const char* maxrate_key;
		const char* bufsize_key;
		const char* preset_key;
		const char* style_key;
	};

	/**
	 * @brief Video encoder rows.
	 * @return Span over the static video table.
	 */
	std::span<const EncoderDef> Video() noexcept;

	/**
	 * @brief Audio encoder rows.
	 * @return Span over the static audio table.
	 */
	std::span<const EncoderDef> Audio() noexcept;

	/**
	 * @brief Text subtitle encoder rows (ASS/SRT/WebVTT/…). Not bitmap, not OCR.
	 * @return Span over the static subtitle table.
	 */
	std::span<const EncoderDef> Subtitle() noexcept;
}
