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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/bwdif.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cstdint>
#include <format>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Bwdif;
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

	int Cubic(int a, int b, int c, int d) noexcept {
		return std::clamp((-a + 9 * b + 9 * c - d + 8) / 16, 0, 1 << 30);
	}

	void FilterPlane(const FFrame& prev, const FFrame& cur, const FFrame& next,
		FFrame& out, int plane, int bpc, bool tff) noexcept {
		const int w = cur.PlaneWidth(plane);
		const int h = cur.PlaneHeight(plane);
		uint8_t* dst = out.Data(plane);
		const uint8_t* c = cur.Data(plane);
		const uint8_t* p = prev ? prev.Data(plane) : nullptr;
		const uint8_t* n = next ? next.Data(plane) : nullptr;
		const int ds = out.Linesize(plane);
		const int cs = cur.Linesize(plane);
		const int ps = prev ? prev.Linesize(plane) : 0;
		const int ns = next ? next.Linesize(plane) : 0;
		if (!dst || !c || w <= 0 || h <= 0)
			return;

		for (int y = 0; y < h; ++y) {
			uint8_t* drow = dst + y * ds;
			const uint8_t* crow = c + y * cs;
			const bool keep = tff ? ((y & 1) == 0) : ((y & 1) == 1);
			if (keep) {
				const int bytes = (bpc <= 8) ? w : w * 2;
				std::copy(crow, crow + bytes, drow);
				continue;
			}
			const auto line = [&](int yy) {
				return c + std::clamp(yy, 0, h - 1) * cs;
			};
			const uint8_t* l0 = line(y - 3);
			const uint8_t* l1 = line(y - 1);
			const uint8_t* l2 = line(y + 1);
			const uint8_t* l3 = line(y + 3);
			const uint8_t* prow = p ? p + y * ps : nullptr;
			const uint8_t* nrow = n ? n + y * ns : nullptr;
			for (int x = 0; x < w; ++x) {
				int spatial = Cubic(Pel(l0, x, bpc), Pel(l1, x, bpc),
					Pel(l2, x, bpc), Pel(l3, x, bpc));
				spatial = std::clamp(spatial, 0, (1 << bpc) - 1);
				if (!prow || !nrow) {
					Put(drow, x, bpc, spatial);
					continue;
				}
				const int temporal = (Pel(prow, x, bpc) + Pel(nrow, x, bpc) + 1) / 2;
				const int dspt = std::abs(spatial - Pel(crow, x, bpc));
				const int dtm = std::abs(temporal - Pel(crow, x, bpc));
				Put(drow, x, bpc, dspt <= dtm ? spatial : temporal);
			}
		}
	}
}

Bwdif::Bwdif(StormByte::Safe::Shared<StormByte::Logger::Log> log, bool onlyInterlaced) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("bwdif")), m_onlyInterlaced(onlyInterlaced) {}

Bwdif::~Bwdif() noexcept {
	Clean();
}

enum Type Bwdif::Media() const noexcept {
	return Type::Video;
}

void Bwdif::Clean() noexcept {
	m_prev.Unref();
	m_cur.Unref();
}

void Bwdif::Setup() noexcept {
	Clean();
}

void Bwdif::Weave(const FFrame& prev, const FFrame& cur, const FFrame& next) noexcept {
	if (!cur) {
		Log(Level::Warning, "bwdif: empty current look");
		return;
	}
	if (m_onlyInterlaced && !cur.Interlaced())
		return;

	FFrame out;
	if (!out.AllocVideo(cur.Width(), cur.Height(), cur.Format()) || !out.CopyProps(cur)) {
		Fail("bwdif: AllocVideo failed");
		return;
	}
	out.Interlaced(false, false);

	const int bpc = Bpc(cur);
	const bool tff = cur.TopFieldFirst();
	const int planes = cur.PlaneCount();
	for (int i = 0; i < planes; ++i)
		FilterPlane(prev, cur, next, out, i, bpc, tff);

	Log(Level::LowLevel, std::format("bwdif weave {}x{} tff={} pts={}",
		cur.Width(), cur.Height(), tff, cur.Pts()));
	Save(std::move(out));
}

void Bwdif::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& raw = AVFrame();
	if (!raw) {
		Log(Level::Warning, "bwdif: frame has no backend");
		return;
	}

	FFrame incoming = raw.Clone();
	if (!incoming) {
		Log(Level::Warning, "bwdif: Clone failed");
		return;
	}

	if (!m_cur) {
		m_cur = std::move(incoming);
		return;
	}

	Weave(m_prev, m_cur, incoming);
	m_prev = std::move(m_cur);
	m_cur = std::move(incoming);
}

void Bwdif::Eof() noexcept {
	if (m_cur)
		Weave(m_prev, m_cur, FFrame{});
	Clean();
}

void Bwdif::LastChance(const Pipeline::Frame&) noexcept {}
