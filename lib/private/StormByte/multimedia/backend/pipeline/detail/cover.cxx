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

#include <StormByte/multimedia/backend/pipeline/detail/cover.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
	std::string Lower(std::string_view view) noexcept {
		std::string out(view);
		for (char& ch : out)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		return out;
	}

	bool SplitMime(std::string_view value, std::string& type, std::string& subtype) noexcept {
		const auto slash = value.find('/');
		if (slash == std::string_view::npos || slash == 0 || slash + 1 == value.size())
			return false;
		if (value.find('/', slash + 1) != std::string_view::npos)
			return false;
		type = Lower(value.substr(0, slash));
		subtype = Lower(value.substr(slash + 1));
		return !type.empty() && !subtype.empty();
	}

	std::optional<std::string> StreamTag(const StormByte::Multimedia::FFmpeg::AVStream& stream, const char* key) noexcept {
		const char* value = stream.Tag(key);
		if (!value || value[0] == '\0')
			return std::nullopt;
		return std::string{value};
	}

	std::optional<std::string> GuessMime(int codecId) noexcept {
		switch (static_cast<AVCodecID>(codecId)) {
			case AV_CODEC_ID_MJPEG:
			case AV_CODEC_ID_MJPEGB:
			case AV_CODEC_ID_JPEGLS:
				return std::string{"image/jpeg"};
			case AV_CODEC_ID_PNG:
			case AV_CODEC_ID_APNG:
				return std::string{"image/png"};
			case AV_CODEC_ID_BMP:
				return std::string{"image/bmp"};
			case AV_CODEC_ID_GIF:
				return std::string{"image/gif"};
			case AV_CODEC_ID_WEBP:
				return std::string{"image/webp"};
			case AV_CODEC_ID_TIFF:
				return std::string{"image/tiff"};
			default:
				return std::nullopt;
		}
	}

	StormByte::Buffer::FIFO BytesToFifo(const std::uint8_t* data, int size) noexcept {
		if (!data || size <= 0)
			return StormByte::Buffer::FIFO{};
		StormByte::BinaryData bytes;
		bytes.assign(
			reinterpret_cast<const std::byte*>(data),
			reinterpret_cast<const std::byte*>(data) + size);
		return StormByte::Buffer::FIFO{std::move(bytes)};
	}
}

namespace StormByte::Multimedia::Detail {
	bool MimePatternOk(std::string_view pattern) noexcept {
		std::string type;
		std::string subtype;
		if (!SplitMime(pattern, type, subtype))
			return false;
		if (type == "*" && subtype != "*")
			return false;
		return true;
	}

	bool MimeMatches(std::string_view mime, std::string_view pattern) noexcept {
		std::string mimeType;
		std::string mimeSub;
		std::string patType;
		std::string patSub;
		if (!SplitMime(mime, mimeType, mimeSub))
			return false;
		if (!SplitMime(pattern, patType, patSub))
			return false;
		if (patType == "*" && patSub == "*")
			return true;
		if (patType != mimeType)
			return false;
		if (patSub == "*")
			return true;
		return patSub == mimeSub;
	}

	Multimedia::Attachment MakeAttachment(const FFmpeg::AVStream& stream) noexcept {
		std::optional<std::string> name = StreamTag(stream, "filename");
		if (!name)
			name = StreamTag(stream, "title");
		std::optional<std::string> mime = StreamTag(stream, "mimetype");
		if (!mime) {
			const FFmpeg::AVCodecParameters params = stream.CodecParameters();
			mime = GuessMime(params.CodecId());
		}

		StormByte::Buffer::FIFO payload;
		if (IsAttachedPicture(stream)) {
			const ::AVStream* raw = stream.Raw();
			if (raw && raw->attached_pic.size > 0 && raw->attached_pic.data)
				payload = BytesToFifo(raw->attached_pic.data, raw->attached_pic.size);
		}

		return Multimedia::Attachment{std::move(name), std::move(mime), std::move(payload)};
	}

	CollectedAttachments CollectAttachments(const FFmpeg::AVFormatContext& ctx) noexcept {
		CollectedAttachments out;
		const bool hasPrimaryVideo = HasPrimaryVideo(ctx);
		for (const auto& stream : ctx.Streams()) {
			if (!IsContainerAttachment(stream) && !IsCoverStream(stream, hasPrimaryVideo))
				continue;
			out.items.push_back(MakeAttachment(stream));
		}
		return out;
	}

	void FillEmptyAttachmentPayloads(FFmpeg::AVFormatContext& ctx, Multimedia::Attachments& items) noexcept {
		if (items.empty())
			return;

		const bool hasPrimaryVideo = HasPrimaryVideo(ctx);
		std::unordered_map<int, std::size_t> empty;
		std::size_t slot = 0;
		for (const auto& stream : ctx.Streams()) {
			if (!IsContainerAttachment(stream) && !IsCoverStream(stream, hasPrimaryVideo))
				continue;
			if (slot >= items.size())
				break;
				if (items[slot].Payload().Available() == 0)
				empty.emplace(stream.Index(), slot);
			++slot;
		}
		if (empty.empty())
			return;

		FFmpeg::AVPacket packet;
		for (;;) {
			const auto result = ctx.ReadPacket(packet);
			if (result == FFmpeg::OperationResult::EndOfFile)
				break;
			if (result == FFmpeg::OperationResult::TryAgain)
				continue;
			if (result != FFmpeg::OperationResult::Success)
				break;

			const auto it = empty.find(packet.StreamIndex());
			if (it != empty.end() && packet.Data() && packet.Size() > 0) {
				auto& dest = items[it->second];
							if (dest.Payload().Available() == 0) {
					dest = Multimedia::Attachment{dest.FileName(), dest.MimeType(),
						BytesToFifo(packet.Data(), packet.Size())};
					empty.erase(it);
				}
			}
			packet.Unref();
			if (empty.empty())
				break;
		}
	}
}
