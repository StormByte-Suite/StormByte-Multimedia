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

#include <StormByte/multimedia/pipeline/config/audio.hxx>

#include <utility>

using namespace StormByte::Multimedia::Pipeline::Config;

Audio::Audio(): Base(StormByte::Multimedia::Type::Audio), m_codec(nullptr) {}

Audio::Audio(const Audio& other) = default;

Audio::Audio(Audio&& other) noexcept = default;

Audio::~Audio() noexcept = default;

Audio& Audio::operator=(const Audio& other) = default;

Audio& Audio::operator=(Audio&& other) noexcept = default;

Audio::PointerType Audio::Clone() const {
	return MakePointer<Audio>(*this);
}

Audio::PointerType Audio::Move() {
	return MakePointer<Audio>(std::move(*this));
}

void Audio::BitRate(std::int64_t bits_per_second) {
	m_bitRate = bits_per_second;
}

void Audio::MaxBitRate(std::int64_t bits_per_second) {
	m_maxBitRate = bits_per_second;
}

void Audio::Preset(StormByte::Safe::String name) {
	if (name.empty())
		m_preset.reset();
	else
		m_preset = std::move(name);
}

void Audio::FineTune(StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options) noexcept {
	m_fineTune = std::move(options);
}
