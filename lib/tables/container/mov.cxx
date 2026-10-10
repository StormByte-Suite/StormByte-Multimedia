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

#include <tables/container/table.hxx>

using namespace StormByte::Multimedia::Tables::Container;

namespace {
	constexpr CompatDef table[] = {
		{ nullptr,	"AV1" },
		{ nullptr,	"Apple Intermediate Codec" },
		{ nullptr,	"CineForm HD" },
		{ nullptr,	"Cinepak" },
		{ nullptr,	"DNxHD" },
		{ nullptr,	"DV Video" },
		{ nullptr,	"H.261" },
		{ nullptr,	"H.263" },
		{ nullptr,	"H.264" },
		{ nullptr,	"H.265" },
		{ nullptr,	"H.266" },
		{ nullptr,	"HAP" },
		{ nullptr,	"JPEG 2000" },
		{ nullptr,	"MJPEG" },
		{ nullptr,	"MPEG-1 Video" },
		{ nullptr,	"MPEG-2 Video" },
		{ nullptr,	"MPEG-4 Part 2" },
		{ nullptr,	"PNG" },
		{ nullptr,	"ProRes" },
		{ nullptr,	"ProRes RAW" },
		{ nullptr,	"QuickTime 8BPS" },
		{ nullptr,	"QuickTime Animation" },
		{ nullptr,	"QuickTime RPZA" },
		{ nullptr,	"QuickTime SMC" },
		{ nullptr,	"Sorenson Video 1" },
		{ nullptr,	"Sorenson Video 3" },
		{ nullptr,	"Ut Video" },
		{ nullptr,	"VP8" },
		{ nullptr,	"VP9" },

		{ nullptr,	"AAC" },
		{ nullptr,	"AAC LATM" },
		{ nullptr,	"AC-3" },
		{ nullptr,	"ADPCM IMA QuickTime" },
		{ nullptr,	"ALAC" },
		{ nullptr,	"AMR-NB" },
		{ nullptr,	"AMR-WB" },
		{ nullptr,	"E-AC3" },
		{ nullptr,	"FLAC" },
		{ nullptr,	"GSM" },
		{ nullptr,	"MACE 3:1" },
		{ nullptr,	"MACE 6:1" },
		{ nullptr,	"MP1" },
		{ nullptr,	"MP2" },
		{ nullptr,	"MP3" },
		{ nullptr,	"Opus" },
		{ nullptr,	"PCM A-law" },
		{ nullptr,	"PCM F32 BE" },
		{ nullptr,	"PCM F32 LE" },
		{ nullptr,	"PCM S16 BE" },
		{ nullptr,	"PCM S16 LE" },
		{ nullptr,	"PCM S24 BE" },
		{ nullptr,	"PCM S24 LE" },
		{ nullptr,	"PCM S32 BE" },
		{ nullptr,	"PCM S32 LE" },
		{ nullptr,	"PCM S8" },
		{ nullptr,	"PCM μ-law" },
		{ nullptr,	"QDM2" },
		{ nullptr,	"QDMC" },
		{ nullptr,	"TrueHD" },

		{ nullptr,	"3GPP Timed Text" },
		{ nullptr,	"EIA-608" },
		{ nullptr,	"TTML" },
		{ nullptr,	"WebVTT" },
	};
}

std::span<const CompatDef> StormByte::Multimedia::Tables::Container::MOV() noexcept {
	return table;
}
