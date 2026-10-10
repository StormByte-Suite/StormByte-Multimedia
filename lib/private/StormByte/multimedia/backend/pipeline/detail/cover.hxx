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

#pragma once

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/multimedia/attachment.hxx>
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVStream.hxx>

#include <chrono>
#include <string_view>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
}

/**
 * @namespace StormByte::Multimedia::Detail
 * @brief Private probe helpers.
 */
namespace StormByte::Multimedia::Detail {
	/**
	 * @struct CollectedAttachments
	 * @brief Attachments taken from an open format context.
	 */
	struct CollectedAttachments {
		Multimedia::Attachments items;	///< Catalogue in container order
	};

	/**
	 * @brief Still-image codecs used as covers.
	 * @param codecId `AVCodecID`.
	 * @return true for MJPEG/PNG.
	 */
	inline bool IsStillImageCodec(int codecId) noexcept {
		switch (static_cast<AVCodecID>(codecId)) {
			case AV_CODEC_ID_MJPEG:
			case AV_CODEC_ID_MJPEGB:
			case AV_CODEC_ID_PNG:
			case AV_CODEC_ID_APNG:
			case AV_CODEC_ID_BMP:
			case AV_CODEC_ID_GIF:
			case AV_CODEC_ID_WEBP:
			case AV_CODEC_ID_TIFF:
			case AV_CODEC_ID_JPEG2000:
			case AV_CODEC_ID_PAM:
			case AV_CODEC_ID_PPM:
			case AV_CODEC_ID_JPEGLS:
				return true;
			default:
				return false;
		}
	}

	/**
	 * @brief FFmpeg attached-pic flag.
	 * @param stream Stream view.
	 * @return true if `AV_DISPOSITION_ATTACHED_PIC`.
	 */
	inline bool IsAttachedPicture(const FFmpeg::AVStream& stream) noexcept {
		return (stream.Disposition() & AV_DISPOSITION_ATTACHED_PIC) != 0;
	}

	/**
	 * @brief Matroska attachment stream (`ATTACHMENT` / `NONE`).
	 * @param stream Stream view.
	 * @return true if it is not a media track.
	 */
	inline bool IsContainerAttachment(const FFmpeg::AVStream& stream) noexcept {
		if (stream.Type() == AVMEDIA_TYPE_ATTACHMENT)
			return true;
		const FFmpeg::AVCodecParameters params = stream.CodecParameters();
		return params.CodecId() == AV_CODEC_ID_NONE;
	}

	/**
	 * @brief true if the container has a real video track.
	 * @param ctx Opened format context.
	 * @return true when a non-cover video exists.
	 */
	inline bool HasPrimaryVideo(const FFmpeg::AVFormatContext& ctx) noexcept {
		const auto streams = ctx.Streams();
		for (const auto& stream : streams) {
			if (stream.Type() != AVMEDIA_TYPE_VIDEO)
				continue;
			if (IsAttachedPicture(stream) || IsContainerAttachment(stream))
				continue;
			const FFmpeg::AVCodecParameters params = stream.CodecParameters();
			if (IsStillImageCodec(params.CodecId()))
				continue;
			return true;
		}
		return false;
	}

	/**
	 * @brief Cover-like video (flag or remuxed still next to a primary video).
	 * @param stream Stream view.
	 * @param hasPrimaryVideo Result of HasPrimaryVideo.
	 * @return true if it must not be a Stream.
	 */
	inline bool IsCoverStream(const FFmpeg::AVStream& stream, bool hasPrimaryVideo) noexcept {
		if (IsAttachedPicture(stream))
			return true;
		if (!hasPrimaryVideo)
			return false;
		if (stream.Type() != AVMEDIA_TYPE_VIDEO)
			return false;
		const FFmpeg::AVCodecParameters params = stream.CodecParameters();
		if (!IsStillImageCodec(params.CodecId()))
			return false;
		const auto duration = stream.Duration();
		if (!duration.has_value() || duration->Nanoseconds() <= std::chrono::milliseconds{50})
			return true;
		return false;
	}

	/**
	 * @brief Packet/stream index that File maps to Attachments().
	 * @param ctx Opened format context.
	 * @param index avformat stream index.
	 * @return true if Demux must drop it and Decoder must fail.
	 */
	inline bool IsAttachmentIndex(const FFmpeg::AVFormatContext& ctx, int index) noexcept {
		const bool hasPrimaryVideo = HasPrimaryVideo(ctx);
		const auto streams = ctx.Streams();
		for (const auto& stream : streams) {
			if (stream.Index() != index)
				continue;
			return IsContainerAttachment(stream) || IsCoverStream(stream, hasPrimaryVideo);
		}
		return false;
	}

	/**
	 * @brief true if @p pattern is exact, type-star, or star-star.
	 * @param pattern MIME pattern.
	 * @return true if usable as a filter.
	 */
	bool MimePatternOk(std::string_view pattern) noexcept;

	/**
	 * @brief true if @p mime matches @p pattern.
	 * @param mime Attachment MIME.
	 * @param pattern Exact type/subtype, type-star category, or star-star (all).
	 * @return true on match.
	 */
	bool MimeMatches(std::string_view mime, std::string_view pattern) noexcept;

	/**
	 * @brief Builds an attachment from a cover / attached stream.
	 * @param stream Stream view.
	 * @return Attachment (payload may be empty).
	 */
	Multimedia::Attachment MakeAttachment(const FFmpeg::AVStream& stream) noexcept;

	/**
	 * @brief Collects covers and container attachments from an open context.
	 * @param ctx Opened format context.
	 * @return Catalogue.
	 */
	CollectedAttachments CollectAttachments(const FFmpeg::AVFormatContext& ctx) noexcept;

	/**
	 * @brief Fills empty attachment payloads from packets.
	 * @param ctx Opened format context.
	 * @param items Catalogue from CollectAttachments.
	 */
	void FillEmptyAttachmentPayloads(FFmpeg::AVFormatContext& ctx, Multimedia::Attachments& items) noexcept;
}
