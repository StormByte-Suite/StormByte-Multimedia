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

#include <StormByte/multimedia/ffmpeg/exception.hxx>

using namespace StormByte::Multimedia::FFmpeg;

Exception::Exception(const Exception& other) = default;

Exception::Exception(Exception&& other) noexcept = default;

Exception::~Exception() noexcept = default;

Exception& Exception::operator=(const Exception& other) = default;

Exception& Exception::operator=(Exception&& other) noexcept = default;

BSFError::BSFError(const BSFError& other) = default;

BSFError::BSFError(BSFError&& other) noexcept = default;

BSFError::~BSFError() noexcept = default;

BSFError& BSFError::operator=(const BSFError& other) = default;

BSFError& BSFError::operator=(BSFError&& other) noexcept = default;

DecoderError::DecoderError(const DecoderError& other) = default;

DecoderError::DecoderError(DecoderError&& other) noexcept = default;

DecoderError::~DecoderError() noexcept = default;

DecoderError& DecoderError::operator=(const DecoderError& other) = default;

DecoderError& DecoderError::operator=(DecoderError&& other) noexcept = default;

EncoderError::EncoderError(const EncoderError& other) = default;

EncoderError::EncoderError(EncoderError&& other) noexcept = default;

EncoderError::~EncoderError() noexcept = default;

EncoderError& EncoderError::operator=(const EncoderError& other) = default;

EncoderError& EncoderError::operator=(EncoderError&& other) noexcept = default;
