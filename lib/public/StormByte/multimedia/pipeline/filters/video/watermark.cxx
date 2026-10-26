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

#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/safe/string.hxx>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>

extern "C" {
	#include <libavutil/pixdesc.h>
	#include <libavutil/pixfmt.h>
}

using namespace StormByte::Multimedia::Pipeline::Filter::Video;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using Level = StormByte::Logger::Level;

namespace {
	constexpr int BarSlack = 16;

	std::string_view ImageHint(const StormByte::Safe::Binary& bytes) noexcept {
		const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
		const auto size = static_cast<std::size_t>(bytes.size());
		if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N'
			&& data[3] == 'G' && data[4] == 0x0d && data[5] == 0x0a
			&& data[6] == 0x1a && data[7] == 0x0a)
			return ".png";
		if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff)
			return ".jpg";
		if (size >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F'
			&& data[3] == 'F' && data[8] == 'W' && data[9] == 'E'
			&& data[10] == 'B' && data[11] == 'P')
			return ".webp";
		if (size >= 2 && data[0] == 'B' && data[1] == 'M')
			return ".bmp";
		return {};
	}

	int SampleY8(const FFrame& src, int x, int y) noexcept {
		if (!src.Data(0) || x < 0 || y < 0 || x >= src.Width() || y >= src.Height())
			return 0;
		return src.Data(0)[y * src.Linesize(0) + x];
	}

	int RowMeanY8(const FFrame& src, int row) noexcept {
		unsigned sum = 0;
		const int step = src.Width() > 64 ? src.Width() / 64 : 1;
		int n = 0;
		for (int x = 0; x < src.Width(); x += step) {
			sum += static_cast<unsigned>(SampleY8(src, x, row));
			++n;
		}

		return n ? static_cast<int>(sum / static_cast<unsigned>(n)) : 0;
	}

	int ColMeanY8(const FFrame& src, int col) noexcept {
		unsigned sum = 0;
		const int step = src.Height() > 64 ? src.Height() / 64 : 1;
		int n = 0;
		for (int y = 0; y < src.Height(); y += step) {
			sum += static_cast<unsigned>(SampleY8(src, col, y));
			++n;
		}

		return n ? static_cast<int>(sum / static_cast<unsigned>(n)) : 0;
	}

	int ScanBar(int limit, int cap, const auto& meanAt) noexcept {
		int bar = 0;
		int noise = 0;
		while (bar < limit) {
			const int mean = meanAt(bar);
			if (mean <= cap) {
				noise = 0;
				++bar;
				continue;
			}

			if (mean <= cap + 8 && noise < 2) {
				++noise;
				++bar;
				continue;
			}

			break;
		}

		return bar - noise;
	}

	std::pair<int, int> PairBars(int a, int b) noexcept {
		const int lo = std::min(a, b);
		const int hi = std::max(a, b);
		if (lo < 4)
			return {0, 0};
		if (hi > 0 && lo * 3 < hi)
			return {lo, lo};
		return {a, b};
	}

	void ApplyLut(FFrame& gray, const std::array<uint8_t, 256>& lut) noexcept {
		if (!gray || !gray.Data(0))
			return;
		for (int y = 0; y < gray.Height(); ++y) {
			uint8_t* row = gray.Data(0) + y * gray.Linesize(0);
			for (int x = 0; x < gray.Width(); ++x)
				row[x] = lut[row[x]];
		}
	}

	std::array<uint8_t, 256> StretchLut(int lo, int hi) noexcept {
		std::array<uint8_t, 256> lut {};
		if (hi <= lo) {
			for (int i = 0; i < 256; ++i)
				lut[static_cast<std::size_t>(i)] = static_cast<uint8_t>(i);
			return lut;
		}

		for (int i = 0; i < 256; ++i) {
			if (i <= lo)
				lut[static_cast<std::size_t>(i)] = 0;
			else if (i >= hi)
				lut[static_cast<std::size_t>(i)] = 255;
			else
				lut[static_cast<std::size_t>(i)] = static_cast<uint8_t>(((i - lo) * 255) / (hi - lo));
		}

		return lut;
	}

	std::array<uint8_t, 256> GammaLut(double gamma) noexcept {
		std::array<uint8_t, 256> lut {};
		for (int i = 0; i < 256; ++i)
			lut[static_cast<std::size_t>(i)] = static_cast<uint8_t>(
				std::clamp(std::pow(static_cast<double>(i) / 255.0, gamma) * 255.0, 0.0, 255.0));
		return lut;
	}

	struct Probe {
		int top = 0;
		int bottom = 0;
		int left = 0;
		int right = 0;
	};

	Probe Measure(const FFrame& gray) noexcept {
		Probe out;
		const int midY = RowMeanY8(gray, gray.Height() / 2);
		const int midX = ColMeanY8(gray, gray.Width() / 2);
		const int edgeTop = RowMeanY8(gray, 0);
		const int edgeBot = RowMeanY8(gray, gray.Height() - 1);
		const int edgeLeft = ColMeanY8(gray, 0);
		const int edgeRight = ColMeanY8(gray, gray.Width() - 1);
		const int picture = std::max(midY, midX);
		const int maxY = gray.Height() / 3;
		const int maxX = gray.Width() / 4;

		const int edgeY = std::min(edgeTop, edgeBot);
		if (picture >= edgeY + 16) {
			const int cap = edgeY + std::max(10, (picture - edgeY) / 4);
			out.top = ScanBar(maxY, cap, [&](int i) { return RowMeanY8(gray, i); });
			out.bottom = ScanBar(maxY, cap,
				[&](int i) { return RowMeanY8(gray, gray.Height() - 1 - i); });
			const auto pair = PairBars(out.top, out.bottom);
			out.top = pair.first;
			out.bottom = pair.second;
		}

		const int edgeX = std::min(edgeLeft, edgeRight);
		if (picture >= edgeX + 16) {
			const int cap = edgeX + std::max(10, (picture - edgeX) / 4);
			out.left = ScanBar(maxX, cap, [&](int i) { return ColMeanY8(gray, i); });
			out.right = ScanBar(maxX, cap,
				[&](int i) { return ColMeanY8(gray, gray.Width() - 1 - i); });
			const auto pair = PairBars(out.left, out.right);
			out.left = pair.first;
			out.right = pair.second;
		}

		return out;
	}

	bool Boxed(const Probe& p) noexcept {
		return (p.top > 4 && p.bottom > 4) || (p.left > 4 && p.right > 4);
	}

	bool NearBar(int found, int stored) noexcept {
		return std::abs(found - stored) <= BarSlack;
	}

	std::pair<int, int> Place(int logoW, int logoH,
		const StormByte::Safe::Optional<Anchor>& anchor,
		const StormByte::Safe::Optional<StormByte::Multimedia::Property::Point>& point,
		int margin, int x0, int y0, int aw, int ah) noexcept {
		if (point.has_value())
			return {point->X(), point->Y()};
		const Anchor a = anchor.value_or(Anchor::BottomRight);
		int x = x0 + margin;
		int y = y0 + margin;
		switch (a) {
			case Anchor::TopLeft:		x = x0 + margin; y = y0 + margin; break;
			case Anchor::TopCenter:		x = x0 + (aw - logoW) / 2; y = y0 + margin; break;
			case Anchor::TopRight:		x = x0 + aw - logoW - margin; y = y0 + margin; break;
			case Anchor::CenterLeft:	x = x0 + margin; y = y0 + (ah - logoH) / 2; break;
			case Anchor::Center:		x = x0 + (aw - logoW) / 2; y = y0 + (ah - logoH) / 2; break;
			case Anchor::CenterRight:	x = x0 + aw - logoW - margin; y = y0 + (ah - logoH) / 2; break;
			case Anchor::BottomLeft:	x = x0 + margin; y = y0 + ah - logoH - margin; break;
			case Anchor::BottomCenter:	x = x0 + (aw - logoW) / 2; y = y0 + ah - logoH - margin; break;
			case Anchor::BottomRight:	x = x0 + aw - logoW - margin; y = y0 + ah - logoH - margin; break;
		}

		return {x, y};
	}

	void Blend(uint8_t* dst, int dstLinesize, int frameW, int frameH,
		const uint8_t* logo, int logoW, int logoH, int x, int y, unsigned opacity) noexcept {
		const int alphaScale = static_cast<int>(opacity);
		for (int row = 0; row < logoH; ++row) {
			const int fy = y + row;
			if (fy < 0 || fy >= frameH)
				continue;
			uint8_t* line = dst + fy * dstLinesize;
			const uint8_t* src = logo + row * logoW * 4;
			for (int col = 0; col < logoW; ++col) {
				const int fx = x + col;
				if (fx < 0 || fx >= frameW)
					continue;
				const uint8_t* px = src + col * 4;
				const int a = (static_cast<int>(px[3]) * alphaScale) / 100;
				if (a <= 0)
					continue;
				uint8_t* d = line + fx * 4;
				const int ia = 255 - a;
				d[0] = static_cast<uint8_t>((d[0] * ia + px[0] * a) / 255);
				d[1] = static_cast<uint8_t>((d[1] * ia + px[1] * a) / 255);
				d[2] = static_cast<uint8_t>((d[2] * ia + px[2] * a) / 255);
				d[3] = 255;
			}
		}
	}

	constexpr double GraphicsWhite = 203.0;
	constexpr double HdrPeak = 10000.0;
	constexpr int MaxLogoEdge = 16384;

	bool HdrTransfer(int trc) noexcept {
		return trc == AVCOL_TRC_SMPTE2084 || trc == AVCOL_TRC_ARIB_STD_B67;
	}

	int WorkingPrimaries(const FFrame& src) noexcept {
		const int primaries = src.ColorPrimaries();
		if (primaries == AVCOL_PRI_UNSPECIFIED || primaries == AVCOL_PRI_RESERVED0)
			return HdrTransfer(src.ColorTransfer()) ? AVCOL_PRI_BT2020 : AVCOL_PRI_BT709;
		return primaries;
	}

	int WorkingTransfer(const FFrame& src) noexcept {
		const int transfer = src.ColorTransfer();
		if (transfer == AVCOL_TRC_UNSPECIFIED || transfer == AVCOL_TRC_RESERVED0)
			return AVCOL_TRC_BT709;
		return transfer;
	}

	int WorkingMatrix(const FFrame& src, bool rgb) noexcept {
		if (rgb)
			return AVCOL_SPC_RGB;
		const int matrix = src.ColorSpace();
		if (matrix == AVCOL_SPC_UNSPECIFIED || matrix == AVCOL_SPC_RGB)
			return HdrTransfer(src.ColorTransfer()) ? AVCOL_SPC_BT2020_NCL : AVCOL_SPC_BT709;
		return matrix;
	}

	int WorkingRange(const FFrame& src, bool rgb) noexcept {
		if (rgb || src.ColorRange() == AVCOL_RANGE_JPEG)
			return AVCOL_RANGE_JPEG;
		return AVCOL_RANGE_MPEG;
	}

	FFrame::ColorTarget LinearTarget(int primaries, bool hdr) noexcept {
		FFrame::ColorTarget color;
		color.transfer = AVCOL_TRC_LINEAR;
		color.primaries = primaries;
		color.matrix = AVCOL_SPC_RGB;
		color.range = AVCOL_RANGE_JPEG;
		color.peak = hdr ? HdrPeak : 100.0;
		return color;
	}

	int ScaledDim(int n, double factor) noexcept {
		const double value = static_cast<double>(n) * factor;
		if (!std::isfinite(value) || value >= static_cast<double>(MaxLogoEdge))
			return MaxLogoEdge;
		return std::max(1, static_cast<int>(std::lround(value)));
	}

	void TagSrgb(FFrame& frame) noexcept {
		frame.ColorTransfer(AVCOL_TRC_IEC61966_2_1);
		frame.ColorPrimaries(AVCOL_PRI_BT709);
		frame.ColorSpace(AVCOL_SPC_RGB);
		frame.ColorRange(AVCOL_RANGE_JPEG);
	}

	bool StorePacked(FFrame& dst, const std::uint8_t* src, int width, int height) noexcept {
		if (!src || !dst.AllocVideo(width, height, AV_PIX_FMT_RGBA) || !dst.Data(0))
			return false;
		for (int row = 0; row < height; ++row) {
			const auto* in = src + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4;
			std::copy(in, in + static_cast<std::size_t>(width) * 4, dst.Data(0) + row * dst.Linesize(0));
		}
		TagSrgb(dst);
		return true;
	}

	void ExtractAlpha(const FFrame& rgba, FFrame& gray) noexcept {
		for (int row = 0; row < rgba.Height(); ++row) {
			const std::uint8_t* src = rgba.Data(0) + row * rgba.Linesize(0);
			std::uint8_t* dst = gray.Data(0) + row * gray.Linesize(0);
			for (int col = 0; col < rgba.Width(); ++col)
				dst[col] = src[col * 4 + 3];
		}
	}

	bool ResizeRgba(FFrame::ScaleContext& ctx, const std::uint8_t* src, int srcW, int srcH,
		int dstW, int dstH, StormByte::Safe::Vector<std::uint8_t>& dst) noexcept {
		FFrame packed;
		if (!StorePacked(packed, src, srcW, srcH))
			return false;
		FFrame color;
		color.Format(AV_PIX_FMT_GBRP);
		if (!packed.ScaleTo(ctx, color, srcW, srcH, FFrame::Resample::Point, FFrame::Scaler::Sws))
			return false;
		TagSrgb(color);
		FFrame alpha;
		if (!alpha.AllocVideo(srcW, srcH, AV_PIX_FMT_GRAY8) || !alpha.Data(0))
			return false;
		ExtractAlpha(packed, alpha);
		alpha.ColorRange(AVCOL_RANGE_JPEG);
		FFrame colorOut;
		colorOut.Format(AV_PIX_FMT_GBRP);
		if (!color.ScaleTo(ctx, colorOut, dstW, dstH, FFrame::Resample::Lanczos, FFrame::Scaler::Zimg)
			|| !colorOut.Data(0))
			return false;
		FFrame alphaOut;
		alphaOut.Format(AV_PIX_FMT_GRAY8);
		if (!alpha.ScaleTo(ctx, alphaOut, dstW, dstH, FFrame::Resample::Lanczos, FFrame::Scaler::Zimg)
			|| !alphaOut.Data(0))
			return false;
		FFrame packedOut;
		packedOut.Format(AV_PIX_FMT_RGBA);
		if (!colorOut.ScaleTo(ctx, packedOut, dstW, dstH, FFrame::Resample::Point, FFrame::Scaler::Sws)
			|| !packedOut.Data(0))
			return false;
		dst.resize(static_cast<std::size_t>(dstW) * static_cast<std::size_t>(dstH) * 4);
		for (int row = 0; row < dstH; ++row) {
			const std::uint8_t* rgb = packedOut.Data(0) + row * packedOut.Linesize(0);
			const std::uint8_t* a = alphaOut.Data(0) + row * alphaOut.Linesize(0);
			std::uint8_t* out = dst.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(dstW) * 4;
			for (int col = 0; col < dstW; ++col) {
				out[col * 4 + 0] = rgb[col * 4 + 0];
				out[col * 4 + 1] = rgb[col * 4 + 1];
				out[col * 4 + 2] = rgb[col * 4 + 2];
				out[col * 4 + 3] = a[col];
			}
		}
		return true;
	}

	void ScaleLinearRgb(FFrame& frame, float gain) noexcept {
		if (gain == 1.f || !frame.Data(0))
			return;
		for (int plane = 0; plane < 3; ++plane) {
			if (!frame.Data(plane))
				return;
			for (int row = 0; row < frame.Height(); ++row) {
				auto* samples = reinterpret_cast<float*>(frame.Data(plane) + row * frame.Linesize(plane));
				for (int col = 0; col < frame.Width(); ++col)
					samples[col] *= gain;
			}
		}
	}

	bool CanComposite(const AVPixFmtDescriptor* desc) noexcept {
		if (!desc || (desc->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_PAL | AV_PIX_FMT_FLAG_BITSTREAM)) != 0)
			return false;
		if (desc->nb_components >= 3 && desc->comp[1].plane == desc->comp[2].plane
			&& desc->comp[1].plane != desc->comp[0].plane)
			return false;
		if ((desc->flags & AV_PIX_FMT_FLAG_RGB) != 0 && (desc->flags & AV_PIX_FMT_FLAG_PLANAR) == 0)
			return desc->log2_chroma_w == 0 && desc->log2_chroma_h == 0;
		return (desc->flags & AV_PIX_FMT_FLAG_PLANAR) != 0 || desc->nb_components == 1;
	}

	int PlaneShift(const AVPixFmtDescriptor* desc, int plane, bool vertical) noexcept {
		if ((desc->flags & AV_PIX_FMT_FLAG_PLANAR) == 0 || plane == 0
			|| (desc->flags & AV_PIX_FMT_FLAG_RGB) != 0)
			return 0;
		return vertical ? desc->log2_chroma_h : desc->log2_chroma_w;
	}

	struct Box {
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
	};

	Box AlignBox(int placeX, int placeY, int logoW, int logoH, int frameW, int frameH,
		int hshift, int vshift) noexcept {
		const int maskX = (1 << hshift) - 1;
		const int maskY = (1 << vshift) - 1;
		int x0 = std::max(0, placeX & ~maskX);
		int y0 = std::max(0, placeY & ~maskY);
		int x1 = (placeX + logoW + maskX) & ~maskX;
		int y1 = (placeY + logoH + maskY) & ~maskY;
		const int limitX = frameW & ~maskX;
		const int limitY = frameH & ~maskY;
		if (x1 > limitX)
			x1 = limitX;
		if (y1 > limitY)
			y1 = limitY;
		if (x1 < x0)
			x1 = x0;
		if (y1 < y0)
			y1 = y0;
		return {x0, y0, x1 - x0, y1 - y0};
	}

	bool ExtractTile(const FFrame& src, FFrame& tile, int x, int y, int w, int h,
		const AVPixFmtDescriptor* desc) noexcept {
		if (!tile.AllocVideo(w, h, src.Format()))
			return false;
		const bool packed = (desc->flags & AV_PIX_FMT_FLAG_PLANAR) == 0;
		const int planes = std::min(src.PlaneCount(), 4);
		for (int plane = 0; plane < planes; ++plane) {
			if (!src.Data(plane) || !tile.Data(plane))
				return false;
			const int shiftX = PlaneShift(desc, plane, false);
			const int shiftY = PlaneShift(desc, plane, true);
			const int copyW = w >> shiftX;
			const int copyH = h >> shiftY;
			const int sample = packed
				? std::max(1, desc->comp[0].step)
				: std::max(1, (desc->comp[std::min(plane, desc->nb_components - 1)].depth + 7) / 8);
			const int rowBytes = copyW * sample;
			const int srcX = (x >> shiftX) * sample;
			for (int row = 0; row < copyH; ++row) {
				const std::uint8_t* in = src.Data(plane) + ((y >> shiftY) + row) * src.Linesize(plane) + srcX;
				std::uint8_t* out = tile.Data(plane) + row * tile.Linesize(plane);
				std::copy(in, in + rowBytes, out);
			}
		}
		return true;
	}

	bool Covered(const std::uint8_t* rgba, int logoW, int logoH, int frameX, int frameY,
		int placeX, int placeY, unsigned opacity) noexcept {
		const int lx = frameX - placeX;
		const int ly = frameY - placeY;
		if (lx < 0 || ly < 0 || lx >= logoW || ly >= logoH)
			return false;
		const int alpha = static_cast<int>(rgba[(static_cast<std::size_t>(ly) * static_cast<std::size_t>(logoW)
			+ static_cast<std::size_t>(lx)) * 4 + 3]);
		return (alpha * static_cast<int>(opacity)) / 100 > 0;
	}

	bool BlockCovered(const std::uint8_t* rgba, int logoW, int logoH, int frameX, int frameY,
		int placeX, int placeY, unsigned opacity, int spanX, int spanY) noexcept {
		for (int dy = 0; dy < spanY; ++dy) {
			for (int dx = 0; dx < spanX; ++dx) {
				if (Covered(rgba, logoW, logoH, frameX + dx, frameY + dy, placeX, placeY, opacity))
					return true;
			}
		}
		return false;
	}

	void Scatter(const FFrame& tile, FFrame& dst, Box box, const std::uint8_t* rgba,
		int logoW, int logoH, int placeX, int placeY, unsigned opacity,
		const AVPixFmtDescriptor* desc) noexcept {
		const bool packed = (desc->flags & AV_PIX_FMT_FLAG_PLANAR) == 0;
		const bool keepAlpha = (desc->flags & AV_PIX_FMT_FLAG_ALPHA) != 0;
		const int planes = std::min(std::min(tile.PlaneCount(), dst.PlaneCount()), 4);
		const int colorPlanes = (keepAlpha && !packed) ? std::max(1, planes - 1) : planes;
		int alphaOffset = -1;
		int alphaBytes = 0;
		if (keepAlpha && packed && desc->nb_components > 0) {
			const auto& component = desc->comp[desc->nb_components - 1];
			alphaOffset = component.offset;
			alphaBytes = std::max(1, (component.depth + 7) / 8);
		}
		for (int plane = 0; plane < colorPlanes; ++plane) {
			if (!tile.Data(plane) || !dst.Data(plane))
				continue;
			const int shiftX = PlaneShift(desc, plane, false);
			const int shiftY = PlaneShift(desc, plane, true);
			const int copyW = box.w >> shiftX;
			const int copyH = box.h >> shiftY;
			const int sample = packed
				? std::max(1, desc->comp[0].step)
				: std::max(1, (desc->comp[std::min(plane, desc->nb_components - 1)].depth + 7) / 8);
			const int spanX = 1 << shiftX;
			const int spanY = 1 << shiftY;
			for (int row = 0; row < copyH; ++row) {
				for (int col = 0; col < copyW; ++col) {
					const int frameX = box.x + (col << shiftX);
					const int frameY = box.y + (row << shiftY);
					if (!BlockCovered(rgba, logoW, logoH, frameX, frameY, placeX, placeY, opacity, spanX, spanY))
						continue;
					std::uint8_t* pixel = dst.Data(plane)
						+ (static_cast<std::size_t>(box.y >> shiftY) + static_cast<std::size_t>(row))
							* static_cast<std::size_t>(dst.Linesize(plane))
						+ static_cast<std::size_t>((box.x >> shiftX) + col) * static_cast<std::size_t>(sample);
					const std::uint8_t* src = tile.Data(plane) + row * tile.Linesize(plane)
						+ static_cast<std::size_t>(col) * static_cast<std::size_t>(sample);
					if (packed && alphaOffset >= 0 && alphaBytes > 0 && alphaOffset + alphaBytes <= sample) {
						std::uint8_t saved[8] {};
						const int bytes = std::min(alphaBytes, 8);
						std::copy(pixel + alphaOffset, pixel + alphaOffset + bytes, saved);
						std::copy(src, src + sample, pixel);
						std::copy(saved, saved + bytes, pixel + alphaOffset);
					}
					else
						std::copy(src, src + sample, pixel);
				}
			}
		}
	}

	float PqEncode(float linear) noexcept {
		constexpr float m1 = 2610.f / 16384.f, m2 = 2523.f / 32.f;
		constexpr float c1 = 3424.f / 4096.f, c2 = 2413.f / 128.f, c3 = 2392.f / 128.f;
		const float y = std::pow(std::max(linear, 0.f), m1);
		return std::pow((c1 + c2 * y) / (1.f + c3 * y), m2);
	}

	float PqDecode(float encoded) noexcept {
		constexpr float m1 = 2610.f / 16384.f, m2 = 2523.f / 32.f;
		constexpr float c1 = 3424.f / 4096.f, c2 = 2413.f / 128.f, c3 = 2392.f / 128.f;
		const float e = std::pow(std::clamp(encoded, 0.f, 1.f), 1.f / m2);
		return std::pow(std::max(e - c1, 0.f) / (c2 - c3 * e), 1.f / m1);
	}

	// Opacity is perceptual (as in image editors): mixing in linear light makes a
	// bright logo over dark video look almost opaque, especially on PQ content.
	float Mix(float under, float over, float a, bool hdr) noexcept {
		if (hdr)
			return PqDecode(PqEncode(under) * (1.f - a) + PqEncode(over) * a);
		constexpr float gamma = 2.4f;
		const float u = std::pow(std::max(under, 0.f), 1.f / gamma);
		const float o = std::pow(std::max(over, 0.f), 1.f / gamma);
		return std::pow(u * (1.f - a) + o * a, gamma);
	}

	void BlendLinear(FFrame& tile, const FFrame& logo, int placeX, int placeY, int tileX, int tileY,
		const std::uint8_t* rgba, int logoW, int logoH, unsigned opacity, bool hdr) noexcept {
		if (!tile.Data(0) || !tile.Data(1) || !tile.Data(2) || !logo.Data(0) || !logo.Data(1) || !logo.Data(2))
			return;
		const float opacityScale = static_cast<float>(opacity) / 100.f;
		const int originX = placeX - tileX;
		const int originY = placeY - tileY;
		for (int row = 0; row < logoH; ++row) {
			const int ty = originY + row;
			if (ty < 0 || ty >= tile.Height())
				continue;
			auto* tileG = reinterpret_cast<float*>(tile.Data(0) + ty * tile.Linesize(0));
			auto* tileB = reinterpret_cast<float*>(tile.Data(1) + ty * tile.Linesize(1));
			auto* tileR = reinterpret_cast<float*>(tile.Data(2) + ty * tile.Linesize(2));
			const auto* logoG = reinterpret_cast<const float*>(logo.Data(0) + row * logo.Linesize(0));
			const auto* logoB = reinterpret_cast<const float*>(logo.Data(1) + row * logo.Linesize(1));
			const auto* logoR = reinterpret_cast<const float*>(logo.Data(2) + row * logo.Linesize(2));
			const std::uint8_t* alpha = rgba + static_cast<std::size_t>(row) * static_cast<std::size_t>(logoW) * 4;
			for (int col = 0; col < logoW; ++col) {
				const int tx = originX + col;
				if (tx < 0 || tx >= tile.Width())
					continue;
				const float a = (static_cast<float>(alpha[col * 4 + 3]) / 255.f) * opacityScale;
				if (a <= 0.f)
					continue;
				tileG[tx] = Mix(tileG[tx], logoG[col], a, hdr);
				tileB[tx] = Mix(tileB[tx], logoB[col], a, hdr);
				tileR[tx] = Mix(tileR[tx], logoR[col], a, hdr);
			}
		}
	}
}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::String logo, Anchor anchor, unsigned opacity, int margin,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Resolution> designedFor) noexcept
: Watermark(std::move(log), std::move(logo), StormByte::Safe::Binary{},
	StormByte::Safe::Optional<Anchor>{anchor}, {}, opacity, margin, false, std::move(designedFor)) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	const StormByte::Safe::Binary& logo, Anchor anchor, unsigned opacity, int margin,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Resolution> designedFor) noexcept
: Watermark(std::move(log), StormByte::Safe::String{}, logo,
	StormByte::Safe::Optional<Anchor>{anchor}, {}, opacity, margin, true, std::move(designedFor)) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::String logo, StormByte::Multimedia::Property::Point position,
	unsigned opacity,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Resolution> designedFor) noexcept
: Watermark(std::move(log), std::move(logo), StormByte::Safe::Binary{}, {},
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Point>{position},
	opacity, 0, false, std::move(designedFor)) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	const StormByte::Safe::Binary& logo, StormByte::Multimedia::Property::Point position,
	unsigned opacity,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Resolution> designedFor) noexcept
: Watermark(std::move(log), StormByte::Safe::String{}, logo, {},
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Point>{position},
	opacity, 0, true, std::move(designedFor)) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::String path, const StormByte::Safe::Binary& logo,
	StormByte::Safe::Optional<Anchor> anchor,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Point> position,
	unsigned opacity, int margin, bool binaryInput,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Resolution> designedFor) noexcept
: Filter::Process(std::move(log), "watermark"),
	m_logo(logo),
	m_anchor(std::move(anchor)), m_point(std::move(position)),
	m_opacity(std::min(opacity, 100u)), m_margin(margin),
	m_designed(std::move(designedFor)),
	m_scaledMargin(margin),
	m_paintW(0), m_paintH(0), m_preparedW(0), m_preparedH(0),
	m_linearTrc(0), m_linearPri(0), m_linearMat(0), m_linearRange(0),
	m_prepared(false), m_linearReady(false), m_legacyNoted(false),
	m_logoWidth(0), m_logoHeight(0), m_decoded(false),
	m_released(false), m_barTop(0), m_barBottom(0), m_barLeft(0), m_barRight(0),
	m_stable(0), m_lumaW(0), m_lumaH(0), m_lumaFmt(FFrame::FormatNone()) {
	if (m_designed && m_designed->Width() == 0 && m_designed->Height() == 0)
		DisableLogo("design resolution is 0x0");
	if (!binaryInput && m_opacity != 0)
		static_cast<void>(LoadFile(path));
}

Watermark::~Watermark() noexcept {
	DropScale();
}

enum StormByte::Multimedia::Type Watermark::Media() const noexcept {
	return StormByte::Multimedia::Type::Video;
}

void Watermark::DropScale() noexcept {
	m_luma.reset();
	m_lumaW = 0;
	m_lumaH = 0;
	m_lumaFmt = FFrame::FormatNone();
}

void Watermark::Clean() noexcept {
	m_rgba.clear();
	m_scaled.clear();
	m_linear.reset();
	m_logoWidth = 0;
	m_logoHeight = 0;
	m_paintW = 0;
	m_paintH = 0;
	m_preparedW = 0;
	m_preparedH = 0;
	m_decoded = false;
	m_prepared = false;
	m_linearReady = false;
	m_legacyNoted = false;
	m_scaledMargin = m_margin;
	m_released = false;
	m_barTop = 0;
	m_barBottom = 0;
	m_barLeft = 0;
	m_barRight = 0;
	m_stable = 0;
	DropScale();
}

void Watermark::Setup() noexcept {
	if (m_designed)
		Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("opacity {}%, margin {}, designed {}x{}",
			m_opacity, m_margin, m_designed->Width(), m_designed->Height()));
	else
		Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("opacity {}%, margin {}", m_opacity, m_margin));
}

void Watermark::DisableLogo(std::string_view why) noexcept {
	Log(Level::Warning, StormByte::Safe::String::ReplaceToken("disabled: {}", why));
	m_opacity = 0;
	m_rgba.clear();
	m_scaled.clear();
	m_linear.reset();
	m_logoWidth = 0;
	m_logoHeight = 0;
	m_paintW = 0;
	m_paintH = 0;
	m_prepared = false;
	m_linearReady = false;
}

bool Watermark::LoadFile(const StormByte::Safe::String& path) noexcept {
	if (path.empty()) {
		DisableLogo("empty logo path");
		return false;
	}
	const std::filesystem::path nativePath(std::u8string_view(
		reinterpret_cast<const char8_t*>(path.data()), static_cast<std::size_t>(path.size())));
	std::ifstream in(nativePath, std::ios::binary);
	if (!in) {
		DisableLogo("cannot open logo file");
		return false;
	}

	in.seekg(0, std::ios::end);
	const auto size = in.tellg();
	if (size <= 0) {
		DisableLogo("empty logo");
		return false;
	}

	in.seekg(0, std::ios::beg);
	m_logo.resize(static_cast<std::size_t>(size));
	in.read(reinterpret_cast<char*>(m_logo.data()), static_cast<std::streamsize>(size));
	if (!in) {
		m_logo.clear();
		DisableLogo("failed to read logo file");
		return false;
	}

	return true;
}

bool Watermark::DecodeLogo() noexcept {
	if (m_decoded)
		return !m_rgba.empty();
	m_decoded = true;
	if (m_logo.empty()) {
		DisableLogo("empty logo");
		return false;
	}
	const std::string hint{ImageHint(m_logo)};
	FFrame decoded = FFrame::DecodeImage(
		reinterpret_cast<const std::uint8_t*>(m_logo.data()),
		static_cast<std::size_t>(m_logo.size()), hint);
	if (!decoded || decoded.Width() <= 0 || decoded.Height() <= 0) {
		DisableLogo("failed to decode logo");
		return false;
	}

	FFrame rgba;
	rgba.Format(FFrame::FormatRgba());
	if (!decoded.ScaleTo(ScalingContext(), rgba, decoded.Width(), decoded.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws)) {
		DisableLogo("failed to convert logo to RGBA");
		return false;
	}

	m_logoWidth = rgba.Width();
	m_logoHeight = rgba.Height();
	m_rgba.resize(static_cast<std::size_t>(m_logoWidth) * static_cast<std::size_t>(m_logoHeight) * 4);
	for (int row = 0; row < m_logoHeight; ++row) {
		const uint8_t* src = rgba.Data(0) + row * rgba.Linesize(0);
		uint8_t* dst = m_rgba.data() +
			static_cast<std::size_t>(row) * static_cast<std::size_t>(m_logoWidth) * 4;
		std::copy(src, src + m_logoWidth * 4, dst);
	}

	return true;
}

const FFrame* Watermark::Luma(const FFrame& src) noexcept {
	if (!src || src.Width() <= 0 || src.Height() <= 0)
		return nullptr;
	if (src.Hardware()) {
		Fail("watermark needs a software frame");
		return nullptr;
	}

	if (m_luma && (m_lumaW != src.Width() || m_lumaH != src.Height() || m_lumaFmt != src.Format()))
		DropScale();

	if (!m_luma) {
		auto luma = StormByte::Safe::MakeUnique<FFrame>();
		luma->Format(FFrame::FormatGray8());
			if (!src.ScaleTo(ScalingContext(), *luma, src.Width(), src.Height(),
				FFrame::Resample::Default, FFrame::Scaler::Sws)) {
			Fail("failed to allocate luma probe");
			return nullptr;
		}

		m_luma = std::move(luma);
		m_lumaW = src.Width();
		m_lumaH = src.Height();
		m_lumaFmt = src.Format();
	}
	else if (!src.ScaleTo(ScalingContext(), *m_luma, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws)) {
		Fail("failed to convert frame to luma");
		return nullptr;
	}

	return m_luma.get();
}

bool Watermark::ProbeBars(const FFrame& src) noexcept {
	if (!Luma(src) || m_luma->Width() < 16 || m_luma->Height() < 16) {
			Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("probe skip luma={}x{}",
			m_luma ? m_luma->Width() : 0,
			m_luma ? m_luma->Height() : 0));
		return false;
	}
	FFrame& gray = *m_luma;

	Probe found = Measure(gray);
	const bool rawBoxed = Boxed(found);

	if (!Boxed(found)) {
		const int edge = std::min({
			RowMeanY8(gray, 0),
			RowMeanY8(gray, gray.Height() - 1),
			ColMeanY8(gray, 0),
			ColMeanY8(gray, gray.Width() - 1)
		});
		const int core = std::max(
			RowMeanY8(gray, gray.Height() / 2),
			ColMeanY8(gray, gray.Width() / 2));
		if (core > edge) {
			ApplyLut(gray, StretchLut(edge, core));
			found = Measure(gray);
					Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken(
				"probe stretch edge={} core={} found={},{};{},{}",
				edge, core, found.top, found.bottom, found.left, found.right));
		}
	}

	if (!Boxed(found)) {
		ApplyLut(gray, GammaLut(0.45));
		found = Measure(gray);
			Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("probe gamma found={},{};{},{}",
			found.top, found.bottom, found.left, found.right));
	}

	if (!Boxed(found)) {
			Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken(
			"probe unboxed raw={} found={},{};{},{} bars={},{};{},{} stable={}",
			static_cast<int>(rawBoxed), found.top, found.bottom, found.left, found.right,
			m_barTop, m_barBottom, m_barLeft, m_barRight, m_stable));
		return false;
	}

	const bool seeded = m_barTop != 0 || m_barBottom != 0
		|| m_barLeft != 0 || m_barRight != 0;
	const bool same = seeded
		&& NearBar(found.top, m_barTop)
		&& NearBar(found.bottom, m_barBottom)
		&& NearBar(found.left, m_barLeft)
		&& NearBar(found.right, m_barRight);

	if (found.top > 0 && found.bottom > 0) {
		if (m_barTop == 0) {
			m_barTop = found.top;
			m_barBottom = found.bottom;
		}
		else {
			m_barTop = std::min(m_barTop, found.top);
			m_barBottom = std::min(m_barBottom, found.bottom);
		}
	}

	if (found.left > 0 && found.right > 0) {
		if (m_barLeft == 0) {
			m_barLeft = found.left;
			m_barRight = found.right;
		}
		else {
			m_barLeft = std::min(m_barLeft, found.left);
			m_barRight = std::min(m_barRight, found.right);
		}
	}

	if (same)
		++m_stable;
	else
		m_stable = 0;

	Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken(
		"probe same={} stable={} found={},{};{},{} bars={},{};{},{}",
		static_cast<int>(same), m_stable,
		found.top, found.bottom, found.left, found.right,
		m_barTop, m_barBottom, m_barLeft, m_barRight));
	return true;
}

void Watermark::Process(const Pipeline::Frame& frame) noexcept {
	if (m_opacity == 0)
		return;
	Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("process t={} {}:{} opacity={} released={}",
		frame.Track(), frame.Serial().value_or(0), frame.Part(), m_opacity,
		static_cast<int>(m_released)));

	const FFrame& src = AVFrame();

	if (m_anchor && !m_point && !m_released) {
		if (!Held())
			Hold(ProbeMax);
		const bool usable = ProbeBars(src);
		const bool boxed = (m_barTop > 4 && m_barBottom > 4)
			|| (m_barLeft > 4 && m_barRight > 4);
		Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken(
			"hold-state t={} {}:{} usable={} boxed={} stable={} held={} bars={},{};{},{}",
			frame.Track(), frame.Serial().value_or(0), frame.Part(),
			static_cast<int>(usable), static_cast<int>(boxed), m_stable,
			static_cast<unsigned>(HeldFor()),
			m_barTop, m_barBottom, m_barLeft, m_barRight));
		if (usable && boxed && m_stable >= 8) {
			Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("stable-release t={} {}:{}",
				frame.Track(), frame.Serial().value_or(0), frame.Part()));
			m_released = true;
			Release();
			return;
		}

		if (Held())
			return;
		m_released = true;
	}

	Paint();
}

void Watermark::LastChance(const Pipeline::Frame& frame) noexcept {
	Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken(
		"last-chance t={} {}:{} stable={} bars={},{};{},{}",
		frame.Track(), frame.Serial().value_or(0), frame.Part(),
		m_stable, m_barTop, m_barBottom, m_barLeft, m_barRight));
	m_released = true;
	Release();
}

bool Watermark::Prepare(int frameW, int frameH) noexcept {
	if (m_prepared && m_preparedW == frameW && m_preparedH == frameH)
		return m_opacity != 0;
	m_linear.reset();
	m_linearReady = false;
	m_scaled.clear();
	m_scaledMargin = m_margin;
	m_paintW = m_logoWidth;
	m_paintH = m_logoHeight;
	if (m_designed) {
		const auto designW = m_designed->Width();
		const auto designH = m_designed->Height();
		if (designW == 0 && designH == 0) {
			DisableLogo("design resolution is 0x0");
			return false;
		}
		double factor = 1.0;
		if (designW == 0)
			factor = static_cast<double>(frameH) / static_cast<double>(designH);
		else if (designH == 0)
			factor = static_cast<double>(frameW) / static_cast<double>(designW);
		else
			factor = std::min(static_cast<double>(frameW) / static_cast<double>(designW),
				static_cast<double>(frameH) / static_cast<double>(designH));
		if (!std::isfinite(factor) || factor <= 0.0) {
			DisableLogo("design resolution is invalid");
			return false;
		}
		m_paintW = ScaledDim(m_logoWidth, factor);
		m_paintH = ScaledDim(m_logoHeight, factor);
		if (!m_point) {
			const auto margin = std::llround(static_cast<double>(m_margin) * factor);
			m_scaledMargin = static_cast<int>(std::clamp(margin,
				static_cast<long long>(INT_MIN), static_cast<long long>(INT_MAX)));
		}
		if ((m_paintW != m_logoWidth || m_paintH != m_logoHeight)
			&& !ResizeRgba(ScalingContext(), m_rgba.data(), m_logoWidth, m_logoHeight,
				m_paintW, m_paintH, m_scaled)) {
			DisableLogo("failed to scale logo");
			return false;
		}
		Log(Level::LowLevel, StormByte::Safe::String::ReplaceToken("logo {}x{} frame {}x{} -> {}x{} margin {}",
			m_logoWidth, m_logoHeight, frameW, frameH, m_paintW, m_paintH, m_scaledMargin));
	}
	m_prepared = true;
	m_preparedW = frameW;
	m_preparedH = frameH;
	return true;
}

bool Watermark::EnsureLinear(const FFrame& src) noexcept {
	const bool hdr = HdrTransfer(src.ColorTransfer());
	const int primaries = WorkingPrimaries(src);
	const int matrix = src.ColorSpace();
	const int range = src.ColorRange();
	const int transfer = src.ColorTransfer();
	if (m_linearReady && m_linear && m_linear->Width() == m_paintW && m_linear->Height() == m_paintH
		&& m_linearTrc == transfer && m_linearPri == primaries
		&& m_linearMat == matrix && m_linearRange == range)
		return m_linear->Data(0) != nullptr;
	const std::uint8_t* bytes = m_scaled.empty() ? m_rgba.data() : m_scaled.data();
	FFrame packed;
	if (!StorePacked(packed, bytes, m_paintW, m_paintH))
		return false;
	FFrame planar;
	planar.Format(AV_PIX_FMT_GBRP);
	if (!packed.ScaleTo(ScalingContext(), planar, m_paintW, m_paintH,
			FFrame::Resample::Point, FFrame::Scaler::Sws) || !planar.Data(0))
		return false;
	TagSrgb(planar);
	auto linear = StormByte::Safe::MakeUnique<FFrame>();
	linear->Format(AV_PIX_FMT_GBRPF32LE);
	if (!planar.ScaleTo(ScalingContext(), *linear, m_paintW, m_paintH,
			FFrame::Resample::Point, FFrame::Scaler::Zimg, LinearTarget(primaries, hdr))
		|| !linear->Data(0) || !linear->Data(1) || !linear->Data(2))
		return false;
	if (hdr)
		ScaleLinearRgb(*linear, static_cast<float>(GraphicsWhite / HdrPeak));
	m_linear = std::move(linear);
	m_linearTrc = transfer;
	m_linearPri = primaries;
	m_linearMat = matrix;
	m_linearRange = range;
	m_linearReady = true;
	return true;
}

bool Watermark::Composite(const FFrame& src, int x, int y) noexcept {
	const auto* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(src.Format()));
	if (!CanComposite(desc))
		return false;
	if (!EnsureLinear(src) || !m_linear)
		return false;
	const bool rgb = (desc->flags & AV_PIX_FMT_FLAG_RGB) != 0;
	const int hshift = rgb ? 0 : desc->log2_chroma_w;
	const int vshift = rgb ? 0 : desc->log2_chroma_h;
	const Box box = AlignBox(x, y, m_paintW, m_paintH, src.Width(), src.Height(), hshift, vshift);
	if (box.w <= 0 || box.h <= 0 || box.x > x || box.y > y
		|| box.x + box.w < x + m_paintW || box.y + box.h < y + m_paintH)
		return false;
	FFrame tile;
	if (!ExtractTile(src, tile, box.x, box.y, box.w, box.h, desc))
		return false;
	const bool hdr = HdrTransfer(src.ColorTransfer());
	const int primaries = WorkingPrimaries(src);
	tile.ColorTransfer(WorkingTransfer(src));
	tile.ColorPrimaries(primaries);
	tile.ColorSpace(WorkingMatrix(src, rgb));
	tile.ColorRange(WorkingRange(src, rgb));
	tile.ChromaLocation(src.ChromaLocation());
	FFrame linear;
	linear.Format(AV_PIX_FMT_GBRPF32LE);
	if (!tile.ScaleTo(ScalingContext(), linear, box.w, box.h,
			FFrame::Resample::Point, FFrame::Scaler::Zimg, LinearTarget(primaries, hdr))
		|| !linear.Data(0) || !linear.Data(1) || !linear.Data(2))
		return false;
	const std::uint8_t* rgba = m_scaled.empty() ? m_rgba.data() : m_scaled.data();
	BlendLinear(linear, *m_linear, x, y, box.x, box.y, rgba, m_paintW, m_paintH, m_opacity, hdr);
	linear.ColorTransfer(AVCOL_TRC_LINEAR);
	linear.ColorPrimaries(primaries);
	linear.ColorSpace(AVCOL_SPC_RGB);
	linear.ColorRange(AVCOL_RANGE_JPEG);
	FFrame::ColorTarget back;
	back.transfer = WorkingTransfer(src);
	back.primaries = primaries;
	back.matrix = WorkingMatrix(src, rgb);
	back.range = WorkingRange(src, rgb);
	back.peak = hdr ? HdrPeak : 100.0;
	FFrame restored;
	restored.Format(src.Format());
	if (!linear.ScaleTo(ScalingContext(), restored, box.w, box.h,
			FFrame::Resample::Point, FFrame::Scaler::Zimg, back) || !restored.Data(0))
		return false;
	FFrame out;
	if (!out.AllocVideo(src.Width(), src.Height(), src.Format()) || !out.Copy(src) || !out.CopyProps(src)) {
		Fail("failed to allocate destination");
		return true;
	}
	Scatter(restored, out, box, rgba, m_paintW, m_paintH, x, y, m_opacity, desc);
	Save(std::move(out));
	return true;
}

void Watermark::PaintLegacy(int x, int y) noexcept {
	const FFrame& src = AVFrame();
	const std::uint8_t* logo = m_scaled.empty() ? m_rgba.data() : m_scaled.data();
	FFrame out;
	out.Format(FFrame::FormatRgba());
	if (!src.ScaleTo(ScalingContext(), out, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws)) {
		Fail("failed to convert frame to RGBA");
		return;
	}
	if (!out.Data(0)) {
		Fail("failed to allocate destination");
		return;
	}
	Blend(out.Data(0), out.Linesize(0), out.Width(), out.Height(),
		logo, m_paintW, m_paintH, x, y, m_opacity);
	if (src.Format() != FFrame::FormatRgba()) {
		FFrame restored;
		restored.Format(src.Format());
		if (!out.ScaleTo(ScalingContext(), restored, src.Width(), src.Height(),
				FFrame::Resample::Default, FFrame::Scaler::Sws)) {
			Fail("failed to convert frame back");
			return;
		}
		Save(std::move(restored));
		return;
	}
	Save(std::move(out));
}

void Watermark::Paint() noexcept {
	if (m_opacity == 0)
		return;
	if (!DecodeLogo())
		return;

	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Fail("missing video buffer");
		return;
	}

	if (src.Hardware()) {
		Fail("watermark needs a software frame");
		return;
	}

	const int x0 = m_point ? 0 : m_barLeft;
	const int y0 = m_point ? 0 : m_barTop;
	const int aw = m_point ? src.Width() : src.Width() - m_barLeft - m_barRight;
	const int ah = m_point ? src.Height() : src.Height() - m_barTop - m_barBottom;
	if (aw <= 0 || ah <= 0) {
		DisableLogo("active picture is empty");
		return;
	}

	if (!Prepare(src.Width(), src.Height()))
		return;

	const auto [x, y] = Place(m_paintW, m_paintH, m_anchor, m_point,
		m_scaledMargin, x0, y0, aw, ah);
	if (x < x0 || y < y0 || x + m_paintW > x0 + aw || y + m_paintH > y0 + ah) {
		DisableLogo("logo does not fit in the active picture");
		return;
	}

	if (!Composite(src, x, y)) {
		if (!m_legacyNoted) {
			Log(Level::LowLevel, "rectangle composite unavailable");
			m_legacyNoted = true;
		}
		PaintLegacy(x, y);
	}
}
