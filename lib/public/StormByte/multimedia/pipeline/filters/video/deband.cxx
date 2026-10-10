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
#include <StormByte/multimedia/pipeline/filters/video/deband.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cstdint>
#include <format>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Deband;
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

	int HashGrain(int x, int y, int plane, std::int64_t pts, int amp) noexcept {
		if (amp <= 0)
			return 0;
		std::uint32_t h = 2166136261u;
		const auto mix = [&](std::uint32_t v) {
			h ^= v;
			h *= 16777619u;
		};
		mix(static_cast<std::uint32_t>(x));
		mix(static_cast<std::uint32_t>(y));
		mix(static_cast<std::uint32_t>(plane));
		mix(static_cast<std::uint32_t>(pts));
		mix(static_cast<std::uint32_t>(pts >> 32));
		const int span = amp * 2 + 1;
		return static_cast<int>(h % static_cast<std::uint32_t>(span)) - amp;
	}

	void DebandPlane(const FFrame& src, FFrame& dst, int plane, int bpc,
		int range, int threshold, int grain, std::int64_t pts) noexcept {
		const int w = src.PlaneWidth(plane);
		const int h = src.PlaneHeight(plane);
		const uint8_t* s = src.Data(plane);
		uint8_t* d = dst.Data(plane);
		const int ss = src.Linesize(plane);
		const int ds = dst.Linesize(plane);
		if (!s || !d || w <= 0 || h <= 0 || ss <= 0 || ds <= 0)
			return;

		const int chroma = (plane == 0) ? 1 : 2;
		const int r = std::max(1, range / chroma);
		const int peak = (1 << bpc) - 1;
		const int thr = std::max(1, threshold * peak / 255);
		const int amp = grain * peak / 255;

		for (int y = 0; y < h; ++y) {
			const uint8_t* srow = s + y * ss;
			uint8_t* drow = d + y * ds;
			for (int x = 0; x < w; ++x) {
				const int c = Pel(srow, x, bpc);
				const int xl = std::max(0, x - r);
				const int xr = std::min(w - 1, x + r);
				const int yu = std::max(0, y - r);
				const int yd = std::min(h - 1, y + r);
				const int l = Pel(srow, xl, bpc);
				const int ri = Pel(srow, xr, bpc);
				const int u = Pel(s + yu * ss, x, bpc);
				const int dn = Pel(s + yd * ss, x, bpc);
				int out = c;
				if (std::abs(c - l) < thr && std::abs(c - ri) < thr
					&& std::abs(c - u) < thr && std::abs(c - dn) < thr)
					out = (l + ri + u + dn + 2) / 4;
				out += HashGrain(x, y, plane, pts, amp);
				Put(drow, x, bpc, out);
			}
		}
	}
}

Deband::Deband(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	unsigned range, unsigned threshold, unsigned grain) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("deband")),
	m_range(range ? range : 16u),
	m_threshold(threshold ? threshold : 4u),
	m_grain(grain ? grain : 2u) {}

Deband::~Deband() noexcept = default;

enum Type Deband::Media() const noexcept {
	return Type::Video;
}

void Deband::Clean() noexcept {}

void Deband::Setup() noexcept {}

void Deband::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "deband: frame has no picture");
		return;
	}

	FFrame out;
	if (!out.AllocVideo(src.Width(), src.Height(), src.Format()) || !out.CopyProps(src)) {
		Fail("deband: AllocVideo failed");
		return;
	}

	const int bpc = Bpc(src);
	const int planes = src.PlaneCount();
	for (int i = 0; i < planes; ++i)
		DebandPlane(src, out, i, bpc,
			static_cast<int>(m_range), static_cast<int>(m_threshold),
			static_cast<int>(m_grain), src.Pts());

	Log(Level::LowLevel, std::format("deband {}x{} range={} thr={} grain={} pts={}",
		src.Width(), src.Height(), m_range, m_threshold, m_grain, src.Pts()));
	Save(std::move(out));
}
