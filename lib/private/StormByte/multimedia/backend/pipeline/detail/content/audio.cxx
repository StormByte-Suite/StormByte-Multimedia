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

#include <StormByte/multimedia/backend/pipeline/detail/content/audio.hxx>

extern "C" {
#include <libavutil/frame.h>
}

using StormByte::Multimedia::Backend::Pipeline::Detail::Content::Audio;

namespace {
	struct Card {
		int sample_rate = 0;
		int channels = 0;
	};

	Card Read(const ::AVFrame* raw) noexcept {
		Card card;
		if (!raw)
			return card;
		card.sample_rate = raw->sample_rate;
		card.channels = raw->ch_layout.nb_channels;
		return card;
	}

	void DropSide(::AVFrame* raw, AVFrameSideDataType type, StormByte::Safe::String& warning, const char* why) noexcept {
		if (!raw || !av_frame_get_side_data(raw, type))
			return;
		av_frame_remove_side_data(raw, type);
		if (warning.empty())
			warning = std::string_view(why);
		else {
			warning += "; ";
			warning += std::string_view(why);
		}
	}
}

void Audio::Put(const ::AVFrame* before, ::AVFrame* after) noexcept {
	m_warning.clear();
	if (!after)
		return;

	const Card in = Read(before);
	const Card out = Read(after);
	if (in.sample_rate == out.sample_rate && in.channels == out.channels)
		return;
	if (in.sample_rate == 0 && in.channels == 0)
		return;

	DropSide(after, AV_FRAME_DATA_DOWNMIX_INFO, m_warning,
		"dropped downmix info (audio layout or rate changed)");
	DropSide(after, AV_FRAME_DATA_MATRIXENCODING, m_warning,
		"dropped matrix encoding (audio layout or rate changed)");
	DropSide(after, AV_FRAME_DATA_AUDIO_SERVICE_TYPE, m_warning,
		"dropped audio service type (audio layout or rate changed)");
}
