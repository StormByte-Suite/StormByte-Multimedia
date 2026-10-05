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

#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/codec.hxx>

using namespace StormByte::Multimedia;

Container::Container(std::string_view name, std::string_view description, std::string_view extension, Access access) noexcept:
	m_name(name), m_description(description), m_extension(extension), m_access(access) {}

Container::Container(Container&& other) noexcept = default;

Container::~Container() noexcept = default;

Container& Container::operator=(Container&& other) noexcept = default;

bool Container::operator==(const Container& other) const noexcept {
	return this == &other;
}

bool Container::operator!=(const Container& other) const noexcept {
	return !(*this == other);
}

bool Container::HasAccess(Access access) const noexcept {
	return m_access.Has(access);
}

bool Container::Allows(const CodecRefs& codecs) const noexcept {
	if (codecs.empty())
		return true;
	for (const CodecRef codecRef : codecs) {
		if (!codecRef)
			return false;
		const Codec& codec = codecRef.get();
		bool found = false;
		for (const CodecRef allowed : m_allowed) {
			if (allowed && codec == allowed.get()) {
				found = true;
				break;
			}
		}

		if (!found)
			return false;
	}

	return true;
}
