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

#include <StormByte/multimedia/backend/pipeline/content.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/content/audio.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/content/passthrough.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/content/video.hxx>

using StormByte::Multimedia::Backend::Pipeline::Content;
using StormByte::Multimedia::Backend::Pipeline::Detail::Content::Audio;
using StormByte::Multimedia::Backend::Pipeline::Detail::Content::Passthrough;
using StormByte::Multimedia::Backend::Pipeline::Detail::Content::Video;
using StormByte::Multimedia::Type;

StormByte::Safe::Unique<Content> Content::For(Type type) noexcept {
	switch (type) {
		case Type::Video:
			return StormByte::Safe::MakeUnique<Video>();
		case Type::Audio:
			return StormByte::Safe::MakeUnique<Audio>();
		case Type::Subtitle:
		case Type::Attachment:
		case Type::Unknown:
			return StormByte::Safe::MakeUnique<Passthrough>();
	}

	return StormByte::Safe::MakeUnique<Passthrough>();
}

const std::string& Content::Warning() const noexcept {
	return m_warning;
}
