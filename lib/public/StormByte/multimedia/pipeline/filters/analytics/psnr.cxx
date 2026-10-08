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

#include <StormByte/multimedia/pipeline/filters/analytics/psnr.hxx>

#include <cmath>
#include <deque>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::Filter::Video::PSNR;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

/**
 * @brief Provider-owned per-track presentation park and PSNR accumulators.
 */
struct StormByte::Multimedia::Pipeline::Filter::Video::Detail::PSNR::Lane {
	/**
	 * @brief Running MSE for one plane of one track.
	 */
	struct Plane {
		double mse = 0.0;		///< Sum of per-frame MSE
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
	constexpr double kCap = 100.0;

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

	double FromMse(double mse, double peak) noexcept {
		if (!(mse > 0.0) || !(peak > 0.0))
			return kCap;
		return std::min(kCap, 10.0 * std::log10((peak * peak) / mse));
	}

	double PlaneMse(const FFrame& ref, const FFrame& dist, int plane, int bpc) noexcept {
		const uint8_t* a = ref.Data(plane);
		const uint8_t* b = dist.Data(plane);
		const int w = ref.PlaneWidth(plane);
		const int h = ref.PlaneHeight(plane);
		const int as = ref.Linesize(plane);
		const int bs = dist.Linesize(plane);
		if (!a || !b || w <= 0 || h <= 0 || as <= 0 || bs <= 0)
			return -1.0;
		if (dist.PlaneWidth(plane) != w || dist.PlaneHeight(plane) != h)
			return -1.0;

		double acc = 0.0;
		const double n = static_cast<double>(w) * static_cast<double>(h);
		if (bpc <= 8) {
			for (int y = 0; y < h; ++y) {
				const uint8_t* ra = a + y * as;
				const uint8_t* rb = b + y * bs;
				for (int x = 0; x < w; ++x) {
					const int d = static_cast<int>(ra[x]) - static_cast<int>(rb[x]);
					acc += static_cast<double>(d) * static_cast<double>(d);
				}
			}
		}
		else {
			for (int y = 0; y < h; ++y) {
				const auto* ra = reinterpret_cast<const uint16_t*>(a + y * as);
				const auto* rb = reinterpret_cast<const uint16_t*>(b + y * bs);
				for (int x = 0; x < w; ++x) {
					const int d = static_cast<int>(ra[x]) - static_cast<int>(rb[x]);
					acc += static_cast<double>(d) * static_cast<double>(d);
				}
			}
		}
		return acc / n;
	}

	double MeanPsnr(double mseSum, unsigned frames, double peak) noexcept {
		if (frames == 0)
			return 0.0;
		return FromMse(mseSum / static_cast<double>(frames), peak);
	}
}

PSNR::PSNR(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept
	: Filter::Analytics(std::move(log), "psnr") {}

PSNR::~PSNR() noexcept {
	Clean();
}

enum Type PSNR::Media() const noexcept {
	return Type::Video;
}

void PSNR::DropParked(Lane& lane) noexcept {
	lane.ref.clear();
	lane.dist.clear();
}

void PSNR::DropAll() noexcept {
	for (const auto& [track, owner] : std::as_const(m_lanes)) {
		(void)track;
		DropParked(*owner);
	}
	m_lanes.clear();
}

void PSNR::Clean() noexcept {
	DropAll();
}

void PSNR::Setup() noexcept {
	Clean();
	Log(Level::Debug, "setup");
}

void PSNR::Score(Lane& lane, const FFrame& ref, const FFrame& dist) noexcept {
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

	const double y = PlaneMse(ref, *d, 0, lane.bpc);
	if (y < 0.0) {
		Log(Level::Warning, "luma plane unreadable, skip pair");
		return;
	}
	lane.y.mse += y;
	++lane.y.frames;
	const double u = PlaneMse(ref, *d, 1, lane.bpc);
	const double v = PlaneMse(ref, *d, 2, lane.bpc);
	if (u >= 0.0) {
		lane.u.mse += u;
		++lane.u.frames;
	}
	if (v >= 0.0) {
		lane.v.mse += v;
		++lane.v.frames;
	}

	const double peak = static_cast<double>((1 << lane.bpc) - 1);
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
	const double framePsnr = FromMse(acc / n, peak);
	if (!lane.min || framePsnr < *lane.min)
		lane.min = framePsnr;

	++lane.scored;
	Log(Level::LowLevel, std::format("scored n={} psnr={:.3f} ref_pts={} dist_pts={}",
		lane.scored, framePsnr, ref.Pts(), d->Pts()));
}

void PSNR::Drain(Lane& lane) noexcept {
	while (!lane.ref.empty() && !lane.dist.empty()) {
		FFrame ref = std::move(lane.ref.front());
		FFrame dist = std::move(lane.dist.front());
		lane.ref.pop_front();
		lane.dist.pop_front();
		Score(lane, ref, dist);
	}
}

void PSNR::Process(const Pipeline::Frame& frame) noexcept {
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

	StormByte::Safe::Shared<Lane> owner = std::as_const(m_lanes).contains(frame.Track())
		? std::as_const(m_lanes).at(frame.Track()) : StormByte::Safe::Shared<Lane>{};
	if (!owner) {
			owner = StormByte::Safe::MakeShared<Lane>();
		m_lanes[frame.Track()] = owner;
	}
	Lane& lane = *owner;
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

void PSNR::Eof() noexcept {
	unsigned scored = 0;
	for (const auto& [track, owner] : std::as_const(m_lanes)) {
		Lane& lane = *owner;
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

		const double peak = static_cast<double>((1 << lane.bpc) - 1);
		double acc = 0.0;
		double samples = 0.0;
		if (lane.y.frames) {
			acc += lane.y.mse;
			samples += static_cast<double>(lane.y.frames);
		}
		if (lane.u.frames) {
			acc += lane.u.mse;
			samples += static_cast<double>(lane.u.frames);
		}
		if (lane.v.frames) {
			acc += lane.v.mse;
			samples += static_cast<double>(lane.v.frames);
		}
		lane.mean = FromMse(acc / samples, peak);
		if (!lane.min)
			lane.min = lane.mean;
		scored += lane.scored;
		Log(Level::Notice, std::format("t={} mean={:.3f} min={:.3f} n={} latch={}x{}",
			track, *lane.mean, *lane.min, lane.scored, lane.width, lane.height));
	}

	if (m_lanes.empty())
		Log(Level::Error, "no scored pairs");
	else if (scored > 0)
		Log(Level::Debug, std::format("eof tracks={} scored={}", m_lanes.size(), scored));
}

class StormByte::Multimedia::Pipeline::Filter::Report PSNR::Report() const noexcept {
	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data;
	bool failed = m_lanes.empty();
	unsigned ok = 0;
	for (const auto& [track, owner] : m_lanes) {
		const Lane& lane = *owner;
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

	for (const auto& [track, owner] : m_lanes) {
		const Lane& lane = *owner;
		if (lane.mean && lane.min) {
			const double peak = static_cast<double>((1 << lane.bpc) - 1);
			data.emplace(key(track, "psnr_mean"), std::format("{:.6f}", *lane.mean));
			data.emplace(key(track, "psnr_min"), std::format("{:.6f}", *lane.min));
			if (lane.y.frames)
				data.emplace(key(track, "psnr_y"), std::format("{:.6f}",
					MeanPsnr(lane.y.mse, lane.y.frames, peak)));
			if (lane.u.frames)
				data.emplace(key(track, "psnr_u"), std::format("{:.6f}",
					MeanPsnr(lane.u.mse, lane.u.frames, peak)));
			if (lane.v.frames)
				data.emplace(key(track, "psnr_v"), std::format("{:.6f}",
					MeanPsnr(lane.v.mse, lane.v.frames, peak)));
		}
		data.emplace(key(track, "frames"), std::to_string(lane.scored));
		data.emplace(key(track, "width"), std::to_string(lane.width));
		data.emplace(key(track, "height"), std::to_string(lane.height));
	}

	if (failed || ok == 0)
		return {Filter::Report::Status::Failed, std::move(data)};
	return {Filter::Report::Status::Ok, std::move(data)};
}
