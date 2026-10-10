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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/nlmeans.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::NlMeans;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

namespace {
	int Bpc(const FFrame& raw) noexcept {
		if (!raw)
			return 8;
		const int depth = raw.BitsPerComponent();
		return depth <= 8 ? 8 : (depth <= 10 ? 10 : 12);
	}

	int Pel(const uint8_t* base, int stride, int x, int y, int bpc) noexcept {
		const uint8_t* row = base + y * stride;
		if (bpc <= 8)
			return row[x];
		return reinterpret_cast<const uint16_t*>(row)[x];
	}

	void Put(uint8_t* row, int x, int bpc, int value) noexcept {
		const int maxv = (1 << bpc) - 1;
		value = std::clamp(value, 0, maxv);
		if (bpc <= 8)
			row[x] = static_cast<uint8_t>(value);
		else
			reinterpret_cast<uint16_t*>(row)[x] = static_cast<uint16_t>(value);
	}

	double PatchDist(const uint8_t* s, int stride, int x, int y,
		int nx, int ny, int p, int w, int h, int bpc, int peak) noexcept {
		double acc = 0.0;
		unsigned n = 0;
		for (int j = -p; j <= p; ++j) {
			const int y0 = std::clamp(y + j, 0, h - 1);
			const int y1 = std::clamp(ny + j, 0, h - 1);
			for (int i = -p; i <= p; ++i) {
				const int x0 = std::clamp(x + i, 0, w - 1);
				const int x1 = std::clamp(nx + i, 0, w - 1);
				const int d = Pel(s, stride, x0, y0, bpc) - Pel(s, stride, x1, y1, bpc);
				acc += static_cast<double>(d) * static_cast<double>(d);
				++n;
			}
		}
		const double norm = static_cast<double>(peak) * static_cast<double>(peak) * static_cast<double>(n);
		return acc / norm;
	}

	void FilterPlane(const FFrame& src, FFrame& dst, int plane, int bpc,
		int research, int patch, double h) noexcept {
		const int w = src.PlaneWidth(plane);
		const int hgt = src.PlaneHeight(plane);
		const uint8_t* s = src.Data(plane);
		uint8_t* d = dst.Data(plane);
		const int ss = src.Linesize(plane);
		const int ds = dst.Linesize(plane);
		if (!s || !d || w <= 0 || hgt <= 0)
			return;

		const int peak = (1 << bpc) - 1;
		const double inv = 1.0 / std::max(1e-8, h * h);
		for (int y = 0; y < hgt; ++y) {
			uint8_t* drow = d + y * ds;
			for (int x = 0; x < w; ++x) {
				double acc = 0.0;
				double wgt = 0.0;
				const int y0 = std::max(0, y - research);
				const int y1 = std::min(hgt - 1, y + research);
				const int x0 = std::max(0, x - research);
				const int x1 = std::min(w - 1, x + research);
				for (int ny = y0; ny <= y1; ++ny) {
					for (int nx = x0; nx <= x1; ++nx) {
						const double dist = PatchDist(s, ss, x, y, nx, ny, patch, w, hgt, bpc, peak);
						const double ww = std::exp(-dist * inv);
						acc += static_cast<double>(Pel(s, ss, nx, ny, bpc)) * ww;
						wgt += ww;
					}
				}
				Put(drow, x, bpc, static_cast<int>(acc / std::max(wgt, 1e-8) + 0.5));
			}
		}
	}
}

NlMeans::NlMeans(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<unsigned> research, StormByte::Safe::Optional<unsigned> patch,
	StormByte::Safe::Optional<double> strength) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("nlmeans")),
	m_researchIn(research), m_patchIn(patch), m_hIn(strength),
	m_research(0), m_patch(0), m_h(0.0), m_latched(false) {}

NlMeans::~NlMeans() noexcept = default;

enum Type NlMeans::Media() const noexcept {
	return Type::Video;
}

void NlMeans::Clean() noexcept {
	m_latched = false;
}

void NlMeans::Setup() noexcept {
	Clean();
}

void NlMeans::Latch(int width, int height) noexcept {
	const bool uhd = width >= 3840 || height >= 2160;
	m_research = m_researchIn.value_or(uhd ? 2u : 3u);
	m_patch = m_patchIn.value_or(uhd ? 1u : 2u);
	m_h = m_hIn.value_or(uhd ? 0.8 : 1.0);
	m_latched = true;
	Log(Level::Debug, std::format("nlmeans latch {}x{} r={} p={} h={:.3f} uhd={}",
		width, height, m_research, m_patch, m_h, uhd));
}

void NlMeans::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "nlmeans: frame has no picture");
		return;
	}

	if (!m_latched)
		Latch(src.Width(), src.Height());

	FFrame out;
	if (!out.AllocVideo(src.Width(), src.Height(), src.Format()) || !out.CopyProps(src)) {
		Fail("nlmeans: AllocVideo failed");
		return;
	}

	const int bpc = Bpc(src);
	const int planes = src.PlaneCount();
	for (int i = 0; i < planes; ++i)
		FilterPlane(src, out, i, bpc,
			static_cast<int>(m_research), static_cast<int>(m_patch), m_h);

	Log(Level::LowLevel, std::format("nlmeans {}x{} r={} p={} h={:.3f} pts={}",
		src.Width(), src.Height(), m_research, m_patch, m_h, src.Pts()));
	Save(std::move(out));
}
