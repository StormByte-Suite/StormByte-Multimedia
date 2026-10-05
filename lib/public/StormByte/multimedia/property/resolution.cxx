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

#include <StormByte/multimedia/property/resolution.hxx>

#include <string>

using namespace StormByte::Multimedia::Property;

Resolution::Resolution(std::uint32_t width, std::uint32_t height) noexcept:
m_width(width), m_height(height) {}

std::uint32_t Resolution::Width() const noexcept {
	return m_width;
}

std::uint32_t Resolution::Height() const noexcept {
	return m_height;
}

StormByte::Safe::String Resolution::Name() const {
	return StormByte::Safe::String(std::to_string(m_width) + "x" + std::to_string(m_height));
}

StormByte::Safe::String Resolution::StandardName() const {
	if (m_height > 2160)
		return StormByte::Safe::String("4K+");
	else if (m_height > 1080)
		return StormByte::Safe::String("4K");
	else if (m_height > 720)
		return StormByte::Safe::String("1080p");
	else if (m_height > 480)
		return StormByte::Safe::String("720p");
	else if (m_height > 240)
		return StormByte::Safe::String("480p");
	else
		return StormByte::Safe::String("240p");
}
