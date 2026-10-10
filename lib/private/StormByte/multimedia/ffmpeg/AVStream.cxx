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

#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVStream.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/mathematics.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVStream::AVStream(::AVStream* stream) noexcept
:m_stream(stream) {}

bool FFmpeg::AVStream::operator<(const AVStream& other) const noexcept {
	return Index() < other.Index();
}

int FFmpeg::AVStream::Index() const noexcept {
	return m_stream ? m_stream->index : -1;
}

int FFmpeg::AVStream::Type() const noexcept {
	return m_stream ? m_stream->codecpar->codec_type : AVMEDIA_TYPE_UNKNOWN;
}

FFmpeg::AVCodecParameters FFmpeg::AVStream::CodecParameters() const noexcept {
	return m_stream ? AVCodecParameters(m_stream->codecpar) : AVCodecParameters(nullptr);
}

FFmpeg::AVRational FFmpeg::AVStream::TimeBase() const noexcept {
	return m_stream ? FFmpeg::FromRaw(m_stream->time_base) : FFmpeg::AVRational{0, 1};
}

StormByte::Safe::Optional<Property::Duration> FFmpeg::AVStream::Duration() const noexcept {
	if (!m_stream || m_stream->duration == AV_NOPTS_VALUE || m_stream->time_base.den <= 0)
		return {};
	const std::int64_t ns = av_rescale_q(m_stream->duration, m_stream->time_base, ::AVRational{1, 1000000000});
	if (ns < 0)
		return {};
	return Property::Duration(std::chrono::nanoseconds{ns});
}

double FFmpeg::AVStream::FrameRate() const noexcept {
	if (!m_stream)
		return 0.0;
	if (m_stream->avg_frame_rate.num && m_stream->avg_frame_rate.den)
		return av_q2d(m_stream->avg_frame_rate);
	if (m_stream->r_frame_rate.num && m_stream->r_frame_rate.den)
		return av_q2d(m_stream->r_frame_rate);
	return 0.0;
}

FFmpeg::AVRational FFmpeg::AVStream::FrameRateRational() const noexcept {
	if (!m_stream)
		return FFmpeg::AVRational{0, 1};
	if (m_stream->avg_frame_rate.num > 0 && m_stream->avg_frame_rate.den > 0)
		return FFmpeg::FromRaw(m_stream->avg_frame_rate);
	if (m_stream->r_frame_rate.num > 0 && m_stream->r_frame_rate.den > 0)
		return FFmpeg::FromRaw(m_stream->r_frame_rate);
	return FFmpeg::AVRational{0, 1};
}

FFmpeg::AVRational FFmpeg::AVStream::AverageFrameRateRational() const noexcept {
	if (!m_stream || m_stream->avg_frame_rate.num <= 0 || m_stream->avg_frame_rate.den <= 0)
		return FFmpeg::AVRational{0, 1};
	return FFmpeg::FromRaw(m_stream->avg_frame_rate);
}

FFmpeg::AVRational FFmpeg::AVStream::SampleAspectRatio() const noexcept {
	if (!m_stream)
		return FFmpeg::AVRational{0, 1};
	if (m_stream->codecpar) {
		const auto sar = FFmpeg::FromRaw(m_stream->codecpar->sample_aspect_ratio);
		if (sar.Valid())
			return sar;
	}
	return FFmpeg::FromRaw(m_stream->sample_aspect_ratio);
}

StormByte::Safe::Optional<StormByte::Safe::String> FFmpeg::AVStream::Tag(const char* key) const noexcept {
	if (!m_stream || !m_stream->metadata || !key)
		return {};
	const AVDictionaryEntry* entry = av_dict_get(m_stream->metadata, key, nullptr, 0);
	if (!entry)
		return {};
	return StormByte::Safe::String(entry->value);
}

int FFmpeg::AVStream::Disposition() const noexcept {
	return m_stream ? m_stream->disposition : 0;
}

::AVStream* FFmpeg::AVStream::Raw() const noexcept {
	return m_stream;
}
