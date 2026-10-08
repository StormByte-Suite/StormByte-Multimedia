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

#include <StormByte/multimedia/ffmpeg/Dictionary.hxx>

extern "C" {
	#include <libavutil/dict.h>
}

using namespace StormByte::Multimedia;

FFmpeg::Dictionary::Dictionary() noexcept
: AVPointer(static_cast<::AVDictionary*>(nullptr)) {}

FFmpeg::Dictionary::~Dictionary() noexcept {
	Free();
}

FFmpeg::Dictionary::operator bool() const noexcept {
	return m_ptr != nullptr;
}

bool FFmpeg::Dictionary::Set(const char* key, const char* value, int flags) noexcept {
	if (!key)
		return false;
	return av_dict_set(&m_ptr, key, value, flags) >= 0;
}

const char* FFmpeg::Dictionary::Value(const char* key) const noexcept {
	if (!key || !m_ptr)
		return nullptr;
	const AVDictionaryEntry* e = av_dict_get(m_ptr, key, nullptr, 0);
	return e ? e->value : nullptr;
}

int FFmpeg::Dictionary::Count() const noexcept {
	return m_ptr ? av_dict_count(m_ptr) : 0;
}

void FFmpeg::Dictionary::Free() noexcept {
	av_dict_free(&m_ptr);
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVDictionary>;
