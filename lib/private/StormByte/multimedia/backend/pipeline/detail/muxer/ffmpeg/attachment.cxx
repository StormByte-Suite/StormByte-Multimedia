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
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <cstring>
#include <span>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
	#include <libavutil/mem.h>
}

namespace {
	std::span<const std::byte> UnreadSpan(const StormByte::Buffer::FIFO& fifo) noexcept {
		const auto& stored = fifo.Data();
		const auto avail = fifo.Available();
		if (avail == 0 || avail > stored.size())
			return {};
		return std::span<const std::byte>{stored.data() + (stored.size() - avail), avail};
	}

	enum AVCodecID AttachmentCodecId(const StormByte::Safe::Optional<StormByte::Safe::String>& mime) noexcept {
		if (!mime)
			return AV_CODEC_ID_NONE;
		if (*mime == "image/jpeg" || *mime == "image/jpg")
			return AV_CODEC_ID_MJPEG;
		if (*mime == "image/png")
			return AV_CODEC_ID_PNG;
		return AV_CODEC_ID_NONE;
	}

	bool PlanWantsAttachments(const StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		const auto& plan = owner.Plan();
		if (!plan)
			return false;
		for (const auto& track : plan->Tracks()) {
			if (track && track->Type() == StormByte::Multimedia::Type::Attachment)
				return true;
		}
		return false;
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg {
	bool Attachment::Write(StormByte::Multimedia::Pipeline::Muxer& owner,
		AVFormatContext* ctx, const StormByte::Multimedia::Attachments& attachments) noexcept {
		if (!ctx)
			return true;
		if (!PlanWantsAttachments(owner) || attachments.empty())
			return true;
		for (const auto& attachment : attachments) {
			AVStream* stream = avformat_new_stream(ctx, nullptr);
			if (!stream) {
				owner.Fail("avformat_new_stream failed for attachment");
				return false;
			}

			stream->codecpar->codec_type = AVMEDIA_TYPE_ATTACHMENT;
			stream->codecpar->codec_id = AttachmentCodecId(attachment.MimeType());
			if (attachment.FileName())
				av_dict_set(&stream->metadata, "filename", attachment.FileName()->data(), 0);
			if (attachment.MimeType())
				av_dict_set(&stream->metadata, "mimetype", attachment.MimeType()->data(), 0);

			const auto view = UnreadSpan(attachment.Payload());
			if (view.empty())
				continue;
			auto* extra = static_cast<std::uint8_t*>(
				av_malloc(view.size() + static_cast<std::size_t>(AV_INPUT_BUFFER_PADDING_SIZE)));
			if (!extra) {
				owner.Fail("av_malloc failed for attachment");
				return false;
			}

			std::memcpy(extra, view.data(), view.size());
			std::memset(extra + view.size(), 0, static_cast<std::size_t>(AV_INPUT_BUFFER_PADDING_SIZE));
			stream->codecpar->extradata = extra;
			stream->codecpar->extradata_size = static_cast<int>(view.size());
		}

		return true;
	}
}
