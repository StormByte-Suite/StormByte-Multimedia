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

#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/avutil.h>
	#include <libavutil/mem.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVSubtitle::AVSubtitle() noexcept
: m_sub(new ::AVSubtitle{}) {}

FFmpeg::AVSubtitle::AVSubtitle(AVSubtitle&& other) noexcept
: m_sub(other.m_sub) {
	other.m_sub = nullptr;
}

FFmpeg::AVSubtitle::~AVSubtitle() noexcept {
	Free();
	delete m_sub;
	m_sub = nullptr;
}

FFmpeg::AVSubtitle& FFmpeg::AVSubtitle::operator=(AVSubtitle&& other) noexcept {
	if (this != &other) {
		Free();
		delete m_sub;
		m_sub = other.m_sub;
		other.m_sub = nullptr;
	}

	return *this;
}

::AVSubtitle* FFmpeg::AVSubtitle::Get() noexcept {
	return m_sub;
}

const ::AVSubtitle* FFmpeg::AVSubtitle::Get() const noexcept {
	return m_sub;
}

std::int64_t FFmpeg::AVSubtitle::Pts() const noexcept {
	return m_sub ? m_sub->pts : AV_NOPTS_VALUE;
}

std::uint32_t FFmpeg::AVSubtitle::DisplayDurationMs() const noexcept {
	if (!m_sub || m_sub->end_display_time < m_sub->start_display_time)
		return 0;
	const auto ms = m_sub->end_display_time - m_sub->start_display_time;
	if (ms == 0 || ms > 600000)
		return 0;
	return ms;
}

StormByte::Safe::String FFmpeg::AVSubtitle::Text() const noexcept {
	StormByte::Safe::String out;
	if (!m_sub)
		return out;
	for (unsigned i = 0; i < m_sub->num_rects; ++i) {
		const AVSubtitleRect* rect = m_sub->rects[i];
		if (!rect)
			continue;
		const char* text = rect->text ? rect->text : rect->ass;
		if (!text)
			continue;
		if (!out.empty())
			out.push_back('\n');
		out.append(text);
	}

	return out;
}

void FFmpeg::AVSubtitle::FillText(std::string_view text, std::int64_t pts, std::uint32_t duration_ms, bool ass) noexcept {
	Free();
	if (!m_sub)
		m_sub = new ::AVSubtitle{};
	m_sub->pts = pts;
	m_sub->start_display_time = 0;
	m_sub->end_display_time = duration_ms;
	m_sub->num_rects = 1;
	m_sub->rects = static_cast<AVSubtitleRect**>(av_mallocz(sizeof(AVSubtitleRect*)));
	if (!m_sub->rects) {
		m_sub->num_rects = 0;
		return;
	}

	m_sub->rects[0] = static_cast<AVSubtitleRect*>(av_mallocz(sizeof(AVSubtitleRect)));
	if (!m_sub->rects[0]) {
		av_free(m_sub->rects);
		m_sub->rects = nullptr;
		m_sub->num_rects = 0;
		return;
	}

	if (ass) {
		m_sub->rects[0]->type = SUBTITLE_ASS;
		m_sub->rects[0]->ass = av_strndup(text.empty() ? "" : text.data(), text.size());
	}

	else {
		m_sub->rects[0]->type = SUBTITLE_TEXT;
		m_sub->rects[0]->text = av_strndup(text.empty() ? "" : text.data(), text.size());
	}
}

void FFmpeg::AVSubtitle::Free() noexcept {
	if (!m_sub)
		return;
	avsubtitle_free(m_sub);
	*m_sub = {};
}
