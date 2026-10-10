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
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	bool Encodes(const StormByte::Multimedia::Pipeline::Track& track) noexcept {
		const auto* config = track.Config();
		if (const auto* video = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Video*>(config))
			return video->Codec() != nullptr;
		if (const auto* audio = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Audio*>(config))
			return audio->Codec() != nullptr;
		if (const auto* subtitle = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Subtitle*>(config))
			return subtitle->Codec() != nullptr;
		return false;
	}

	class Matroska final: public Policy {
		public:
			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MarkDefaultStream(stream, haveDefaultVideo, haveDefaultAudio);
			}

			void ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer&,
				::AVFormatContext& context, std::int64_t, ::AVDictionary** options) const noexcept override {
				av_dict_set(&context.metadata, "encoding_tool", "StormByte-Multimedia " STORMBYTE_MULTIMEDIA_VERSION, 0);
				av_dict_set(options, "default_mode", "passthrough", 0);
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::MatroskaPolicy() noexcept {
	static const Matroska policy;
	return policy;
}
