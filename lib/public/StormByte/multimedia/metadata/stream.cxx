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

#include <StormByte/multimedia/metadata/stream.hxx>

using namespace StormByte;
using namespace StormByte::Multimedia::Metadata;

Disposition::Disposition() noexcept = default;

Disposition::Disposition(DispositionFlag flag) noexcept
: StormByte::Bitmask<Disposition, DispositionFlag>(flag) {}

Disposition::Disposition(const class Disposition& disposition) noexcept = default;

Disposition::Disposition(class Disposition&& disposition) noexcept = default;

Disposition::~Disposition() noexcept = default;

Disposition& Disposition::operator=(const class Disposition& disposition) noexcept = default;

Disposition& Disposition::operator=(class Disposition&& disposition) noexcept = default;

Stream::Stream() = default;

Stream::Stream(const Stream& other) = default;

Stream::Stream(Stream&& other) noexcept = default;

Stream::~Stream() noexcept = default;

Stream& Stream::operator=(const Stream& other) = default;

Stream& Stream::operator=(Stream&& other) noexcept = default;

const Safe::Optional<Safe::String>& Stream::Title() const noexcept { return m_title; }

const Safe::Optional<Safe::String>& Stream::Language() const noexcept { return m_language; }

Safe::Optional<std::uint64_t> Stream::BitRate() const { return m_bitRate; }

StormByte::Multimedia::Metadata::Disposition Stream::Disposition() const noexcept { return m_disposition; }

void Stream::Title(Safe::String title) { m_title = std::move(title); }

void Stream::Language(Safe::String language) { m_language = std::move(language); }

void Stream::BitRate(std::uint64_t bitRate) { m_bitRate = bitRate; }

void Stream::Disposition(class Disposition disposition) noexcept { m_disposition = disposition; }
