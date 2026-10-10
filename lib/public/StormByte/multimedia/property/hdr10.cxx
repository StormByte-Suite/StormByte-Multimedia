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

#include <StormByte/multimedia/property/hdr10.hxx>

using namespace StormByte::Multimedia::Property;

const HDR10 HDR10::DEFAULT = {
	{34000, 16000}, {13250, 34500}, {7500, 3000}, {15635, 16450}, {1, 10000000},
	std::nullopt, HDR10::Source::Heuristics
};

HDR10::HDR10():
m_red(34000, 16000), m_green(13250, 34500), m_blue(7500, 3000),
m_white(15635, 16450), m_luminance(1, 10000000),
m_source(Source::Heuristics), m_hdr10plus(false) {}

HDR10::HDR10(const Point& red, const Point& green, const Point& blue, const Point& white,
	const Point& luminance, const StormByte::Safe::Optional<Point>& light_level, Source source):
m_red(red), m_green(green), m_blue(blue), m_white(white),
m_luminance(luminance), m_light_level(light_level), m_source(source), m_hdr10plus(false) {}

HDR10::HDR10(Point&& red, Point&& green, Point&& blue, Point&& white,
	Point&& luminance, StormByte::Safe::Optional<Point>&& light_level, Source source) noexcept:
m_red(std::move(red)), m_green(std::move(green)), m_blue(std::move(blue)),
m_white(std::move(white)), m_luminance(std::move(luminance)), m_light_level(std::move(light_level)),
m_source(source), m_hdr10plus(false) {}

HDR10::HDR10(const HDR10& other) = default;

HDR10::HDR10(HDR10&& other) noexcept = default;

HDR10::~HDR10() noexcept = default;

HDR10& HDR10::operator=(const HDR10& other) = default;

HDR10& HDR10::operator=(HDR10&& other) noexcept = default;

const Point& HDR10::Red() const noexcept {
	return m_red;
}

const Point& HDR10::Green() const noexcept {
	return m_green;
}

const Point& HDR10::Blue() const noexcept {
	return m_blue;
}

const Point& HDR10::White() const noexcept {
	return m_white;
}

const Point& HDR10::Luminance() const noexcept {
	return m_luminance;
}

const StormByte::Safe::Optional<Point>& HDR10::LightLevel() const noexcept {
	return m_light_level;
}

HDR10::Source HDR10::Origin() const noexcept {
	return m_source;
}

bool HDR10::IsHDR10Plus() const noexcept {
	return m_hdr10plus;
}

void HDR10::HDR10Plus(bool hdrplus) noexcept {
	m_hdr10plus = hdrplus;
}
