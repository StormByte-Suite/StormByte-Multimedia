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

#include <StormByte/multimedia/pipeline/track.hxx>

using namespace StormByte::Multimedia::Pipeline;

Track::Track(int in, enum StormByte::Multimedia::Type type) noexcept
: m_in(in), m_type(type) {}

Track::Track(int in, const Config::Base& config)
: m_in(in), m_type(config.Type()), m_config(config.Clone()) {}

Track::Track(int in, Config::Base&& config)
: m_in(in), m_type(config.Type()), m_config(config.Move()) {}

Track::Track(const Track& other)
: m_in(other.m_in), m_type(other.m_type), m_config(other.m_config ? other.m_config->Clone() : nullptr) {}

Track::Track(Track&& other) noexcept = default;

Track::~Track() noexcept = default;

Track& Track::operator=(Track&& other) noexcept = default;

Track::PointerType Track::Clone() const {
	return MakePointer<Track>(*this);
}

Track::PointerType Track::Move() {
	return MakePointer<Track>(std::move(*this));
}

Track& Track::operator=(const Track& other) {
	if (this == &other)
		return *this;
	Track replacement(other);
	*this = std::move(replacement);
	return *this;
}

Tracks::Tracks() noexcept = default;

Tracks::Tracks(const Tracks& other)
: Iterable() {
	for (const auto& track : other)
		add(*track);
}

Tracks::Tracks(Tracks&& other) noexcept = default;

Tracks::~Tracks() noexcept = default;

Tracks& Tracks::operator=(const Tracks& other) {
	if (this == &other)
		return *this;
	*this = Tracks(other);
	return *this;
}

Tracks& Tracks::operator=(Tracks&& other) noexcept = default;

const Track& Tracks::operator[](size_type index) const {
	return *Iterable::Container()[index];
}

void Tracks::add(const Track& track) {
	Iterable::Container().push_back(track.Clone());
}

void Tracks::add(Track&& track) {
	Iterable::Container().push_back(track.Move());
}
