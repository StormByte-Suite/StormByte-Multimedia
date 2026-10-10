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

#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/attachment.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>

extern "C" {
	#include <libavformat/avformat.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

Policy::Policy() noexcept = default;

Policy::~Policy() noexcept = default;

void Policy::PrepareStream(::AVStream&, bool&, bool&) const noexcept {}

bool Policy::WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
	::AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) const noexcept {
	return Attachment::Write(owner, &context, attachments);
}

bool Policy::WriteHeaderPackets(StormByte::Multimedia::Pipeline::Muxer&, ::AVFormatContext&) const noexcept {
	return true;
}

void Policy::ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer&,
	::AVFormatContext&, std::int64_t, ::AVDictionary**) const noexcept {}

void Policy::MarkDefaultStream(::AVStream& stream, bool& haveDefaultVideo,
	bool& haveDefaultAudio) noexcept {
	if (!stream.codecpar)
		return;
	if (stream.codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !haveDefaultVideo) {
		stream.disposition |= AV_DISPOSITION_DEFAULT;
		haveDefaultVideo = true;
	}
	if (stream.codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !haveDefaultAudio) {
		stream.disposition |= AV_DISPOSITION_DEFAULT;
		haveDefaultAudio = true;
	}
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::GenericPolicy() noexcept {
	static const Policy policy;
	return policy;
}
