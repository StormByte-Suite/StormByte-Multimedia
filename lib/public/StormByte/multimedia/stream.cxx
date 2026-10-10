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

#include <StormByte/multimedia/exception.hxx>
#include <StormByte/multimedia/stream.hxx>

#include <utility>

using namespace ::StormByte::Multimedia;

Stream::Stream(): m_index(-1), m_codec(nullptr) {}

Stream::Stream(int index, const class Codec& codec, Metadata::Stream metadata,
	StormByte::Safe::Optional<Property::Duration> duration, Properties properties) noexcept
: m_index(index), m_codec(&codec), m_metadata(std::move(metadata)),
m_duration(std::move(duration)), m_properties(std::move(properties)) {}

Stream::Stream(const Stream& other) = default;

Stream::Stream(Stream&& other) noexcept = default;

Stream::~Stream() noexcept = default;

Stream& Stream::operator=(const Stream& other) {
	if (this != &other) {
		Stream copy(other);
		*this = std::move(copy);
	}
	return *this;
}

Stream& Stream::operator=(Stream&& other) noexcept = default;

const class Codec& Stream::Codec() const {
	if (!m_codec)
		throw Exception("Stream", "empty stream has no codec");
	return *m_codec;
}

StormByte::Multimedia::Type Stream::Type() const noexcept {
	return m_codec ? m_codec->Type() : StormByte::Multimedia::Type::Unknown;
}

const StormByte::Safe::Optional<Property::Duration>& Stream::Duration() const noexcept {
	return m_duration;
}

const StormByte::Safe::Optional<Property::Video>& Stream::Video() const noexcept {
	return m_properties.first;
}

const StormByte::Safe::Optional<Property::Audio>& Stream::Audio() const noexcept {
	return m_properties.second;
}
