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

#include <StormByte/multimedia/pipeline/filters/video/degrain.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <utility>

using namespace StormByte::Multimedia::Pipeline::Filter::Video;
using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

/*
 * Detection intent
 * ----------------
 * A single strength cannot clean a noisy wall and preserve a face in the same
 * picture. Measure a fixed 12x8 grid, keep one compact target map per original
 * timestamp, then interpolate strengths spatially during the second pass.
 * Zero means preserve the original, not run a denoiser with sigma zero. Darkness
 * is never evidence of grain by itself. Missing evidence must not create a
 * minimum strength: this is deliberately biased toward underfiltering.
 *
 * Measurement uses 8-bit YUV copies for comparable thresholds, not for output.
 * Regional means compensate gradual lighting drift (lamps, candles) before
 * checking whether the same coordinate still observes a stable surface. High
 * frequency temporal differences then estimate grain, while spatial texture,
 * unstable surfaces and skin-like colours veto or reduce the target. These are
 * heuristics, not optical flow, object recognition or a guarantee of clean-image
 * preservation. Equal-brightness cuts, moving flat surfaces, fine texture and
 * non-Gaussian/compressed grain need visual regression examples when tuning.
 *
 * The target map is measured per frame so a moving subject does not inherit a
 * wall's strength for an entire scene. Positive targets are smoothed only within
 * continuity groups; zero evidence stays zero. Bilinear interpolation avoids
 * hard tile seams, but spreads a neighbouring positive target near a tile edge.
 * Therefore a zero cell is not an exact per-pixel protection mask. All-zero
 * frames are exact passthrough; narrow details need finer analysis in a future
 * version rather than stronger global thresholds.
 *
 * Processing intent
 * -----------------
 * Generate three strengths bracketing each frame's maximum target and blend
 * adjacent outputs, including the original as level zero. Each window is fully
 * drained and its centre is selected by synthetic PTS before restoring source
 * properties. This avoids changing the filter base's current-item lineage or
 * silently discarding delayed outputs at strength changes. It intentionally
 * trades graph creation cost for one-input/one-output accounting and bounded
 * history. The future input is a duplicate current frame, not a real lookahead;
 * this asymmetric baseline must not be described as symmetric temporal filtering.
 *
 * Keep output at the source depth/layout and copy source properties and alpha.
 * Never feed filtered output back as history. Format changes, ambiguous PTS and
 * detected brightness boundaries invalidate history. When extending this code,
 * test clean/dark-clean passthrough, noisy flats beside detail, changing light,
 * cuts, first/tail frames, duplicate PTS, byte order, alpha and HDR side data.
 */
namespace {
	constexpr std::size_t RingSpan = 5;
	constexpr std::size_t RingMax = 2 * RingSpan + 1;
	constexpr int64_t MissingPts = std::numeric_limits<int64_t>::min();
	constexpr double DefaultCap = 4.0;
	constexpr double StableFraction = 0.85;

	float ClampedTarget(double target, double cap) noexcept {
		const float stored = static_cast<float>(std::clamp(target, 0.0, cap));
		return static_cast<double>(stored) > cap ? std::nextafter(stored, 0.0f) : stored;
	}

	double Median(std::vector<double>& values) noexcept {
		if (values.empty())
			return 0.0;
		auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
		std::nth_element(values.begin(), middle, values.end());
		const double upper = *middle;
		return values.size() % 2 ? upper : 0.5 * (upper + *std::max_element(values.begin(), middle));
	}

	double Box(const FFrame& frame, int horizontal, int vertical, int radius) noexcept {
		double sum = 0.0;
		int count = 0;
		for (int row = vertical - radius; row <= vertical + radius; ++row) {
			const auto* pixels = frame.Data(0) + static_cast<std::ptrdiff_t>(row) * frame.Linesize(0);
			for (int column = horizontal - radius; column <= horizontal + radius; ++column) {
				sum += pixels[column];
				++count;
			}
		}
		return sum / count;
	}

	bool SameLayout(const FFrame& first, const FFrame& second) noexcept {
		if (first.Width() != second.Width() || first.Height() != second.Height()
			|| first.Format() != second.Format() || first.PlaneCount() != second.PlaneCount())
			return false;
		for (int plane = 0; plane < first.PlaneCount(); ++plane)
			if (!first.Data(plane) || !second.Data(plane)
				|| first.PlaneWidth(plane) != second.PlaneWidth(plane)
				|| first.PlaneHeight(plane) != second.PlaneHeight(plane))
				return false;
		return true;
	}

	unsigned ReadSample(const uint8_t* sample, int bytes, bool bigEndian) noexcept {
		if (bytes == 1)
			return sample[0];
		return bigEndian ? (static_cast<unsigned>(sample[0]) << 8) | sample[1]
			: (static_cast<unsigned>(sample[1]) << 8) | sample[0];
	}

	void WriteSample(uint8_t* sample, unsigned value, int bytes, bool bigEndian) noexcept {
		if (bytes == 1) {
			sample[0] = static_cast<uint8_t>(value);
			return;
		}
		sample[bigEndian ? 0 : 1] = static_cast<uint8_t>(value >> 8);
		sample[bigEndian ? 1 : 0] = static_cast<uint8_t>(value & 255);
	}
}

Degrain::Degrain(std::shared_ptr<StormByte::Logger::Log> log,
	std::optional<double> sigmaCap) noexcept
	: Filter::ProcessTwoPasses(std::move(log), "degrain"), m_capIn(sigmaCap) {}

Degrain::~Degrain() noexcept = default;

enum Type Degrain::Media() const noexcept {
	return Type::Video;
}

void Degrain::Clean() noexcept {
	m_ring.clear();
	m_row.clear();
	m_lookup.clear();
	m_processed.clear();
	m_previous.reset();
	m_previousRow.reset();
	m_lastPts.reset();
	m_group = 0;
	m_frames = 0;
	m_voted = false;
	m_sigmaMin = 0.0;
	m_sigmaP50 = 0.0;
	m_sigmaMax = 0.0;
	m_ran = 0;
	m_skipped = 0;
	m_applied = 0;
	m_unsupported = 0;
	m_rejected = 0;
}

void Degrain::Setup() noexcept {
	// Setup occurs between passes: retain measured targets, never encode history.
	m_processed.clear();
	m_previous.reset();
	m_previousRow.reset();
	m_applied = 0;
	m_unsupported = 0;
	if (m_capIn && !std::isfinite(*m_capIn))
		Fail("degrain: sigma cap must be finite");
}

void Degrain::PushFrame(const FFrame& source) noexcept {
	++m_frames;
	const int64_t pts = source.Pts();
	const auto found = m_lookup.find(pts);
	if (pts == MissingPts || found != m_lookup.end()) {
		// All measured occurrences of a duplicate are ambiguous, including the first.
		// Keeping the lookup entry prevents a third occurrence from becoming valid.
		if (found != m_lookup.end()) {
			m_row[found->second].valid = false;
			m_row[found->second].regions.fill(0.0f);
		}
		++m_rejected;
		FlushRing();
		++m_group;
		m_lastPts.reset();
		return;
	}
	if (m_ring.size() == RingMax)
		m_ring.pop_front();
	Slot slot;
	slot.pic = std::make_unique<FFrame>();
	slot.pic->Format(FFrame::FormatYUV420P());
	if (source.Hardware() || source.Width() < 16 || source.Height() < 16
		|| !source.ScaleTo(*slot.pic, source.Width(), source.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Sws) || !slot.pic->Data(0)) {
		++m_rejected;
		FlushRing();
		++m_group;
		m_lastPts.reset();
		return;
	}

	// A regional mean sees a local flash that a global mean can hide. Sampling
	// each cell evenly makes the comparison independent of row padding. This is
	// a brightness detector, not a semantic scene detector: equal-luma cuts can
	// escape it and must still pass the conservative surface-stability tests.
	for (int regionY = 0; regionY < GridHeight; ++regionY) {
		for (int regionX = 0; regionX < GridWidth; ++regionX) {
			double sum = 0.0;
			for (int sampleY = 0; sampleY < 8; ++sampleY) {
				const int vertical = (regionY * 8 + sampleY) * source.Height() / (GridHeight * 8);
				const auto* pixels = slot.pic->Data(0) + static_cast<std::ptrdiff_t>(vertical) * slot.pic->Linesize(0);
				for (int sampleX = 0; sampleX < 8; ++sampleX) {
					const int horizontal = (regionX * 8 + sampleX) * source.Width() / (GridWidth * 8);
					sum += pixels[horizontal];
				}
			}
			slot.means[regionY * GridWidth + regionX] = static_cast<float>(sum / 64.0);
		}
	}
	bool boundary = m_lastPts && pts <= *m_lastPts;
	if (!m_ring.empty()) {
		const auto& previous = m_ring.back();
		const auto& row = m_row[previous.row];
		boundary = boundary || row.width != source.Width() || row.height != source.Height()
			|| row.format != source.Format();
		double signedDifference = 0.0;
		double absoluteDifference = 0.0;
		for (std::size_t region = 0; region < slot.means.size(); ++region) {
			const double difference = slot.means[region] - previous.means[region];
			signedDifference += difference;
			absoluteDifference += std::fabs(difference);
		}
		boundary = boundary || std::fabs(signedDifference / slot.means.size()) >= 12.0
			|| absoluteDifference / slot.means.size() >= 14.0;
	}
	if (boundary) {
		FlushRing();
		++m_group;
	}
	m_lastPts = pts;
	slot.row = m_row.size();
	Row row;
	row.pts = pts;
	row.group = m_group;
	row.width = source.Width();
	row.height = source.Height();
	row.format = source.Format();
	m_lookup.emplace(pts, slot.row);
	m_row.push_back(row);
	m_ring.push_back(std::move(slot));
	if (m_ring.size() > RingSpan)
		ScoreCenter(m_ring.size() - 1 - RingSpan);
}

void Degrain::ScoreCenter(std::size_t index) noexcept {
	if (index >= m_ring.size() || m_ring[index].scored)
		return;
	auto& slot = m_ring[index];
	slot.scored = true;
	auto& row = m_row[slot.row];
	if (!row.valid)
		return;
	const auto& current = *slot.pic;
	const double cap = std::clamp(m_capIn.value_or(DefaultCap), 0.0, 100.0);
	for (int regionY = 0; regionY < GridHeight; ++regionY) {
		for (int regionX = 0; regionX < GridWidth; ++regionX) {
			const int region = regionY * GridWidth + regionX;
			const int left = regionX * current.Width() / GridWidth;
			const int right = (regionX + 1) * current.Width() / GridWidth;
			const int top = regionY * current.Height() / GridHeight;
			const int bottom = (regionY + 1) * current.Height() / GridHeight;
			if (right - left < 8 || bottom - top < 8)
				continue;
			std::vector<double> residuals;
			residuals.reserve(64 * 2 * RingSpan);
			int seen = 0;
			int stable = 0;
			int detail = 0;
			int skin = 0;
			for (int sampleY = 0; sampleY < 8; ++sampleY) {
				const int vertical = top + (2 * sampleY + 1) * (bottom - top) / 16;
				if (vertical < 4 || vertical >= current.Height() - 4)
					continue;
				for (int sampleX = 0; sampleX < 8; ++sampleX) {
					const int horizontal = left + (2 * sampleX + 1) * (right - left) / 16;
					if (horizontal < 4 || horizontal >= current.Width() - 4)
						continue;
					++seen;
					const double luminance = current.Data(0)[static_cast<std::ptrdiff_t>(vertical) * current.Linesize(0) + horizontal];
					const double low = Box(current, horizontal, vertical, 1);
					const double high = luminance - low;

					// Difference of 3x3 and 7x7 averages detects mid-scale texture;
					// differences of displaced 3x3 averages detect edges. Grain is
					// attenuated by these averages, unlike hair, fabric and contours.
					// Fine texture and correlated/compressed noise can still alias:
					// this is a veto/hint, never a claim to identify real objects.
					const bool textured = std::fabs(low - Box(current, horizontal, vertical, 3)) > 2.5
						|| std::fabs(Box(current, horizontal - 2, vertical, 1)
							- Box(current, horizontal + 2, vertical, 1)) > 6.0
						|| std::fabs(Box(current, horizontal, vertical - 2, 1)
							- Box(current, horizontal, vertical + 2, 1)) > 6.0;
					if (textured)
						++detail;
					const int chromaX = horizontal * current.PlaneWidth(1) / current.Width();
					const int chromaY = vertical * current.PlaneHeight(1) / current.Height();
					const int chromaU = current.Data(1)[static_cast<std::ptrdiff_t>(chromaY) * current.Linesize(1) + chromaX];
					const int chromaV = current.Data(2)[static_cast<std::ptrdiff_t>(chromaY) * current.Linesize(2) + chromaX];
					// Broad YUV skin-like box, deliberately not face recognition.
					// Warm walls can match it; conservative underfiltering is preferable.
					if (luminance >= 50.0 && luminance <= 180.0 && chromaU <= 128
						&& chromaV >= 133 && chromaV - chromaU >= 12)
						++skin;

					std::array<double, 2 * RingSpan> differences{};
					std::size_t accepted = 0;
					std::size_t candidates = 0;
					const std::size_t first = index > RingSpan ? index - RingSpan : 0;
					const std::size_t last = std::min(m_ring.size() - 1, index + RingSpan);
					for (std::size_t neighbour = first; neighbour <= last; ++neighbour) {
						if (neighbour == index || !m_row[m_ring[neighbour].row].valid)
							continue;
						++candidates;
						const auto& other = *m_ring[neighbour].pic;
						const double otherLow = Box(other, horizontal, vertical, 1);
						const double brightness = m_ring[neighbour].means[region] - slot.means[region];
						// Subtract each cell's brightness drift before testing whether
						// this coordinate still observes the same low-frequency surface.
						// No search, warping or motion compensation is performed. Even
						// noisy motion loses confidence here and normally selects zero.
						if (std::fabs(otherLow - low - brightness) > 3.0)
							continue;
						const double otherHigh = other.Data(0)[static_cast<std::ptrdiff_t>(vertical) * other.Linesize(0) + horizontal] - otherLow;
						differences[accepted++] = high - otherHigh;
					}
					if (accepted < 3 || candidates == 0 || static_cast<double>(accepted) / candidates < 0.8
						|| luminance < 4.0 || luminance > 245.0)
						continue;
					++stable;
					if (!textured)
						residuals.insert(residuals.end(), differences.begin(), differences.begin() + static_cast<std::ptrdiff_t>(accepted));
				}
			}
			if (seen < 24 || stable < 24 || static_cast<double>(stable) / seen < StableFraction
				|| static_cast<double>(detail) / seen > 0.45 || residuals.size() < 72)
				continue;

			// Use every accepted temporal difference, never the minimum neighbour.
			// MAD rejects outliers and centring rejects a residual offset. For
			// H = Y - box3(Y), independent pixel noise has variance 8/9*sigma^2;
			// H(now)-H(other) has variance 16/9*sigma^2. The Gaussian MAD factor
			// 0.67448975 therefore converts these differences to 8-bit sigma.
			// Shared centre noise correlates differences: more neighbours increase
			// robustness, not the independent sample count. Film grain is not
			// necessarily white/Gaussian; these are conservative heuristic targets.
			const double centre = Median(residuals);
			for (auto& residual : residuals)
				residual = std::fabs(residual - centre);
			const double mad = Median(residuals);
			auto upper = residuals.begin() + static_cast<std::ptrdiff_t>(residuals.size() * 9 / 10);
			std::nth_element(residuals.begin(), upper, residuals.end());
			const double noise = mad / (0.67448975 * std::sqrt(16.0 / 9.0));
			if (noise < 1.25 || *upper > 4.0 * mad)
				continue;
			const double detailProtection = 1.0 - std::min(0.9, 2.0 * detail / seen);
			const double skinProtection = 1.0 - 0.75 * std::min(1.0, (static_cast<double>(skin) / seen) / 0.15);
			row.regions[region] = ClampedTarget(
				0.8 * (noise - 0.75) * detailProtection * skinProtection, cap);
		}
	}
}

void Degrain::FlushRing() noexcept {
	// Keep all remaining neighbours until every tail picture has been scored.
	for (std::size_t index = 0; index < m_ring.size(); ++index)
		ScoreCenter(index);
	m_ring.clear();
}

void Degrain::Decide() noexcept {
	if (m_voted)
		return;
	if (m_capIn && !std::isfinite(*m_capIn)) {
		Fail("degrain: sigma cap must be finite");
		m_ring.clear();
		m_voted = true;
		return;
	}
	FlushRing();
	m_voted = true;
	RegionMap previousRaw{};
	std::vector<double> strengths;
	for (std::size_t index = 0; index < m_row.size(); ++index) {
		auto& row = m_row[index];
		const RegionMap raw = row.regions;
		if (!row.valid)
			row.regions.fill(0.0f);
		else {
			for (std::size_t region = 0; region < raw.size(); ++region) {
				if (raw[region] == 0.0f)
					continue;
				// A modest 70/15/15 smoother uses unsmoothed neighbours, never
				// feeds its own result back, never raises an evidence-free zero,
				// and cannot cross a brightness cut, flash, format or PTS gap.
				double target = 0.7 * raw[region];
				double weight = 0.7;
				if (index > 0 && m_row[index - 1].valid && m_row[index - 1].group == row.group
					&& previousRaw[region] > 0.0f) {
					target += 0.15 * previousRaw[region];
					weight += 0.15;
				}
				if (index + 1 < m_row.size() && m_row[index + 1].valid && m_row[index + 1].group == row.group
					&& m_row[index + 1].regions[region] > 0.0f) {
					target += 0.15 * m_row[index + 1].regions[region];
					weight += 0.15;
				}
				row.regions[region] = ClampedTarget(target / weight,
					std::clamp(m_capIn.value_or(DefaultCap), 0.0, 100.0));
			}
		}
		previousRaw = raw;
		bool active = false;
		for (float target : row.regions) {
			if (target > 0.0f) {
				active = true;
				strengths.push_back(target);
			}
		}
		if (active)
			++m_ran;
		else
			++m_skipped;
	}
	if (strengths.empty())
		Log(Level::Warning, "degrain: no confident regional grain; leaving video untouched");
	else {
		m_sigmaP50 = Median(strengths);
		m_sigmaMin = *std::min_element(strengths.begin(), strengths.end());
		m_sigmaMax = *std::max_element(strengths.begin(), strengths.end());
		Log(Level::Notice, std::format("regional targets frames={} active={} sigma min/p50/max={:.3f}/{:.3f}/{:.3f}",
			m_frames, m_ran, m_sigmaMin, m_sigmaP50, m_sigmaMax));
	}
}

double Degrain::RegionTarget(const RegionMap& regions, double horizontal, double vertical) noexcept {
	const double positionX = std::clamp(horizontal * GridWidth - 0.5, 0.0, static_cast<double>(GridWidth - 1));
	const double positionY = std::clamp(vertical * GridHeight - 0.5, 0.0, static_cast<double>(GridHeight - 1));
	const int left = static_cast<int>(positionX);
	const int top = static_cast<int>(positionY);
	const int right = std::min(left + 1, GridWidth - 1);
	const int bottom = std::min(top + 1, GridHeight - 1);
	const double fractionX = positionX - left;
	const double fractionY = positionY - top;
	const double upper = (1.0 - fractionX) * regions[top * GridWidth + left] + fractionX * regions[top * GridWidth + right];
	const double lower = (1.0 - fractionX) * regions[bottom * GridWidth + left] + fractionX * regions[bottom * GridWidth + right];
	return (1.0 - fractionY) * upper + fractionY * lower;
}

bool Degrain::FilterWindow(const FFrame& previous, const FFrame& current,
	double sigma, FFrame& output) noexcept {
	if (!SameLayout(previous, current) || !std::isfinite(sigma) || sigma <= 0.0)
		return false;
	FFrame input;
	if (!input.Ref(previous))
		return false;
	input.Pts(0);
	const std::string chain = std::format(
		"fftdnoiz=sigma={:.9f}:prev=1:next=1:block=32:overlap=0.5:planes={},format=pix_fmts={}",
		sigma, current.PlaneCount() == 1 ? 1 : 7, current.FormatName());
	FGraph graph = FGraph::Open(input, chain);
	if (!graph)
		return false;

	// Save attaches CURRENT item lineage; a persistent temporal graph would
	// return older inputs and lose delayed frames on resets. Instead each sigma
	// gets its own complete [previous real, current, duplicate current] window.
	// Synthetic PTS 0/1/2 identify its centre unambiguously. This is asymmetric
	// temporal denoising, NOT a future real frame, and NOT motion compensation.
	// At discontinuities the previous input is also current (spatial baseline).
	// Cost: up to three graph creations and nine submissions for one real frame.
	// FFmpeg scales its strength internally by bit depth; do not scale these
	// 8-bit-equivalent heuristic strengths a second time for 10/12/16-bit input.
	std::array<bool, 3> received{};
	const auto Accept = [&](FFrame& candidate) noexcept {
		if (candidate.Width() <= 0)
			return true;
		if (!SameLayout(candidate, current) || candidate.Pts() < 0 || candidate.Pts() > 2)
			return false;
		const auto timestamp = static_cast<std::size_t>(candidate.Pts());
		if (received[timestamp])
			return false;
		received[timestamp] = true;
		if (timestamp == 1)
			output = std::move(candidate);
		return true;
	};
	for (int timestamp = 0; timestamp < 3; ++timestamp) {
		if (timestamp > 0 && !input.Ref(current))
			return false;
		input.Pts(timestamp);
		FFrame candidate;
		if (!graph.Filter(input, candidate) || !Accept(candidate))
			return false;
	}
	// Three inputs imply at most three outputs. The extra drain detects an
	// unexpected fourth output without permitting an unbounded backend loop.
	for (int drain = 0; drain < 4; ++drain) {
		FFrame candidate;
		if (!graph.Flush(candidate))
			return false;
		const bool hasPicture = candidate.Width() > 0;
		if (!Accept(candidate))
			return false;
		if (!hasPicture)
			return received[0] && received[1] && received[2];
	}
	return false;
}

bool Degrain::Apply(const FFrame& source, FFrame& output) noexcept {
	const auto found = m_lookup.find(source.Pts());
	if (source.Pts() == MissingPts || found == m_lookup.end() || !m_processed.insert(source.Pts()).second
		|| !m_row[found->second].valid || m_row[found->second].width != source.Width()
		|| m_row[found->second].height != source.Height() || m_row[found->second].format != source.Format()) {
		m_previous.reset();
		m_previousRow.reset();
		return false;
	}
	if (!source.PlanarInteger()) {
		++m_unsupported;
		m_previous.reset();
		m_previousRow.reset();
		return false;
	}
	const std::size_t index = found->second;
	const auto& row = m_row[index];
	const FFrame& previous = m_previous && m_previousRow && *m_previousRow + 1 == index
		&& m_row[*m_previousRow].group == row.group && SameLayout(*m_previous, source)
		? *m_previous : source;
	const double maximum = std::min(std::clamp(m_capIn.value_or(DefaultCap), 0.0, 100.0),
		static_cast<double>(*std::max_element(row.regions.begin(), row.regions.end())));
	std::array<FFrame, 3> variants;
	std::array<double, 3> levels{};
	if (maximum > 0.0) {
		// Quantize the maximum into three evenly spaced strengths. Every target
		// lies between zero/original and two adjacent filtered levels. There is
		// no single strong-frame alpha approximation, and no level exceeds cap.
		for (std::size_t level = 0; level < variants.size(); ++level) {
			levels[level] = maximum * (level + 1) / variants.size();
			if (!FilterWindow(previous, source, levels[level], variants[level])) {
				Fail("degrain: temporal window failed or returned ambiguous/incompatible outputs");
				return false;
			}
		}
		// Deep copy preserves alpha (never mixed), untouched samples and all
		// properties/side data. Graph-generated properties never replace the
		// source metadata. Byte-wise sample access avoids alignment assumptions
		// and handles LE/BE 8/10/12/16-bit planes on every host endian/CRT.
		if (!output.AllocVideo(source.Width(), source.Height(), source.Format())
			|| !output.Copy(source) || !output.CopyProps(source)) {
			Fail("degrain: output copy failed");
			return false;
		}
		const int bytes = source.BitsPerComponent() > 8 ? 2 : 1;
		const bool bigEndian = source.BigEndianSamples();
		const unsigned limit = (1u << source.BitsPerComponent()) - 1;
		for (int plane = 0; plane < std::min(3, source.PlaneCount()); ++plane) {
			const int width = source.PlaneWidth(plane);
			const int height = source.PlaneHeight(plane);
			for (int vertical = 0; vertical < height; ++vertical) {
				auto* destination = output.Data(plane) + static_cast<std::ptrdiff_t>(vertical) * output.Linesize(plane);
				const auto* original = source.Data(plane) + static_cast<std::ptrdiff_t>(vertical) * source.Linesize(plane);
				std::array<const uint8_t*, 3> filtered{};
				for (std::size_t level = 0; level < variants.size(); ++level)
					filtered[level] = variants[level].Data(plane) + static_cast<std::ptrdiff_t>(vertical) * variants[level].Linesize(plane);
				for (int horizontal = 0; horizontal < width; ++horizontal) {
					const double target = std::min(maximum,
						RegionTarget(row.regions, (horizontal + 0.5) / width, (vertical + 0.5) / height));
					if (target <= 0.0)
						continue;
					const std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(horizontal) * bytes;
					std::size_t upper = 0;
					while (upper + 1 < levels.size() && target > levels[upper])
						++upper;
					const double lowerSigma = upper == 0 ? 0.0 : levels[upper - 1];
					const double fraction = std::clamp((target - lowerSigma) / (levels[upper] - lowerSigma), 0.0, 1.0);
					const unsigned lowerSample = ReadSample((upper == 0 ? original : filtered[upper - 1]) + offset, bytes, bigEndian);
					const unsigned upperSample = ReadSample(filtered[upper] + offset, bytes, bigEndian);
					const double mixed = (1.0 - fraction) * lowerSample + fraction * upperSample;
					WriteSample(destination + offset, static_cast<unsigned>(std::clamp(std::round(mixed), 0.0, static_cast<double>(limit))), bytes, bigEndian);
				}
			}
		}
		output.Pts(source.Pts());
	}
	// Remember the REAL input even when its targets are zero, never a filtered
	// result. This prevents feedback and bounds encode history to one picture.
	if (!m_previous)
		m_previous = std::make_unique<FFrame>();
	if (!m_previous->AllocVideo(source.Width(), source.Height(), source.Format())
		|| !m_previous->Copy(source) || !m_previous->CopyProps(source)) {
		Fail("degrain: previous input copy failed");
		return false;
	}
	m_previousRow = index;
	return maximum > 0.0;
}

void Degrain::Measure(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video || m_voted)
		return;
	if (m_capIn && !std::isfinite(*m_capIn)) {
		Fail("degrain: sigma cap must be finite");
		return;
	}
	const FFrame& source = AVFrame();
	if (source)
		PushFrame(source);
}

void Degrain::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	if (!m_voted)
		Decide();
	if (m_capIn && !std::isfinite(*m_capIn))
		return;
	const FFrame& source = AVFrame();
	if (!source)
		return;
	FFrame output;
	if (Apply(source, output)) {
		++m_applied;
		Save(std::move(output));
	}
}

void Degrain::Eof() noexcept {
	Decide();
	m_previous.reset();
	m_previousRow.reset();
}

class StormByte::Multimedia::Pipeline::Filter::Report Degrain::Report() const noexcept {
	const bool ok = m_voted && m_ran > 0;
	std::map<std::string, std::string> data;
	data.emplace("status", ok ? "ok" : "noop");
	data.emplace("frames", std::to_string(m_frames));
	data.emplace("ran", std::to_string(m_ran));
	data.emplace("skipped", std::to_string(m_skipped));
	data.emplace("groups", std::to_string(m_row.empty() ? 0 : m_group + 1));
	data.emplace("applied", std::to_string(m_applied));
	data.emplace("unsupported", std::to_string(m_unsupported));
	data.emplace("rejected", std::to_string(m_rejected));
	data.emplace("sigma", std::format("{:.3f}", m_sigmaP50));
	data.emplace("sigma_min", std::format("{:.3f}", m_sigmaMin));
	data.emplace("sigma_max", std::format("{:.3f}", m_sigmaMax));
	data.emplace("sigma_p50", std::format("{:.3f}", m_sigmaP50));
	return Filter::Report(ok ? Filter::Report::Status::Ok : Filter::Report::Status::None, std::move(data));
}
