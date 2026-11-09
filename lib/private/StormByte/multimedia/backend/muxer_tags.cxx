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

#include <StormByte/multimedia/backend/muxer_tags.hxx>

using StormByte::Multimedia::Metadata::FileTag;
using StormByte::Multimedia::Metadata::StreamTag;

namespace {
	constexpr std::uint16_t Bit(FileTag tag) noexcept {
		return static_cast<std::uint16_t>(1U << static_cast<unsigned>(tag));
	}

	constexpr std::uint8_t Bit(StreamTag tag) noexcept {
		return static_cast<std::uint8_t>(1U << static_cast<unsigned>(tag));
	}

	constexpr std::uint16_t AllFile = static_cast<std::uint16_t>((1U << StormByte::Multimedia::Metadata::FileTagCount) - 1U);
	constexpr std::uint8_t Language = Bit(StreamTag::Language);
	constexpr std::uint8_t BothStream = Bit(StreamTag::Language) | Bit(StreamTag::Title);

	struct Row {
		std::string_view name;
		StormByte::Multimedia::Backend::TagSupport support;
	};

	// Measured by writing and probing every tag; unlisted muxers store nothing.
	constexpr Row Rows[] = {
		{"asf", {AllFile, Language, false}},
		{"caf", {AllFile, 0, false}},
		{"ffmetadata", {AllFile, 0, false}},
		{"flac", {AllFile, 0, false}},
		{"flv", {AllFile, 0, false}},
		{"matroska", {AllFile, BothStream, false}},
		{"mp3", {AllFile, 0, false}},
		{"mp4", {AllFile, BothStream, false}},
		{"nut", {AllFile, BothStream, false}},
		{"oga", {AllFile, BothStream, true}},
		{"ogg", {AllFile, BothStream, true}},
		{"ogv", {AllFile, BothStream, true}},
		{"opus", {AllFile, BothStream, true}},
		{"spx", {AllFile, BothStream, true}},
		{"tta", {AllFile, 0, false}},
		{"webm", {AllFile, BothStream, false}},
		{"wtv", {AllFile, 0, false}},
		{"wv", {AllFile, 0, false}},
		{"mov", {Bit(FileTag::Title) | Bit(FileTag::Artist) | Bit(FileTag::Album) | Bit(FileTag::Genre)
			| Bit(FileTag::Comment) | Bit(FileTag::Copyright) | Bit(FileTag::Date), BothStream, false}},
		{"3gp", {0, Language, false}},
		{"3g2", {0, Language, false}},
		{"aiff", {Bit(FileTag::Title) | Bit(FileTag::Comment) | Bit(FileTag::Copyright), 0, false}},
		{"au", {Bit(FileTag::Title) | Bit(FileTag::Artist) | Bit(FileTag::Album) | Bit(FileTag::Genre)
			| Bit(FileTag::Track), 0, false}},
		{"avi", {Bit(FileTag::Title) | Bit(FileTag::Artist) | Bit(FileTag::Genre) | Bit(FileTag::Comment)
			| Bit(FileTag::Copyright) | Bit(FileTag::Date) | Bit(FileTag::Track), Bit(StreamTag::Title), false}},
		{"wav", {Bit(FileTag::Title) | Bit(FileTag::Artist) | Bit(FileTag::Album) | Bit(FileTag::Genre)
			| Bit(FileTag::Comment) | Bit(FileTag::Copyright) | Bit(FileTag::Date) | Bit(FileTag::Track), 0, false}},
		{"rm", {Bit(FileTag::Title) | Bit(FileTag::Comment) | Bit(FileTag::Copyright), 0, false}},
		{"sox", {Bit(FileTag::Comment), 0, false}},
		{"argo_asf", {Bit(FileTag::Title), 0, false}},
		{"mpegts", {0, Language, false}},
	};
}

namespace StormByte::Multimedia::Backend {
	TagSupport SelectTagSupport(std::string_view formatName) noexcept {
		for (const auto& row : Rows) {
			if (row.name == formatName)
				return row.support;
		}
		return {};
	}

	const char* FileTagKey(FileTag tag) noexcept {
		switch (tag) {
			case FileTag::Title:
				return "title";
			case FileTag::Artist:
				return "artist";
			case FileTag::Album:
				return "album";
			case FileTag::AlbumArtist:
				return "album_artist";
			case FileTag::Composer:
				return "composer";
			case FileTag::Genre:
				return "genre";
			case FileTag::Comment:
				return "comment";
			case FileTag::Copyright:
				return "copyright";
			case FileTag::Date:
				return "date";
			case FileTag::Track:
				return "track";
			case FileTag::Disc:
				return "disc";
		}
		return "";
	}
}
