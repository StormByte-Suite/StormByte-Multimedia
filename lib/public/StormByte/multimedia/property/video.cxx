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

#include <StormByte/multimedia/property/video.hxx>

using namespace StormByte::Multimedia::Property;

Video::Video() = default;

Video::Video(class Color color, class Resolution resolution,
	StormByte::Safe::Optional<class HDR10> hdr10, StormByte::Safe::Optional<AVRational> frameRate,
	StormByte::Safe::Optional<AVRational> sampleAspectRatio,
	StormByte::Safe::Optional<class DOVI> dovi) noexcept:
	m_color(std::move(color)),
	m_resolution(std::move(resolution)),
	m_hdr10(std::move(hdr10)),
	m_frameRate(std::move(frameRate)),
	m_sar(std::move(sampleAspectRatio)),
	m_dovi(std::move(dovi)) {
	if (m_frameRate && !m_frameRate->Valid())
		m_frameRate.reset();
	if (m_sar && !m_sar->Valid())
		m_sar.reset();
}

Video::Video(const Video& other) = default;

Video::Video(Video&& other) noexcept = default;

Video::~Video() noexcept = default;

Video& Video::operator=(const Video& other) = default;

Video& Video::operator=(Video&& other) noexcept = default;

const class Color& Video::Color() const noexcept {
	return m_color;
}

const class Resolution& Video::Resolution() const noexcept {
	return m_resolution;
}

const StormByte::Safe::Optional<class HDR10>& Video::HDR10() const noexcept {
	return m_hdr10;
}

const StormByte::Safe::Optional<class DOVI>& Video::DOVI() const noexcept {
	return m_dovi;
}

const StormByte::Safe::Optional<AVRational>& Video::FrameRate() const noexcept {
	return m_frameRate;
}

const StormByte::Safe::Optional<AVRational>& Video::SampleAspectRatio() const noexcept {
	return m_sar;
}
