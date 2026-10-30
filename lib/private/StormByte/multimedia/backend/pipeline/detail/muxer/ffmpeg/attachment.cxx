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
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <climits>
#include <cstring>
#include <span>
#include <string>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
	#include <libavutil/base64.h>
	#include <libavutil/mem.h>
	#include <libavutil/pixdesc.h>
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
		if (*mime == "image/bmp")
			return AV_CODEC_ID_BMP;
		return AV_CODEC_ID_NONE;
	}

	const char* ImageHint(AVCodecID codec) noexcept {
		return codec == AV_CODEC_ID_PNG ? ".png" : codec == AV_CODEC_ID_BMP ? ".bmp" : ".jpg";
	}

}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg {
	bool Attachment::WriteCovers(StormByte::Multimedia::Pipeline::Muxer& owner,
		AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments,
		std::string_view format) noexcept {
		unsigned picture = 0;
		for (const auto& attachment : attachments) {
			const auto mime = attachment.MimeType();
			const auto codec = AttachmentCodecId(mime);
			if (codec == AV_CODEC_ID_NONE) {
				owner.Fail(StormByte::Safe::String::ReplaceToken("{} attachment cannot be represented as cover art: {}",
					format, mime ? static_cast<std::string_view>(*mime) : std::string_view{}));
				return false;
			}
			const auto bytes = UnreadSpan(attachment.Payload());
			if (bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE)) {
				owner.Fail("cover image payload is empty or too large");
				return false;
			}
			const auto image = StormByte::Multimedia::FFmpeg::AVFrame::DecodeImage(
				reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(), ImageHint(codec));
			if (!image) {
				owner.Fail("cover image could not be decoded");
				return false;
			}
			auto* stream = avformat_new_stream(&context, nullptr);
			if (!stream || av_new_packet(&stream->attached_pic, static_cast<int>(bytes.size())) < 0) {
				owner.Fail("could not allocate cover stream or packet");
				return false;
			}
			stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
			stream->codecpar->codec_id = codec;
			stream->codecpar->width = image.Width();
			stream->codecpar->height = image.Height();
			stream->disposition = AV_DISPOSITION_ATTACHED_PIC;
			stream->time_base = ::AVRational{1, 1000};
			std::memcpy(stream->attached_pic.data, bytes.data(), bytes.size());
			stream->attached_pic.stream_index = stream->index;
			stream->attached_pic.pts = 0;
			stream->attached_pic.dts = 0;
			stream->attached_pic.flags |= AV_PKT_FLAG_KEY;
			if ((attachment.FileName()
				&& av_dict_set(&stream->metadata, "title", attachment.FileName()->data(), 0) < 0)
				|| av_dict_set(&stream->metadata, "comment", picture++ == 0 ? "Cover (front)" : "Other", 0) < 0) {
				owner.Fail("could not copy cover image metadata");
				return false;
			}
		}
		return true;
	}

	bool Attachment::WriteCoverPackets(StormByte::Multimedia::Pipeline::Muxer& owner,
		AVFormatContext& context) noexcept {
		for (unsigned index = 0; index < context.nb_streams; ++index) {
			const auto* stream = context.streams[index];
			if (!(stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
				continue;
			auto* packet = av_packet_clone(&stream->attached_pic);
			if (!packet) {
				owner.Fail("could not allocate cover output packet");
				return false;
			}
			packet->stream_index = static_cast<int>(index);
			const int result = av_interleaved_write_frame(&context, packet);
			av_packet_free(&packet);
			if (result < 0) {
				owner.Fail(StormByte::Safe::String::ReplaceToken("cover write failed: {}",
					static_cast<std::string_view>(StormByte::Multimedia::FFmpeg::ErrorToString(result))));
				return false;
			}
		}
		return true;
	}

	bool Attachment::WritePictureComments(StormByte::Multimedia::Pipeline::Muxer& owner,
		AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) noexcept {
		unsigned picture = 0;
		for (const auto& attachment : attachments) {
			const auto mime = attachment.MimeType();
			const auto codec = AttachmentCodecId(mime);
			const auto bytes = UnreadSpan(attachment.Payload());
			const auto description = attachment.FileName() ? static_cast<std::string_view>(*attachment.FileName()) : std::string_view{};
			if (codec == AV_CODEC_ID_NONE || bytes.empty() || bytes.size() > 0xffffff - 32
				|| description.size() > 0xffffff - 32 - bytes.size()
				|| mime->size() > 0xffffff - 32 - bytes.size() - description.size()) {
				owner.Fail("Ogg cover must be a non-empty PNG, JPEG or BMP image smaller than 16 MiB");
				return false;
			}
			const auto image = StormByte::Multimedia::FFmpeg::AVFrame::DecodeImage(
				reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(), ImageHint(codec));
			if (!image) {
				owner.Fail("Ogg cover image could not be decoded");
				return false;
			}
			std::vector<std::uint8_t> block;
			const auto integer = [&](std::uint32_t value) {
				for (int shift = 24; shift >= 0; shift -= 8)
					block.push_back(static_cast<std::uint8_t>(value >> shift));
			};
			const auto text = [&](std::string_view value) {
				integer(static_cast<std::uint32_t>(value.size()));
				block.insert(block.end(), value.begin(), value.end());
			};
			integer(picture++ == 0 ? 3 : 0);
			text(static_cast<std::string_view>(*mime));
			text(description);
			integer(static_cast<std::uint32_t>(image.Width()));
			integer(static_cast<std::uint32_t>(image.Height()));
			const auto* format = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(image.Format()));
			integer(format ? static_cast<std::uint32_t>(av_get_bits_per_pixel(format)) : 0);
			integer(0);
			integer(static_cast<std::uint32_t>(bytes.size()));
			const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
			block.insert(block.end(), data, data + bytes.size());
			std::string encoded(AV_BASE64_SIZE(block.size()), '\0');
			if (!av_base64_encode(encoded.data(), static_cast<int>(encoded.size()), block.data(), static_cast<int>(block.size()))) {
				owner.Fail("could not write Ogg cover metadata");
				return false;
			}
			// oggenc merges global tags with DONT_OVERWRITE, which would discard all
			// but the first picture. Store each picture directly on every audio stream.
			for (unsigned index = 0; index < context.nb_streams; ++index) {
				auto* stream = context.streams[index];
				if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
					&& av_dict_set(&stream->metadata, "METADATA_BLOCK_PICTURE", encoded.c_str(), AV_DICT_MULTIKEY) < 0) {
					owner.Fail("could not write Ogg cover metadata");
					return false;
				}
			}
		}
		return true;
	}

	bool Attachment::Write(StormByte::Multimedia::Pipeline::Muxer& owner,
		AVFormatContext* ctx, const StormByte::Multimedia::Attachments& attachments) noexcept {
		if (!ctx)
			return true;
		if (attachments.empty())
			return true;
		for (const auto& attachment : attachments) {
			AVStream* stream = avformat_new_stream(ctx, nullptr);
			if (!stream) {
				owner.Fail("avformat_new_stream failed for attachment");
				return false;
			}

			stream->codecpar->codec_type = AVMEDIA_TYPE_ATTACHMENT;
			stream->codecpar->codec_id = AttachmentCodecId(attachment.MimeType());
			if (attachment.FileName() && !attachment.FileName()->empty())
				av_dict_set(&stream->metadata, "filename", attachment.FileName()->data(), 0);
			else if (stream->codecpar->codec_id != AV_CODEC_ID_NONE) {
				// ID3 pictures need no filename, but Matroska attachments do.
				const auto name = StormByte::Safe::String::ReplaceToken("cover-{}{}",
					stream->index, ImageHint(stream->codecpar->codec_id));
				if (av_dict_set(&stream->metadata, "filename", name.data(), 0) < 0) {
					owner.Fail("could not name embedded cover attachment");
					return false;
				}
			}
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
