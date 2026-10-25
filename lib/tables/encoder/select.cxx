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

#include <tables/encoder/table.hxx>

extern "C" {
	#include <libavcodec/avcodec.h>
}

using namespace StormByte::Multimedia;

namespace {
	const Tables::Encoder::EncoderDef* ScanRows(
		std::span<const Tables::Encoder::EncoderDef> rows,
		std::string_view codec, std::string_view pin, bool matchNeed,
		const Features& need) noexcept {
		const Tables::Encoder::EncoderDef* best = nullptr;
		for (const auto& row : rows) {
			if (codec != row.codec)
				continue;
			if (!pin.empty() && pin != row.name)
				continue;
			if (matchNeed && !row.features.Has(need))
				continue;
			if (matchNeed && avcodec_find_encoder_by_name(row.name) == nullptr)
				continue;
			if (!matchNeed)
				return &row;
			if (!best || row.preference < best->preference)
				best = &row;
		}

		return best;
	}
}

const Tables::Encoder::EncoderDef* Tables::Encoder::FindRow(
	std::string_view codec, std::string_view pin) noexcept {
	if (const auto* row = ScanRows(Video(), codec, pin, false, {}))
		return row;
	if (const auto* row = ScanRows(Audio(), codec, pin, false, {}))
		return row;
	return ScanRows(Subtitle(), codec, pin, false, {});
}

const Tables::Encoder::EncoderDef* Tables::Encoder::PickEncoder(
	std::string_view codec, std::string_view pin, const Features& need) noexcept {
	if (const auto* row = ScanRows(Video(), codec, pin, true, need))
		return row;
	if (const auto* row = ScanRows(Audio(), codec, pin, true, need))
		return row;
	return ScanRows(Subtitle(), codec, pin, true, need);
}
