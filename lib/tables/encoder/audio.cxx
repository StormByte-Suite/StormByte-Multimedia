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

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Tables::Encoder;

namespace {
	constexpr EncoderDef table[] = {
		{ "AAC", "aac", "Native AAC encoder", 0, Feature::LowDelay, "", "", "", "", "b", "", "", "", "" },
		{ "AAC", "libfdk_aac", "Fraunhofer FDK AAC", 1, Feature::HighQuality | Feature::LowDelay | Feature::ProfileBased | Feature::SurroundSound, "", "", "", "", "b", "", "", "", "" },
		{ "Vorbis", "libvorbis", "libvorbis encoder", 0, Feature::HighQuality | Feature::LowDelay | Feature::SurroundSound, "", "", "", "", "b", "", "", "", "" },
		{ "Opus", "libopus", "libopus encoder", 0, Feature::HighQuality | Feature::LowDelay | Feature::SurroundSound, "", "", "", "", "b", "", "", "", "" },
		{ "MP3", "libmp3lame", "LAME MP3 encoder", 0, Feature::HighQuality | Feature::LowDelay, "", "", "", "", "b", "", "", "", "" },
		{ "FLAC", "flac", "FLAC encoder", 0, Feature::HighQuality | Feature::Lossless | Feature::SurroundSound, "", "", "", "", "", "", "", "", "" },
		{ "ALAC", "alac", "ALAC encoder", 0, Feature::HighQuality | Feature::Lossless | Feature::SurroundSound, "", "", "", "", "", "", "", "", "" },
		{ "AC-3", "ac3", "Native AC-3 encoder", 0, Feature::LowDelay | Feature::SurroundSound, "", "", "", "", "b", "", "", "", "" },
		{ "AC-3", "ac3_fixed", "Fixed-point AC-3 encoder", 1, Feature::LowDelay | Feature::SurroundSound, "", "", "", "", "b", "", "", "", "" },
		{ "E-AC3", "eac3", "Native E-AC3 encoder", 0, Feature::LowDelay | Feature::SurroundSound, "", "", "", "", "b", "", "", "", "" },
	};
}

std::span<const EncoderDef> StormByte::Multimedia::Tables::Encoder::Audio() noexcept {
	return table;
}
