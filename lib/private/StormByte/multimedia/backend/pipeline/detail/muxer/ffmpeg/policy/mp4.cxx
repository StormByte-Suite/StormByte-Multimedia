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
#include <StormByte/multimedia/pipeline/muxer.hxx>

#include <algorithm>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	class Mp4 final: public Policy {
		public:
			bool ZeroBasedTimeline() const noexcept override { return true; }
			bool CopiesCoverMetadata() const noexcept override { return true; }

			// movenc writes the dvcC/dvvC Dolby Vision configuration box only under
			// FF_COMPLIANCE_UNOFFICIAL; relax compliance just for outputs carrying it.
			void ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer&,
				::AVFormatContext& context, std::int64_t, ::AVDictionary**) const noexcept override {
				for (unsigned i = 0; i < context.nb_streams; ++i) {
					const auto* par = context.streams[i]->codecpar;
					if (par && av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data,
						AV_PKT_DATA_DOVI_CONF)) {
						context.strict_std_compliance = std::min(context.strict_std_compliance,
							FF_COMPLIANCE_UNOFFICIAL);
						return;
					}
				}
			}

			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MarkDefaultStream(stream, haveDefaultVideo, haveDefaultAudio);
			}

			bool WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) const noexcept override {
				return Attachment::WriteCovers(owner, context, attachments, "MP4");
			}

			bool WriteHeaderPackets(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context) const noexcept override {
				return Attachment::WriteCoverPackets(owner, context);
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::Mp4Policy() noexcept {
	static const Mp4 policy;
	return policy;
}
