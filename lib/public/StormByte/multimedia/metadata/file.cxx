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

#include <StormByte/multimedia/metadata/file.hxx>

using namespace StormByte;
using namespace StormByte::Multimedia::Metadata;

File::File() = default;

File::File(const File& other) = default;

File::File(File&& other) noexcept = default;

File::~File() noexcept = default;

File& File::operator=(const File& other) = default;

File& File::operator=(File&& other) noexcept = default;

const Safe::Optional<Safe::String>& File::Title() const noexcept { return m_title; }

const Safe::Optional<Safe::String>& File::Artist() const noexcept { return m_artist; }

const Safe::Optional<Safe::String>& File::Album() const noexcept { return m_album; }

const Safe::Optional<Safe::String>& File::AlbumArtist() const noexcept { return m_albumArtist; }

const Safe::Optional<Safe::String>& File::Composer() const noexcept { return m_composer; }

const Safe::Optional<Safe::String>& File::Genre() const noexcept { return m_genre; }

const Safe::Optional<Safe::String>& File::Comment() const noexcept { return m_comment; }

const Safe::Optional<Safe::String>& File::Copyright() const noexcept { return m_copyright; }

const Safe::Optional<Safe::String>& File::Encoder() const noexcept { return m_encoder; }

const Safe::Optional<Safe::String>& File::Date() const noexcept { return m_date; }

Safe::Optional<unsigned> File::Track() const { return m_track; }

Safe::Optional<unsigned> File::Disc() const { return m_disc; }

void File::Title(Safe::String title) { m_title = std::move(title); }

void File::Artist(Safe::String artist) { m_artist = std::move(artist); }

void File::Album(Safe::String album) { m_album = std::move(album); }

void File::AlbumArtist(Safe::String albumArtist) { m_albumArtist = std::move(albumArtist); }

void File::Composer(Safe::String composer) { m_composer = std::move(composer); }

void File::Genre(Safe::String genre) { m_genre = std::move(genre); }

void File::Comment(Safe::String comment) { m_comment = std::move(comment); }

void File::Copyright(Safe::String copyright) { m_copyright = std::move(copyright); }

void File::Encoder(Safe::String encoder) { m_encoder = std::move(encoder); }

void File::Date(Safe::String date) { m_date = std::move(date); }

void File::Track(unsigned track) { m_track = track; }

void File::Disc(unsigned disc) { m_disc = disc; }
