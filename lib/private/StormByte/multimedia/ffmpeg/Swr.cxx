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

#include <StormByte/multimedia/ffmpeg/Swr.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>

extern "C" {
	#include <libswresample/swresample.h>
}

using namespace StormByte::Multimedia;

FFmpeg::Swr::Swr(SwrContext* ctx) noexcept
: AVPointer(ctx) {}

FFmpeg::Swr::~Swr() noexcept {
	Free();
}

FFmpeg::Swr::operator bool() const noexcept {
	return m_ptr != nullptr;
}

FFmpeg::Swr FFmpeg::Swr::Open(const FFmpeg::AVChannelLayout& out_layout, int out_fmt, int out_rate,
	const FFmpeg::AVChannelLayout& in_layout, int in_fmt, int in_rate) noexcept {
	SwrContext* ctx = nullptr;
	const auto* outRaw = FFmpeg::ToRaw(out_layout);
	const auto* inRaw = FFmpeg::ToRaw(in_layout);
	if (!outRaw || !inRaw) {
		return Swr(nullptr);
	}
	if (swr_alloc_set_opts2(&ctx, outRaw, static_cast<AVSampleFormat>(out_fmt), out_rate,
		inRaw, static_cast<AVSampleFormat>(in_fmt), in_rate, 0, nullptr) < 0) {
		swr_free(&ctx);
		return Swr(nullptr);
	}
	if (swr_init(ctx) < 0) {
		swr_free(&ctx);
		return Swr(nullptr);
	}
	return Swr(ctx);
}

bool FFmpeg::Swr::Convert(const AVFrame& src, AVFrame& dst) const noexcept {
	if (!m_ptr || !src.Get() || !dst.Get())
		return false;
	return swr_convert_frame(m_ptr, dst.Get(), src.Get()) >= 0;
}

bool FFmpeg::Swr::Drain(AVFrame& dst) const noexcept {
	if (!m_ptr || !dst.Get())
		return false;
	return swr_convert_frame(m_ptr, dst.Get(), nullptr) >= 0;
}

std::int64_t FFmpeg::Swr::Delay(std::int64_t base) const noexcept {
	return m_ptr ? swr_get_delay(m_ptr, base) : 0;
}

void FFmpeg::Swr::Free() noexcept {
	if (m_ptr)
		swr_free(&m_ptr);
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::SwrContext>;
