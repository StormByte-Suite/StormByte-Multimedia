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

#include <StormByte/multimedia/pipeline/side_data.hxx>

using namespace StormByte::Multimedia::Pipeline;

SideData::SideData() = default;

SideData::SideData(SideDataKind kind, StormByte::Buffer::FIFO payload)
: m_kind(kind), m_payload(std::move(payload)) {}

SideData::SideData(StormByte::Safe::String name, StormByte::Buffer::FIFO payload)
: m_kind(SideDataKind::Other), m_name(std::move(name)), m_payload(std::move(payload)) {}

SideData::SideData(const SideData& other) = default;

SideData::SideData(SideData&& other) noexcept = default;

SideData::~SideData() noexcept = default;

SideData& SideData::operator=(const SideData& other) = default;

SideData& SideData::operator=(SideData&& other) noexcept = default;

SideDataKind SideData::Kind() const noexcept {
	return m_kind;
}

const StormByte::Safe::Optional<StormByte::Safe::String>& SideData::Name() const noexcept {
	return m_name;
}

const StormByte::Buffer::FIFO& SideData::Payload() const noexcept {
	return m_payload;
}

StormByte::Buffer::FIFO& SideData::Payload() noexcept {
	return m_payload;
}
