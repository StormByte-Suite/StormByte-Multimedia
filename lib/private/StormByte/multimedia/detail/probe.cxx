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

#include <StormByte/multimedia/detail/probe.hxx>
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/safe/string.hxx>

#include <charconv>

extern "C" {
	#include <libavformat/avformat.h>
}

namespace FFmpeg = StormByte::Multimedia::FFmpeg;

namespace {
	StormByte::Safe::Optional<StormByte::Safe::String> NonEmpty(StormByte::Safe::Optional<StormByte::Safe::String> value) noexcept {
		return value && !value->empty() ? std::move(value) : std::nullopt;
	}

	StormByte::Safe::Optional<unsigned> ParseIndex(const StormByte::Safe::Optional<StormByte::Safe::String>& value) noexcept {
		if (!value || value->empty())
			return std::nullopt;
		unsigned out = 0;
		const char* begin = value->data();
		const char* end = begin;
		while (*end && *end != '/')
			++end;
		const auto parsed = std::from_chars(begin, end, out);
		if (parsed.ec != std::errc{} || parsed.ptr == begin)
			return std::nullopt;
		return out;
	}
}

StormByte::Multimedia::Metadata::Disposition
StormByte::Multimedia::Detail::Probe::Disposition(int flags) noexcept {
	Metadata::Disposition out;
	if (flags & AV_DISPOSITION_DEFAULT)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Default);
	if (flags & AV_DISPOSITION_DUB)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Dub);
	if (flags & AV_DISPOSITION_ORIGINAL)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Original);
	if (flags & AV_DISPOSITION_COMMENT)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Comment);
	if (flags & AV_DISPOSITION_LYRICS)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Lyrics);
	if (flags & AV_DISPOSITION_KARAOKE)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Karaoke);
	if (flags & AV_DISPOSITION_FORCED)
		out |= Metadata::Disposition(Metadata::DispositionFlag::Forced);
	if (flags & AV_DISPOSITION_HEARING_IMPAIRED)
		out |= Metadata::Disposition(Metadata::DispositionFlag::HearingImpaired);
	if (flags & AV_DISPOSITION_VISUAL_IMPAIRED)
		out |= Metadata::Disposition(Metadata::DispositionFlag::VisualImpaired);
	if (flags & AV_DISPOSITION_ATTACHED_PIC)
		out |= Metadata::Disposition(Metadata::DispositionFlag::AttachedPicture);
	return out;
}

StormByte::Multimedia::Metadata::File
StormByte::Multimedia::Detail::Probe::File(const FFmpeg::AVFormatContext& ctx) noexcept {
	Metadata::File meta;
	if (const auto value = NonEmpty(ctx.Tag("title")))
		meta.Title(*value);
	if (const auto value = NonEmpty(ctx.Tag("artist")))
		meta.Artist(*value);
	if (const auto value = NonEmpty(ctx.Tag("album")))
		meta.Album(*value);
	if (const auto value = NonEmpty(ctx.Tag("album_artist")))
		meta.AlbumArtist(*value);
	if (const auto value = NonEmpty(ctx.Tag("composer")))
		meta.Composer(*value);
	if (const auto value = NonEmpty(ctx.Tag("genre")))
		meta.Genre(*value);
	if (const auto value = NonEmpty(ctx.Tag("comment")))
		meta.Comment(*value);
	if (const auto value = NonEmpty(ctx.Tag("copyright")))
		meta.Copyright(*value);
	if (const auto value = NonEmpty(ctx.Tag("encoder")))
		meta.Encoder(*value);
	if (const auto value = NonEmpty(ctx.Tag("date")))
		meta.Date(*value);
	else if (const auto value = NonEmpty(ctx.Tag("year")))
		meta.Date(*value);
	if (const auto track = ParseIndex(ctx.Tag("track")))
		meta.Track(*track);
	if (const auto disc = ParseIndex(ctx.Tag("disc")))
		meta.Disc(*disc);
	return meta;
}

StormByte::Multimedia::Metadata::Stream
StormByte::Multimedia::Detail::Probe::Stream(const FFmpeg::AVStream& stream) noexcept {
	Metadata::Stream meta;
	if (const auto value = NonEmpty(stream.Tag("title")))
		meta.Title(*value);
	if (const auto value = NonEmpty(stream.Tag("language")))
		meta.Language(*value);
	const auto bitRate = stream.CodecParameters().BitRate();
	if (bitRate > 0)
		meta.BitRate(static_cast<std::uint64_t>(bitRate));
	meta.Disposition(Disposition(stream.Disposition()));
	return meta;
}
