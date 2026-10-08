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

#include <StormByte/multimedia/pipeline/filters/video/crop.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Crop;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

namespace {
	constexpr int BarSlack = 16;

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
}

Crop::Crop(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("crop")),
	m_auto(true), m_released(false), m_skip(false),
	m_x(0), m_y(0), m_w(0), m_h(0),
	m_left(0), m_right(0), m_top(0), m_bottom(0), m_stable(0),
	m_lumaW(0), m_lumaH(0), m_lumaFmt(FFrame::FormatNone()) {}

Crop::Crop(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	int x, int y, int width, int height) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("crop")),
	m_auto(false), m_released(true), m_skip(false),
	m_x(x), m_y(y), m_w(width), m_h(height),
	m_left(0), m_right(0), m_top(0), m_bottom(0), m_stable(0),
	m_lumaW(0), m_lumaH(0), m_lumaFmt(FFrame::FormatNone()) {}

Crop::~Crop() noexcept {
	DropLuma();
}

enum Type Crop::Media() const noexcept {
	return Type::Video;
}

void Crop::DropLuma() noexcept {
	m_luma.reset();
	m_lumaW = 0;
	m_lumaH = 0;
	m_lumaFmt = FFrame::FormatNone();
}

void Crop::Clean() noexcept {
	m_released = !m_auto;
	m_skip = false;
	m_left = 0;
	m_right = 0;
	m_top = 0;
	m_bottom = 0;
	m_stable = 0;
	DropLuma();
}

void Crop::Setup() noexcept {
	Clean();
}

bool Crop::Sure() const noexcept {
	const bool boxed = (m_top > 4 && m_bottom > 4) || (m_left > 4 && m_right > 4);
	return boxed && m_stable >= 8;
}

const FFrame* Crop::Luma(const FFrame& src) noexcept {
	if (!src || src.Width() <= 0 || src.Height() <= 0)
		return nullptr;
	if (src.Hardware()) {
		Fail("crop needs a software frame");
		return nullptr;
	}
	if (m_luma && (m_lumaW != src.Width() || m_lumaH != src.Height() || m_lumaFmt != src.Format()))
		DropLuma();
	if (!m_luma) {
		auto luma = StormByte::Safe::MakeUnique<FFrame>();
		luma->Format(FFrame::FormatGray8());
		if (!src.ScaleTo(*luma, src.Width(), src.Height(),
				FFrame::Resample::Default, FFrame::Scaler::Sws)) {
			Fail("crop: failed to allocate luma probe");
			return nullptr;
		}
		m_luma = std::move(luma);
		m_lumaW = src.Width();
		m_lumaH = src.Height();
		m_lumaFmt = src.Format();
	} else if (!src.ScaleTo(*m_luma, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws)) {
		Fail("crop: failed to convert frame to luma");
		return nullptr;
	}
	return m_luma.get();
}

bool Crop::ProbeBars(const FFrame& src) noexcept {
	if (!Luma(src) || m_luma->Width() < 16 || m_luma->Height() < 16)
		return false;
	FFrame& gray = *m_luma;

	Probe found = Measure(gray);
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
		}
	}
	if (!Boxed(found)) {
		ApplyLut(gray, GammaLut(0.45));
		found = Measure(gray);
	}
	if (!Boxed(found))
		return false;

	const bool seeded = m_top != 0 || m_bottom != 0 || m_left != 0 || m_right != 0;
	const bool same = seeded
		&& NearBar(found.top, m_top)
		&& NearBar(found.bottom, m_bottom)
		&& NearBar(found.left, m_left)
		&& NearBar(found.right, m_right);

	if (found.top > 0 && found.bottom > 0) {
		if (m_top == 0) {
			m_top = found.top;
			m_bottom = found.bottom;
		} else {
			m_top = std::min(m_top, found.top);
			m_bottom = std::min(m_bottom, found.bottom);
		}
	}
	if (found.left > 0 && found.right > 0) {
		if (m_left == 0) {
			m_left = found.left;
			m_right = found.right;
		} else {
			m_left = std::min(m_left, found.left);
			m_right = std::min(m_right, found.right);
		}
	}
	if (same)
		++m_stable;
	else
		m_stable = 0;
	return true;
}

void Crop::Apply() noexcept {
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Fail("crop: missing video buffer");
		return;
	}
	if (src.Hardware()) {
		Fail("crop needs a software frame");
		return;
	}

	int left = m_left;
	int top = m_top;
	int right = m_right;
	int bottom = m_bottom;
	if (!m_auto) {
		left = m_x;
		top = m_y;
		right = src.Width() - m_x - m_w;
		bottom = src.Height() - m_y - m_h;
		if (m_w <= 0 || m_h <= 0 || left < 0 || top < 0 || right < 0 || bottom < 0) {
			Fail("crop: window is outside the frame");
			return;
		}
	}

	if (left == 0 && right == 0 && top == 0 && bottom == 0) {
		Log(Level::LowLevel, "crop no-op identity");
		return;
	}
	if (src.Width() - left - right <= 0 || src.Height() - top - bottom <= 0) {
		Fail("crop: empty picture");
		return;
	}

	FFrame out = src.Clone();
	if (!out) {
		Fail("crop: clone failed");
		return;
	}
	out.Crop(left, right, top, bottom);
	if (!out.ApplyCropping()) {
		Fail("crop: ApplyCropping failed");
		return;
	}
	Log(Level::LowLevel, std::format(
		"crop {}x{} -> {}x{} bars LRTB={},{};{},{}",
		src.Width(), src.Height(), out.Width(), out.Height(),
		left, right, top, bottom));
	Save(std::move(out));
}

void Crop::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	if (m_skip)
		return;

	if (m_auto && !m_released) {
		if (!Held())
			Hold(ProbeMax);
		const bool usable = ProbeBars(AVFrame());
		if (usable && Sure()) {
			Log(Level::Debug, std::format(
				"crop stable release bars LRTB={},{};{},{}",
				m_left, m_right, m_top, m_bottom));
			m_released = true;
			Release();
			return;
		}
		if (Held())
			return;
		m_released = true;
		m_skip = !Sure();
		if (m_skip) {
			Log(Level::Warning,
				"crop: bars not stable, leaving the frame uncropped");
			return;
		}
	}

	Apply();
}

void Crop::LastChance(const Pipeline::Frame&) noexcept {
	m_released = true;
	if (!Sure()) {
		m_skip = true;
		Log(Level::Warning,
			"crop: could not detect bars, leaving the frame uncropped");
	}
	Release();
}
