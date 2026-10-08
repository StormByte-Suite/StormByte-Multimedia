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

#include <StormByte/multimedia/ffmpeg/Sws.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>

#include <utility>

extern "C" {
	#include <libavutil/pixfmt.h>
	#include <libswscale/swscale.h>
}

using namespace StormByte::Multimedia;

FFmpeg::Sws::Sws(::SwsContext* ctx) noexcept
: AVPointer(ctx) {}

FFmpeg::Sws::Sws() noexcept
: AVPointer(nullptr) {}

FFmpeg::Sws::~Sws() noexcept {
	Free();
}

FFmpeg::Sws::operator bool() const noexcept {
	return m_ptr != nullptr;
}

int FFmpeg::Sws::Bilinear() noexcept {
	return SWS_BILINEAR;
}

int FFmpeg::Sws::Bicubic() noexcept {
	return SWS_BICUBIC;
}

int FFmpeg::Sws::FastBilinear() noexcept {
	return SWS_FAST_BILINEAR;
}

int FFmpeg::Sws::Point() noexcept {
	return SWS_POINT;
}

int FFmpeg::Sws::Lanczos() noexcept {
	return SWS_LANCZOS;
}

FFmpeg::Sws FFmpeg::Sws::Open(int src_w, int src_h, int src_fmt,
	int dst_w, int dst_h, int dst_fmt, int flags) noexcept {
	if (flags == 0)
		flags = Bilinear();
	SwsContext* ctx = sws_getContext(
		src_w, src_h, static_cast<AVPixelFormat>(src_fmt),
		dst_w, dst_h, static_cast<AVPixelFormat>(dst_fmt),
		flags, nullptr, nullptr, nullptr);
	Sws out(ctx);
	out.m_srcW = src_w;
	out.m_srcH = src_h;
	out.m_srcFmt = src_fmt;
	out.m_dstW = dst_w;
	out.m_dstH = dst_h;
	out.m_dstFmt = dst_fmt;
	out.m_flags = flags;
	return out;
}

bool FFmpeg::Sws::Ensure(int src_w, int src_h, int src_fmt,
	int dst_w, int dst_h, int dst_fmt, int flags) noexcept {
	if (flags == 0)
		flags = Bilinear();
	if (m_ptr && m_srcW == src_w && m_srcH == src_h && m_srcFmt == src_fmt
		&& m_dstW == dst_w && m_dstH == dst_h && m_dstFmt == dst_fmt
		&& m_flags == flags)
		return true;
	Sws next = Open(src_w, src_h, src_fmt, dst_w, dst_h, dst_fmt, flags);
	if (!next)
		return false;
	*this = std::move(next);
	return true;
}

bool FFmpeg::Sws::Scale(const AVFrame& src, AVFrame& dst) const noexcept {
	if (!m_ptr || !src.Get() || !dst.Get())
		return false;
	const uint8_t* srcSlice[4]{};
	uint8_t* dstSlice[4]{};
	int srcStride[4]{};
	int dstStride[4]{};
	for (int i = 0; i < 4; ++i) {
		srcSlice[i] = src.Data(i);
		dstSlice[i] = dst.Data(i);
		srcStride[i] = src.Linesize(i);
		dstStride[i] = dst.Linesize(i);
	}
	return sws_scale(m_ptr, srcSlice, srcStride, 0, src.Height(), dstSlice, dstStride) > 0;
}

void FFmpeg::Sws::Free() noexcept {
	if (m_ptr) {
		sws_freeContext(m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::SwsContext>;
