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

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <utility>

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
}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::String logo, Anchor anchor, unsigned opacity, int margin) noexcept
: Watermark(std::move(log), std::move(logo), StormByte::Safe::Binary{},
	StormByte::Safe::Optional<Anchor>{anchor}, {}, opacity, margin, false) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	const StormByte::Safe::Binary& logo, Anchor anchor, unsigned opacity, int margin) noexcept
: Watermark(std::move(log), StormByte::Safe::String{}, logo,
	StormByte::Safe::Optional<Anchor>{anchor}, {}, opacity, margin, true) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::String logo, StormByte::Multimedia::Property::Point position,
	unsigned opacity) noexcept
: Watermark(std::move(log), std::move(logo), StormByte::Safe::Binary{}, {},
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Point>{position}, opacity, 0, false) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	const StormByte::Safe::Binary& logo, StormByte::Multimedia::Property::Point position,
	unsigned opacity) noexcept
: Watermark(std::move(log), StormByte::Safe::String{}, logo, {},
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Point>{position}, opacity, 0, true) {}

Watermark::Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::String path, const StormByte::Safe::Binary& logo,
	StormByte::Safe::Optional<Anchor> anchor,
	StormByte::Safe::Optional<StormByte::Multimedia::Property::Point> position,
	unsigned opacity, int margin, bool binaryInput) noexcept
: Filter::Process(std::move(log), "watermark"),
	m_logo(logo),
	m_anchor(std::move(anchor)), m_point(std::move(position)),
	m_opacity(std::min(opacity, 100u)), m_margin(margin),
	m_logoWidth(0), m_logoHeight(0), m_decoded(false),
	m_released(false), m_barTop(0), m_barBottom(0), m_barLeft(0), m_barRight(0),
	m_stable(0), m_lumaW(0), m_lumaH(0), m_lumaFmt(FFrame::FormatNone()) {
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
	m_logoWidth = 0;
	m_logoHeight = 0;
	m_decoded = false;
	m_released = false;
	m_barTop = 0;
	m_barBottom = 0;
	m_barLeft = 0;
	m_barRight = 0;
	m_stable = 0;
	DropScale();
}

void Watermark::Setup() noexcept {
	Log(Level::Debug, std::format("opacity {}%, margin {}", m_opacity, m_margin));
}

void Watermark::DisableLogo(std::string_view why) noexcept {
	Log(Level::Warning, std::format("disabled: {}", why));
	m_opacity = 0;
	m_rgba.clear();
	m_logoWidth = 0;
	m_logoHeight = 0;
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
	if (!decoded.ScaleTo(rgba, decoded.Width(), decoded.Height(),
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
		if (!src.ScaleTo(*luma, src.Width(), src.Height(),
				FFrame::Resample::Default, FFrame::Scaler::Sws)) {
			Fail("failed to allocate luma probe");
			return nullptr;
		}

		m_luma = std::move(luma);
		m_lumaW = src.Width();
		m_lumaH = src.Height();
		m_lumaFmt = src.Format();
	}
	else if (!src.ScaleTo(*m_luma, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws)) {
		Fail("failed to convert frame to luma");
		return nullptr;
	}

	return m_luma.get();
}

bool Watermark::ProbeBars(const FFrame& src) noexcept {
	if (!Luma(src) || m_luma->Width() < 16 || m_luma->Height() < 16) {
		Log(Level::Debug, std::format("probe skip luma={}x{}",
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
			Log(Level::Debug, std::format(
				"probe stretch edge={} core={} found={},{};{},{}",
				edge, core, found.top, found.bottom, found.left, found.right));
		}
	}

	if (!Boxed(found)) {
		ApplyLut(gray, GammaLut(0.45));
		found = Measure(gray);
		Log(Level::Debug, std::format("probe gamma found={},{};{},{}",
			found.top, found.bottom, found.left, found.right));
	}

	if (!Boxed(found)) {
		Log(Level::Debug, std::format(
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

	Log(Level::Debug, std::format(
		"probe same={} stable={} found={},{};{},{} bars={},{};{},{}",
		static_cast<int>(same), m_stable,
		found.top, found.bottom, found.left, found.right,
		m_barTop, m_barBottom, m_barLeft, m_barRight));
	return true;
}

void Watermark::Process(const Pipeline::Frame&) noexcept {
	if (m_opacity == 0)
		return;

	const FFrame& src = AVFrame();

	if (m_anchor && !m_point && !m_released) {
		if (!Held())
			Hold(ProbeMax);
		const bool usable = ProbeBars(src);
		const bool boxed = (m_barTop > 4 && m_barBottom > 4)
			|| (m_barLeft > 4 && m_barRight > 4);
		Log(Level::Debug, std::format(
			"hold usable={} boxed={} stable={} held={} bars={},{};{},{}",
			static_cast<int>(usable), static_cast<int>(boxed), m_stable,
			static_cast<unsigned>(HeldFor()),
			m_barTop, m_barBottom, m_barLeft, m_barRight));
		if (usable && boxed && m_stable >= 8) {
			Log(Level::Debug, "stable release");
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

void Watermark::LastChance(const Pipeline::Frame&) noexcept {
	Log(Level::Debug, std::format(
		"last-chance stable={} bars={},{};{},{}",
		m_stable, m_barTop, m_barBottom, m_barLeft, m_barRight));
	m_released = true;
	Release();
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

	const auto [x, y] = Place(m_logoWidth, m_logoHeight, m_anchor, m_point,
		m_margin, x0, y0, aw, ah);
	if (x < x0 || y < y0 || x + m_logoWidth > x0 + aw || y + m_logoHeight > y0 + ah) {
		DisableLogo("logo does not fit in the active picture");
		return;
	}

	FFrame out;
	out.Format(FFrame::FormatRgba());
	if (!src.ScaleTo(out, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws)) {
		Fail("failed to convert frame to RGBA");
		return;
	}

	if (!out.Data(0)) {
		Fail("failed to allocate destination");
		return;
	}

	Blend(out.Data(0), out.Linesize(0), out.Width(), out.Height(),
		m_rgba.data(),
		m_logoWidth, m_logoHeight, x, y, m_opacity);

	if (src.Format() != FFrame::FormatRgba()) {
		FFrame restored;
		restored.Format(src.Format());
		if (!out.ScaleTo(restored, src.Width(), src.Height(),
				FFrame::Resample::Default, FFrame::Scaler::Sws)) {
			Fail("failed to convert frame back");
			return;
		}
		Save(std::move(restored));
		return;
	}

	Save(std::move(out));
}
