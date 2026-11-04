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

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <iostream>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Expected the stereo WAV fixture path\n";
		return 1;
	}
	auto opened = StormByte::Multimedia::File::Open(StormByte::Safe::String{argv[1]});
	if (!opened) {
		std::cerr << "Installed Multimedia could not open the WAV fixture\n";
		return 1;
	}
	const auto streams = opened->Streams();
	if (streams.size() != 1 || streams[0].Type() != StormByte::Multimedia::Type::Audio
			|| streams[0].Codec().Name() != "PCM S16 LE") {
		std::cerr << "Installed Multimedia reported unexpected WAV streams\n";
		return 1;
	}
	const auto audio = streams[0].Audio();
	if (!audio || audio->Channels() != 2 || audio->SampleRate() != 48000) {
		std::cerr << "Installed Multimedia reported unexpected stereo audio properties\n";
		return 1;
	}
	if (!StormByte::Multimedia::Registry::Instance().FindCodec("PCM S16 LE")) {
		std::cerr << "Installed Multimedia did not expose the PCM codec registry\n";
		return 1;
	}
	return 0;
}
