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

#include <StormByte/multimedia/pipeline/config/video.hxx>

#include <utility>

using namespace StormByte::Multimedia::Pipeline::Config;

Video::Video(): Base(StormByte::Multimedia::Type::Video), m_codec(nullptr) {}

Video::Video(const Video& other) = default;

Video::Video(Video&& other) noexcept = default;

Video::~Video() noexcept = default;

Video& Video::operator=(const Video& other) = default;

Video& Video::operator=(Video&& other) noexcept = default;

Video::PointerType Video::Clone() const {
	return MakePointer<Video>(*this);
}

Video::PointerType Video::Move() {
	return MakePointer<Video>(std::move(*this));
}

void Video::CRF(int value) {
	m_crf = value;
	m_bitRate.reset();
}

void Video::BitRate(std::int64_t bits_per_second) {
	m_bitRate = bits_per_second;
	m_crf.reset();
}

void Video::MaxBitRate(std::int64_t bits_per_second) {
	m_maxBitRate = bits_per_second;
}

void Video::Preset(StormByte::Safe::String name) {
	if (name.empty())
		m_preset.reset();
	else
		m_preset = std::move(name);
}

void Video::Tune(StormByte::Safe::String name) {
	if (name.empty())
		m_tune.reset();
	else
		m_tune = std::move(name);
}

void Video::FineTune(StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options) noexcept {
	m_fineTune = std::move(options);
}
