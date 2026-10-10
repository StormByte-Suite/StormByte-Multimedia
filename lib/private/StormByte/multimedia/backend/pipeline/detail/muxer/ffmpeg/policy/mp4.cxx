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

#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>

#include <climits>
#include <cstring>
#include <format>
#include <span>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	std::span<const std::byte> UnreadSpan(const StormByte::Buffer::FIFO& fifo) noexcept {
		const auto& stored = fifo.Data();
		const auto available = static_cast<std::size_t>(fifo.Available());
		if (available == 0 || available > stored.size())
			return {};
		return {stored.data() + (stored.size() - available), available};
	}

	class Mp4 final: public Policy {
		public:
			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MarkDefaultStream(stream, haveDefaultVideo, haveDefaultAudio);
			}

			bool WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) const noexcept override {
				for (const auto& attachment : attachments) {
					const auto mime = attachment.MimeType();
					const auto type = mime ? static_cast<std::string_view>(mime.value()) : std::string_view{};
					AVCodecID codec = AV_CODEC_ID_NONE;
					std::string_view hint;
					if (type == "image/png") {
						codec = AV_CODEC_ID_PNG;
						hint = ".png";
					}
					else if (type == "image/jpeg" || type == "image/jpg") {
						codec = AV_CODEC_ID_MJPEG;
						hint = ".jpg";
					}
					else if (type == "image/bmp") {
						codec = AV_CODEC_ID_BMP;
						hint = ".bmp";
					}
					if (codec == AV_CODEC_ID_NONE) {
						owner.Fail(std::format("MP4 attachment cannot be represented as cover art: {}", type));
						return false;
					}
					const auto bytes = UnreadSpan(attachment.Payload());
					if (bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE)) {
						owner.Fail("MP4 cover image payload is empty or too large");
						return false;
					}
					const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
					const auto image = StormByte::Multimedia::FFmpeg::AVFrame::DecodeImage(data, bytes.size(), hint);
					if (!image) {
						owner.Fail("MP4 cover image could not be decoded");
						return false;
					}
					AVStream* stream = avformat_new_stream(&context, nullptr);
					if (!stream || av_new_packet(&stream->attached_pic, static_cast<int>(bytes.size())) < 0) {
						owner.Fail("could not allocate MP4 cover stream or packet");
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
					if (const auto name = attachment.FileName(); name)
						av_dict_set(&stream->metadata, "title", name->data(), 0);
				}
				return true;
			}

			bool WriteHeaderPackets(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context) const noexcept override {
				for (unsigned index = 0; index < context.nb_streams; ++index) {
					const auto* stream = context.streams[index];
					if (stream->disposition != AV_DISPOSITION_ATTACHED_PIC)
						continue;
					AVPacket* packet = av_packet_clone(&stream->attached_pic);
					if (!packet) {
						owner.Fail("could not allocate MP4 cover output packet");
						return false;
					}
					packet->stream_index = static_cast<int>(index);
					const int result = av_interleaved_write_frame(&context, packet);
					av_packet_free(&packet);
					if (result < 0) {
						owner.Fail(std::format("MP4 cover write failed: {}",
							static_cast<std::string_view>(StormByte::Multimedia::FFmpeg::ErrorToString(result))));
						return false;
					}
				}
				return true;
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::Mp4Policy() noexcept {
	static const Mp4 policy;
	return policy;
}
