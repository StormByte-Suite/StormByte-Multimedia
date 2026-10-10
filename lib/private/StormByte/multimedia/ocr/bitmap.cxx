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

#include <StormByte/multimedia/ocr/bitmap.hxx>

extern "C" {
	#include <libavcodec/avcodec.h>
}

/**
 * @file bitmap.cxx
 * @brief Paletted subtitle rects to 8-bit grayscale.
 */

namespace StormByte::Multimedia::OCR {
	/**
	 * @brief ITU-R BT.601 luma from an RGBA palette entry.
	 * @param palette Four-byte R,G,B,A.
	 * @return 0–255 luma, or 255 when the texel is transparent.
	 */
	static std::uint8_t Luma(const std::uint8_t* palette) noexcept {
		const unsigned a = palette[3];
		if (a < 16)
			return 255;
		const unsigned r = palette[0];
		const unsigned g = palette[1];
		const unsigned b = palette[2];
		const unsigned y = (77u * r + 150u * g + 29u * b) >> 8;
		return static_cast<std::uint8_t>(255u - y);
	}

	std::optional<GrayBitmap> GrayFromSubtitle(const FFmpeg::AVSubtitle& sub) noexcept {
		const auto* raw = sub.Get();
		if (!raw)
			return std::nullopt;

		int width = 0;
		int height = 0;
		for (unsigned i = 0; i < raw->num_rects; ++i) {
			const AVSubtitleRect* rect = raw->rects ? raw->rects[i] : nullptr;
			if (!rect || rect->type != SUBTITLE_BITMAP || rect->w <= 0 || rect->h <= 0 || !rect->data[0])
				continue;
			if (rect->w > width)
				width = rect->w;
			height += rect->h;
		}

		if (width <= 0 || height <= 0)
			return std::nullopt;

		GrayBitmap out;
		out.width = width;
		out.height = height;
		out.stride = width;
		out.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 255);

		int row = 0;
		for (unsigned i = 0; i < raw->num_rects; ++i) {
			const AVSubtitleRect* rect = raw->rects[i];
			if (!rect || rect->type != SUBTITLE_BITMAP || rect->w <= 0 || rect->h <= 0 || !rect->data[0])
				continue;
			const int linesize = rect->linesize[0] > 0 ? rect->linesize[0] : rect->w;
			const std::uint8_t* pal = rect->data[1];
			for (int y = 0; y < rect->h; ++y) {
				const std::uint8_t* src = rect->data[0] + y * linesize;
				std::uint8_t* dst = out.pixels.data()
					+ static_cast<std::size_t>(row + y) * static_cast<std::size_t>(width);
				for (int x = 0; x < rect->w; ++x) {
					if (pal)
						dst[x] = Luma(pal + static_cast<unsigned>(src[x]) * 4u);
					else
						dst[x] = src[x];
				}
			}

			row += rect->h;
		}

		return out;
	}
}
