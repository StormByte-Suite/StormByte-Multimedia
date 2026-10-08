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

#include <StormByte/multimedia/pipeline/filters/analytics/ssim.hxx>

#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::Filter::Video::SSIM;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

/**
 * @brief Provider-owned per-track presentation park and SSIM accumulators.
 */
struct StormByte::Multimedia::Pipeline::Filter::Video::Detail::SSIM::Lane {
	/**
	 * @brief Running SSIM for one plane of one track.
	 */
	struct Plane {
		double sum = 0.0;		///< Sum of per-frame SSIM
		unsigned frames = 0;	///< Frames that contributed
	};

	std::deque<FFrame> ref;			///< Decoder looks
	std::deque<FFrame> dist;			///< Dest looks
	int width = 0;					///< Latched width
	int height = 0;					///< Latched height
	int bpc = 0;					///< Latched bits per component
	unsigned scored = 0;			///< Accepted pairs
	std::size_t peakRef = 0;			///< Peak parked refs
	std::size_t peakDist = 0;			///< Peak parked dists
	Plane y;						///< Luma
	Plane u;						///< Cb, if the layout has it
	Plane v;						///< Cr, if the layout has it
	std::optional<double> mean;	///< Sample-weighted mean
	std::optional<double> min;	///< Minimum per-frame average
	bool failed = false;			///< No pair scored at EoF
};

namespace {
	constexpr int kWindow = 8;

	int Bpc(const FFrame& raw) noexcept {
		if (!raw)
			return 8;
		const int depth = raw.BitsPerComponent();
		if (depth <= 8)
			return 8;
		if (depth <= 10)
			return 10;
		return 12;
	}

	bool DistLook(Producer producer) noexcept {
		return producer == Producer::Encoder || producer == Producer::Remuxer;
	}

	double Sample(const uint8_t* row, int x, int bpc) noexcept {
		if (bpc <= 8)
			return static_cast<double>(row[x]);
		return static_cast<double>(reinterpret_cast<const uint16_t*>(row)[x]);
	}

	double PlaneSsim(const FFrame& ref, const FFrame& dist, int plane, int bpc) noexcept {
		const uint8_t* a = ref.Data(plane);
		const uint8_t* b = dist.Data(plane);
		const int w = ref.PlaneWidth(plane);
		const int h = ref.PlaneHeight(plane);
		const int as = ref.Linesize(plane);
		const int bs = dist.Linesize(plane);
		if (!a || !b || w < kWindow || h < kWindow || as <= 0 || bs <= 0)
			return -1.0;
		if (dist.PlaneWidth(plane) != w || dist.PlaneHeight(plane) != h)
			return -1.0;

		const double L = static_cast<double>((1 << bpc) - 1);
		const double c1 = (0.01 * L) * (0.01 * L);
		const double c2 = (0.03 * L) * (0.03 * L);
		const double n = static_cast<double>(kWindow * kWindow);

		double acc = 0.0;
		unsigned windows = 0;
		for (int y = 0; y + kWindow <= h; y += kWindow) {
			for (int x = 0; x + kWindow <= w; x += kWindow) {
				double sx = 0.0, sy = 0.0, sxx = 0.0, syy = 0.0, sxy = 0.0;
				for (int j = 0; j < kWindow; ++j) {
					const uint8_t* ra = a + (y + j) * as;
					const uint8_t* rb = b + (y + j) * bs;
					for (int i = 0; i < kWindow; ++i) {
						const double vx = Sample(ra, x + i, bpc);
						const double vy = Sample(rb, x + i, bpc);
						sx += vx;
						sy += vy;
						sxx += vx * vx;
						syy += vy * vy;
						sxy += vx * vy;
					}
				}
				const double mx = sx / n;
				const double my = sy / n;
				const double vx = sxx / n - mx * mx;
				const double vy = syy / n - my * my;
				const double cxy = sxy / n - mx * my;
				const double num = (2.0 * mx * my + c1) * (2.0 * cxy + c2);
				const double den = (mx * mx + my * my + c1) * (vx + vy + c2);
				if (den > 0.0)
					acc += num / den;
				++windows;
			}
		}
		if (windows == 0)
			return -1.0;
		return acc / static_cast<double>(windows);
	}

	double MeanSsim(double sum, unsigned frames) noexcept {
		if (frames == 0)
			return 0.0;
		return sum / static_cast<double>(frames);
	}
}

SSIM::SSIM(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept
	: Filter::Analytics(std::move(log), "ssim") {}

SSIM::~SSIM() noexcept {
	Clean();
}

enum Type SSIM::Media() const noexcept {
	return Type::Video;
}

void SSIM::DropParked(Lane& lane) noexcept {
	lane.ref.clear();
	lane.dist.clear();
}

void SSIM::DropAll() noexcept {
	for (const auto& [track, lane] : std::as_const(m_lanes)) {
		(void)track;
		DropParked(*lane);
	}
	m_lanes.clear();
}

void SSIM::Clean() noexcept {
	DropAll();
}

void SSIM::Setup() noexcept {
	Clean();
	Log(Level::Debug, "setup");
}

void SSIM::Score(Lane& lane, const FFrame& ref, const FFrame& dist) noexcept {
	if (!ref || !dist)
		return;
	if (ref.Width() <= 0 || ref.Height() <= 0 || dist.Width() <= 0 || dist.Height() <= 0) {
		Log(Level::Warning, "look has no picture size, skip pair");
		return;
	}

	if (lane.width == 0) {
		lane.width = ref.Width();
		lane.height = ref.Height();
		lane.bpc = Bpc(ref);
		Log(Level::Debug, std::format(
			"first pair latch={}x{} bpc={} ref_fmt={} ref_pts={} dist={}x{} dist_fmt={} dist_pts={}",
			lane.width, lane.height, lane.bpc,
			ref.FormatName(), ref.Pts(),
			dist.Width(), dist.Height(),
			dist.FormatName(), dist.Pts()));
		if (dist.Width() != lane.width || dist.Height() != lane.height)
			Log(Level::Notice, std::format("geometry ref={}x{} dist={}x{}, scaling dist",
				lane.width, lane.height, dist.Width(), dist.Height()));
	}
	else if (ref.Width() != lane.width || ref.Height() != lane.height) {
		Log(Level::Warning, std::format("skip pair, ref size {}x{} latch {}x{}",
			ref.Width(), ref.Height(), lane.width, lane.height));
		return;
	}

	const FFrame* d = &dist;
	FFrame scaled;
	if (dist.Width() != lane.width || dist.Height() != lane.height) {
		if (!dist.ScaleTo(scaled, lane.width, lane.height, FFrame::Resample::Bicubic, FFrame::Scaler::Sws) || !scaled) {
			Log(Level::Warning, "ScaleTo failed, skip pair");
			return;
		}
		d = &scaled;
	}

	const double y = PlaneSsim(ref, *d, 0, lane.bpc);
	if (y < 0.0) {
		Log(Level::Warning, "luma plane unreadable, skip pair");
		return;
	}
	lane.y.sum += y;
	++lane.y.frames;
	const double u = PlaneSsim(ref, *d, 1, lane.bpc);
	const double v = PlaneSsim(ref, *d, 2, lane.bpc);
	if (u >= 0.0) {
		lane.u.sum += u;
		++lane.u.frames;
	}
	if (v >= 0.0) {
		lane.v.sum += v;
		++lane.v.frames;
	}

	double acc = y;
	double n = 1.0;
	if (u >= 0.0) {
		acc += u;
		n += 1.0;
	}
	if (v >= 0.0) {
		acc += v;
		n += 1.0;
	}
	const double frameSsim = acc / n;
	if (!lane.min || frameSsim < *lane.min)
		lane.min = frameSsim;

	++lane.scored;
	Log(Level::LowLevel, std::format("scored n={} ssim={:.6f} ref_pts={} dist_pts={}",
		lane.scored, frameSsim, ref.Pts(), d->Pts()));
}

void SSIM::Drain(Lane& lane) noexcept {
	while (!lane.ref.empty() && !lane.dist.empty()) {
		FFrame ref = std::move(lane.ref.front());
		FFrame dist = std::move(lane.dist.front());
		lane.ref.pop_front();
		lane.dist.pop_front();
		Score(lane, ref, dist);
	}
}

void SSIM::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const Producer producer = frame.Producer();
	if (producer != Producer::Decoder && !DistLook(producer)) {
		Log(Level::Debug, std::format("ignore producer={}", ToString(producer)));
		return;
	}

	const FFrame& raw = AVFrame();
	if (!raw) {
		Log(Level::Warning, std::format("frame has no backend producer={}", ToString(producer)));
		return;
	}

	StormByte::Safe::Shared<Lane> lanePointer = m_lanes[frame.Track()];
	if (!lanePointer) {
			lanePointer = StormByte::Safe::MakeShared<Lane>();
		m_lanes[frame.Track()] = lanePointer;
	}
	Lane& lane = *lanePointer;
	if (lane.failed)
		return;

	FFrame clone = raw.Clone();
	if (!clone) {
		Log(Level::Warning, "AVFrame::Clone failed");
		return;
	}

	if (producer == Producer::Decoder) {
		if (lane.scored == 0 && lane.ref.empty())
			Log(Level::Debug, std::format("first ref t={} {}x{} fmt={} bpc={} pts={}",
				frame.Track(), raw.Width(), raw.Height(),
				raw.FormatName(), Bpc(raw), raw.Pts()));
		lane.ref.push_back(std::move(clone));
		if (lane.ref.size() > lane.peakRef)
			lane.peakRef = lane.ref.size();
	}
	else {
		if (lane.scored == 0 && lane.dist.empty())
			Log(Level::Debug, std::format("first dist t={} producer={} {}x{} fmt={} bpc={} pts={}",
				frame.Track(), ToString(producer),
				raw.Width(), raw.Height(), raw.FormatName(), Bpc(raw), raw.Pts()));
		lane.dist.push_back(std::move(clone));
		if (lane.dist.size() > lane.peakDist)
			lane.peakDist = lane.dist.size();
	}

	Log(Level::LowLevel, std::format("park t={} producer={} ref={} dist={}",
		frame.Track(), ToString(producer), lane.ref.size(), lane.dist.size()));
	Drain(lane);
}

void SSIM::Eof() noexcept {
	unsigned scored = 0;
	for (const auto& [track, lanePointer] : std::as_const(m_lanes)) {
		Lane& lane = *lanePointer;
		Drain(lane);
		Log(Level::Debug, std::format(
			"eof t={} scored={} ref={} dist={} peak_ref={} peak_dist={} failed={} latch={}x{}",
			track, lane.scored, lane.ref.size(), lane.dist.size(),
			lane.peakRef, lane.peakDist, lane.failed,
			lane.width, lane.height));
		if (!lane.ref.empty() || !lane.dist.empty())
			Log(Level::Warning, std::format("leftover looks t={} ref={} dist={}",
				track, lane.ref.size(), lane.dist.size()));
		DropParked(lane);
		if (lane.scored == 0) {
			Log(Level::Error, std::format("t={} no scored pairs", track));
			lane.failed = true;
			continue;
		}

		double acc = 0.0;
		double samples = 0.0;
		if (lane.y.frames) {
			acc += lane.y.sum;
			samples += static_cast<double>(lane.y.frames);
		}
		if (lane.u.frames) {
			acc += lane.u.sum;
			samples += static_cast<double>(lane.u.frames);
		}
		if (lane.v.frames) {
			acc += lane.v.sum;
			samples += static_cast<double>(lane.v.frames);
		}
		lane.mean = acc / samples;
		if (!lane.min)
			lane.min = lane.mean;
		scored += lane.scored;
		Log(Level::Notice, std::format("t={} mean={:.6f} min={:.6f} n={} latch={}x{}",
			track, *lane.mean, *lane.min, lane.scored, lane.width, lane.height));
	}

	if (m_lanes.empty())
		Log(Level::Error, "no scored pairs");
	else if (scored > 0)
		Log(Level::Debug, std::format("eof tracks={} scored={}", m_lanes.size(), scored));
}

class StormByte::Multimedia::Pipeline::Filter::Report SSIM::Report() const noexcept {
	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data;
	bool failed = m_lanes.empty();
	unsigned ok = 0;
	for (const auto& [track, lanePointer] : m_lanes) {
		const Lane& lane = *lanePointer;
		if (lane.failed || !lane.mean)
			failed = true;
		else
			++ok;
	}

	const bool prefix = m_lanes.size() > 1;
	const auto key = [prefix](int track, std::string_view name) {
		if (!prefix)
			return StormByte::Safe::String(name);
		return StormByte::Safe::String(std::format("{}.{}", track, name));
	};

	for (const auto& [track, lanePointer] : m_lanes) {
		const Lane& lane = *lanePointer;
		if (lane.mean && lane.min) {
			data.emplace(key(track, "ssim_mean"), std::format("{:.6f}", *lane.mean));
			data.emplace(key(track, "ssim_min"), std::format("{:.6f}", *lane.min));
			if (lane.y.frames)
				data.emplace(key(track, "ssim_y"), std::format("{:.6f}",
					MeanSsim(lane.y.sum, lane.y.frames)));
			if (lane.u.frames)
				data.emplace(key(track, "ssim_u"), std::format("{:.6f}",
					MeanSsim(lane.u.sum, lane.u.frames)));
			if (lane.v.frames)
				data.emplace(key(track, "ssim_v"), std::format("{:.6f}",
					MeanSsim(lane.v.sum, lane.v.frames)));
		}
		data.emplace(key(track, "frames"), std::to_string(lane.scored));
		data.emplace(key(track, "width"), std::to_string(lane.width));
		data.emplace(key(track, "height"), std::to_string(lane.height));
	}

	if (failed || ok == 0)
		return {Filter::Report::Status::Failed, std::move(data)};
	return {Filter::Report::Status::Ok, std::move(data)};
}
