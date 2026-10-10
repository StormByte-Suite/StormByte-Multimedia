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
#include <StormByte/multimedia/pipeline/filters/video/hqdn3d.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Hqdn3d;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

namespace {
	int Bpc(const FFrame& raw) noexcept {
		if (!raw)
			return 8;
		const int depth = raw.BitsPerComponent();
		return depth <= 8 ? 8 : (depth <= 10 ? 10 : 12);
	}

	int Pel(const uint8_t* row, int x, int bpc) noexcept {
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

	double Mix(double strength, int delta, int peak) noexcept {
		if (strength <= 0.0 || peak <= 0)
			return 0.0;
		const double n = static_cast<double>(std::abs(delta)) / static_cast<double>(peak);
		const double w = std::exp(-n * n * 16.0 / std::max(0.25, strength));
		return std::clamp(w, 0.0, 1.0);
	}

	void FilterPlane(const FFrame& src, const FFrame& prev, FFrame& dst,
		int plane, int bpc, double spatial, double temporal) noexcept {
		const int w = src.PlaneWidth(plane);
		const int h = src.PlaneHeight(plane);
		const uint8_t* s = src.Data(plane);
		uint8_t* d = dst.Data(plane);
		const uint8_t* p = prev ? prev.Data(plane) : nullptr;
		const int ss = src.Linesize(plane);
		const int ds = dst.Linesize(plane);
		const int ps = prev ? prev.Linesize(plane) : 0;
		if (!s || !d || w <= 0 || h <= 0)
			return;

		const int peak = (1 << bpc) - 1;
		for (int y = 0; y < h; ++y) {
			const uint8_t* srow = s + y * ss;
			uint8_t* drow = d + y * ds;
			const uint8_t* prow = p ? p + y * ps : nullptr;
			const uint8_t* above = s + std::max(0, y - 1) * ss;
			const uint8_t* below = s + std::min(h - 1, y + 1) * ss;
			for (int x = 0; x < w; ++x) {
				const int c = Pel(srow, x, bpc);
				const int l = Pel(srow, std::max(0, x - 1), bpc);
				const int r = Pel(srow, std::min(w - 1, x + 1), bpc);
				const int u = Pel(above, x, bpc);
				const int dn = Pel(below, x, bpc);
				double acc = static_cast<double>(c);
				double wgt = 1.0;
				const auto add = [&](int v) {
					const double m = Mix(spatial, v - c, peak);
					acc += static_cast<double>(v) * m;
					wgt += m;
				};
				add(l);
				add(r);
				add(u);
				add(dn);
				double out = acc / wgt;
				if (prow) {
					const int pv = Pel(prow, x, bpc);
					const double tm = Mix(temporal, pv - c, peak);
					out = out * (1.0 - tm) + static_cast<double>(pv) * tm;
				}
				Put(drow, x, bpc, static_cast<int>(out + 0.5));
			}
		}
	}
}

Hqdn3d::Hqdn3d(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	double lumaSpatial, double chromaSpatial,
	double lumaTemporal, double chromaTemporal) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("hqdn3d")),
	m_ls(lumaSpatial > 0.0 ? lumaSpatial : 4.0),
	m_cs(chromaSpatial > 0.0 ? chromaSpatial : 3.0),
	m_lt(lumaTemporal > 0.0 ? lumaTemporal : 6.0),
	m_ct(chromaTemporal > 0.0 ? chromaTemporal : 4.5) {}

Hqdn3d::~Hqdn3d() noexcept {
	Clean();
}

enum Type Hqdn3d::Media() const noexcept {
	return Type::Video;
}

void Hqdn3d::Clean() noexcept {
	m_prev.Unref();
}

void Hqdn3d::Setup() noexcept {
	Clean();
}

void Hqdn3d::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "hqdn3d: frame has no picture");
		return;
	}

	FFrame out;
	if (!out.AllocVideo(src.Width(), src.Height(), src.Format()) || !out.CopyProps(src)) {
		Fail("hqdn3d: AllocVideo failed");
		return;
	}

	const int bpc = Bpc(src);
	const int planes = src.PlaneCount();
	for (int i = 0; i < planes; ++i) {
		const bool luma = (i == 0);
		FilterPlane(src, m_prev, out, i, bpc,
			luma ? m_ls : m_cs, luma ? m_lt : m_ct);
	}

	Log(Level::LowLevel, std::format("hqdn3d {}x{} pts={}",
		src.Width(), src.Height(), src.Pts()));
	m_prev = out.Clone();
	Save(std::move(out));
}
