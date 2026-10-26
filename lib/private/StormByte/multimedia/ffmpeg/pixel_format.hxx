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

#pragma once

#include <StormByte/multimedia/property/pixel_format.hxx>

extern "C" {
	#include <libavutil/pixfmt.h>
}

namespace StormByte::Multimedia::FFmpeg {
	/**
	 * @brief Maps a public pixel format to its `AVPixelFormat`.
	 * @param format Public pixel format.
	 * @return `AVPixelFormat` as int, or `AV_PIX_FMT_NONE` if unmapped.
	 */
	inline int ToAVPixelFormat(StormByte::Multimedia::Property::PixelFormat format) noexcept {
		using StormByte::Multimedia::Property::PixelFormat;
		switch (format) {
			case PixelFormat::YUV420P:	return AV_PIX_FMT_YUV420P;
			case PixelFormat::YUV422P:	return AV_PIX_FMT_YUV422P;
			case PixelFormat::YUV444P:	return AV_PIX_FMT_YUV444P;
			case PixelFormat::YUV420P10:	return AV_PIX_FMT_YUV420P10LE;
			case PixelFormat::YUV422P10:	return AV_PIX_FMT_YUV422P10LE;
			case PixelFormat::YUV444P10:	return AV_PIX_FMT_YUV444P10LE;
			case PixelFormat::YUV420P12:	return AV_PIX_FMT_YUV420P12LE;
			case PixelFormat::YUV422P12:	return AV_PIX_FMT_YUV422P12LE;
			case PixelFormat::YUV444P12:	return AV_PIX_FMT_YUV444P12LE;
			case PixelFormat::NV12:		return AV_PIX_FMT_NV12;
			case PixelFormat::NV21:		return AV_PIX_FMT_NV21;
			case PixelFormat::P010:		return AV_PIX_FMT_P010LE;
			case PixelFormat::RGB24:		return AV_PIX_FMT_RGB24;
			case PixelFormat::BGR24:		return AV_PIX_FMT_BGR24;
			case PixelFormat::RGBA:		return AV_PIX_FMT_RGBA;
			case PixelFormat::BGRA:		return AV_PIX_FMT_BGRA;
			case PixelFormat::GRAY8:		return AV_PIX_FMT_GRAY8;
			case PixelFormat::GRAY10:		return AV_PIX_FMT_GRAY10LE;
			case PixelFormat::GRAY16:		return AV_PIX_FMT_GRAY16LE;
			default:								return AV_PIX_FMT_NONE;
		}
	}
}
