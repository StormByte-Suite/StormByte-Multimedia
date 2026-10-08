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

#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/vmaf.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

extern "C" {
	#include <libvmaf/libvmaf.h>
	#include <libvmaf/model.h>
	#include <libvmaf/picture.h>
}

using StormByte::Multimedia::Pipeline::Filter::Video::VMAF;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::ToString;
using StormByte::Multimedia::Type;
using StormByte::Logger::Level;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

/**
 * @brief Provider-owned per-track libvmaf context and presentation park.
 */
struct StormByte::Multimedia::Pipeline::Filter::Video::Detail::VMAF::Lane {
	VmafContext* vmaf = nullptr;	///< libvmaf context
	VmafModel* model = nullptr;	///< Loaded model
	std::deque<FFrame> ref;		///< Decoder looks
	std::deque<FFrame> dist;		///< Dest looks
	int width = 0;				///< Latched width
	int height = 0;				///< Latched height
	unsigned index = 0;			///< Next accepted libvmaf index
	unsigned scored = 0;		///< Accepted pairs
	std::size_t peakRef = 0;		///< Peak parked refs
	std::size_t peakDist = 0;		///< Peak parked dists
	std::optional<double> mean;	///< Pooled mean
	std::optional<double> min;	///< Pooled min
	bool failed = false;		///< Context or score failure
};

namespace {
	enum VmafPixelFormat Pix(const FFrame& raw) noexcept {
		switch (raw.Layout()) {
			case FFrame::VideoLayout::Yuv420:	return VMAF_PIX_FMT_YUV420P;
			case FFrame::VideoLayout::Yuv422:	return VMAF_PIX_FMT_YUV422P;
			case FFrame::VideoLayout::Yuv444:	return VMAF_PIX_FMT_YUV444P;
			case FFrame::VideoLayout::Gray:		return VMAF_PIX_FMT_YUV400P;
			default:							return VMAF_PIX_FMT_UNKNOWN;
		}
	}

	unsigned Bpc(const FFrame& raw) noexcept {
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

	void CopyPlane(const uint8_t* src, int srcStride, uint8_t* dst, ptrdiff_t dstStride,
		int bytes, int height) noexcept {
		for (int y = 0; y < height; ++y)
			std::memcpy(dst + y * dstStride, src + y * srcStride, static_cast<std::size_t>(bytes));
	}
}

VMAF::VMAF(StormByte::Safe::Shared<StormByte::Logger::Log> log, StormByte::Safe::String model,
	StormByte::Safe::Optional<unsigned short> threads) noexcept
: Filter::Analytics(std::move(log), "vmaf"),
	m_modelName(std::move(model)), m_threads(threads) {}

VMAF::~VMAF() noexcept {
	Clean();
}

enum Type VMAF::Media() const noexcept {
	return Type::Video;
}

void VMAF::DropParked(Lane& lane) noexcept {
	lane.ref.clear();
	lane.dist.clear();
}

void VMAF::DropAll() noexcept {
	for (const auto& [track, lanePointer] : std::as_const(m_lanes)) {
		Lane& lane = *lanePointer;
		(void)track;
		DropParked(lane);
		if (lane.vmaf) {
			vmaf_close(lane.vmaf);
			lane.vmaf = nullptr;
		}

		if (lane.model) {
			vmaf_model_destroy(lane.model);
			lane.model = nullptr;
		}
	}

	m_lanes.clear();
}

void VMAF::Clean() noexcept {
	DropAll();
}

bool VMAF::OpenLane(Lane& lane) noexcept {
	VmafConfiguration cfg{};
	cfg.log_level = VMAF_LOG_LEVEL_WARNING;
	cfg.n_threads = Threads();
	cfg.n_subsample = 1;
	if (vmaf_init(&lane.vmaf, cfg) != 0) {
		Log(Level::Error, "vmaf_init failed");
		lane.vmaf = nullptr;
		lane.failed = true;
		return false;
	}

	VmafModelConfig modelCfg{};
	modelCfg.name = "vmaf";
	modelCfg.flags = VMAF_MODEL_FLAGS_DEFAULT;
	if (vmaf_model_load(&lane.model, &modelCfg, m_modelName.Bytes()) != 0) {
		Log(Level::Error, std::format("vmaf_model_load({}) failed", static_cast<std::string_view>(m_modelName)));
		vmaf_close(lane.vmaf);
		lane.vmaf = nullptr;
		lane.failed = true;
		return false;
	}

	if (vmaf_use_features_from_model(lane.vmaf, lane.model) != 0) {
		Log(Level::Error, "vmaf_use_features_from_model failed");
		vmaf_close(lane.vmaf);
		lane.vmaf = nullptr;
		vmaf_model_destroy(lane.model);
		lane.model = nullptr;
		lane.failed = true;
		return false;
	}

	Log(Level::Debug, std::format("libvmaf ready threads={} subsample=1", cfg.n_threads));
	return true;
}

void VMAF::Setup() noexcept {
	Clean();
	Log(Level::Debug, std::format("setup model={}", static_cast<std::string_view>(m_modelName)));
}

bool VMAF::Fill(Lane& lane, const FFrame& raw, int tw, int th, void* out) noexcept {
	auto* pic = static_cast<VmafPicture*>(out);
	if (!raw || !pic || tw <= 0 || th <= 0)
		return false;
	const unsigned bpc = Bpc(raw);
	const auto fmt = Pix(raw);
	if (fmt == VMAF_PIX_FMT_UNKNOWN)
		return false;
	if (lane.scored == 0) {
		if (vmaf_picture_alloc(pic, fmt, bpc, static_cast<unsigned>(tw), static_cast<unsigned>(th)) != 0)
			return false;
	}
	else {
		if (vmaf_fetch_preallocated_picture(lane.vmaf, pic) != 0)
			return false;
		if (pic->pix_fmt != fmt || pic->bpc != bpc) {
			vmaf_picture_unref(pic);
			return false;
		}
	}

	const FFrame* src = &raw;
	FFrame scaled;
	if (raw.Width() != tw || raw.Height() != th) {
		if (!raw.ScaleTo(scaled, tw, th, FFrame::Resample::Bicubic, FFrame::Scaler::Sws) || !scaled) {
			vmaf_picture_unref(pic);
			return false;
		}
		src = &scaled;
	}

	const int planes = (fmt == VMAF_PIX_FMT_YUV400P) ? 1 : 3;
	const int bytesPel = (bpc > 8) ? 2 : 1;
	for (int p = 0; p < planes; ++p) {
		if (!src->Data(p) || !pic->data[p])
			continue;
		const int height = static_cast<int>(pic->h[p]);
		const int widthBytes = static_cast<int>(pic->w[p]) * bytesPel;
		CopyPlane(src->Data(p), src->Linesize(p),
			static_cast<uint8_t*>(pic->data[p]), pic->stride[p],
			widthBytes, height);
	}
	return true;
}

void VMAF::Score(Lane& lane, const FFrame& ref, const FFrame& dist, unsigned index) noexcept {
	if (!lane.vmaf || !ref || !dist)
		return;
	if (ref.Width() <= 0 || ref.Height() <= 0 || dist.Width() <= 0 || dist.Height() <= 0) {
		Log(Level::Warning, "look has no picture size, skip pair");
		return;
	}

	if (lane.width == 0) {
		lane.width = ref.Width();
		lane.height = ref.Height();
		Log(Level::Debug, std::format(
			"first pair index={} latch={}x{} ref_fmt={} ref_bpc={} ref_pts={} dist={}x{} dist_fmt={} dist_bpc={} dist_pts={}",
			index, lane.width, lane.height,
			ref.FormatName(), Bpc(ref), ref.Pts(),
			dist.Width(), dist.Height(),
			dist.FormatName(), Bpc(dist), dist.Pts()));
		if (dist.Width() != lane.width || dist.Height() != lane.height)
			Log(Level::Notice, std::format("geometry ref={}x{} dist={}x{}, scaling dist",
				lane.width, lane.height, dist.Width(), dist.Height()));
	}

	else if (ref.Width() != lane.width || ref.Height() != lane.height) {
		Log(Level::Warning, std::format("skip pair, ref size {}x{} latch {}x{}",
			ref.Width(), ref.Height(), lane.width, lane.height));
		return;
	}

	VmafPicture pref{};
	VmafPicture pdist{};
	if (!Fill(lane, ref, lane.width, lane.height, &pref)
		|| !Fill(lane, dist, lane.width, lane.height, &pdist)) {
		Log(Level::Warning, std::format(
			"skip pair, fill failed ref={}x{} dist={}x{} latch={}x{}",
			ref.Width(), ref.Height(), dist.Width(), dist.Height(),
			lane.width, lane.height));
		vmaf_picture_unref(&pref);
		vmaf_picture_unref(&pdist);
		return;
	}

	if (vmaf_read_pictures(lane.vmaf, &pref, &pdist, index) != 0) {
		Log(Level::Warning, "skip pair, vmaf_read_pictures failed");
		vmaf_picture_unref(&pref);
		vmaf_picture_unref(&pdist);
		return;
	}

	++lane.scored;
	Log(Level::LowLevel, std::format("scored index={} n={} ref_pts={} dist_pts={}",
		index, lane.scored, ref.Pts(), dist.Pts()));
}

void VMAF::Drain(Lane& lane) noexcept {
	while (!lane.ref.empty() && !lane.dist.empty()) {
		FFrame ref = std::move(lane.ref.front());
		FFrame dist = std::move(lane.dist.front());
		lane.ref.pop_front();
		lane.dist.pop_front();
		const unsigned before = lane.scored;
		Score(lane, ref, dist, lane.index);
		if (lane.scored > before)
			++lane.index;
	}
}

unsigned VMAF::Threads() const noexcept {
	if (m_threads)
		return std::max<unsigned>(1u, *m_threads);
	if (const unsigned hw = std::thread::hardware_concurrency(); hw > 0)
		return hw;
	return 1;
}

void VMAF::Process(const Pipeline::Frame& frame) noexcept {
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
	if (!lane.vmaf && !OpenLane(lane))
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

void VMAF::Eof() noexcept {
	unsigned scored = 0;
	for (const auto& [track, lanePointer] : std::as_const(m_lanes)) {
		Lane& lane = *lanePointer;
		Drain(lane);
		Log(Level::Debug, std::format("eof t={} scored={} ref={} dist={} peak_ref={} peak_dist={} failed={} latch={}x{}",
			track, lane.scored, lane.ref.size(), lane.dist.size(),
			lane.peakRef, lane.peakDist, lane.failed,
			lane.width, lane.height));
		if (!lane.ref.empty() || !lane.dist.empty())
			Log(Level::Warning, std::format("leftover looks t={} ref={} dist={}",
				track, lane.ref.size(), lane.dist.size()));
		DropParked(lane);
		if (lane.vmaf && lane.scored > 0)
			vmaf_read_pictures(lane.vmaf, nullptr, nullptr, 0);
		if (lane.failed || !lane.vmaf || !lane.model || lane.scored == 0) {
			if (!lane.failed)
				Log(Level::Error, std::format("t={} no scored pairs", track));
			lane.failed = true;
			continue;
		}

		double mean = 0;
		double mn = 0;
		const unsigned last = lane.scored - 1;
		if (vmaf_score_pooled(lane.vmaf, lane.model, VMAF_POOL_METHOD_MEAN, &mean, 0, last) != 0
			|| vmaf_score_pooled(lane.vmaf, lane.model, VMAF_POOL_METHOD_MIN, &mn, 0, last) != 0) {
			Log(Level::Error, std::format("t={} vmaf_score_pooled failed", track));
			lane.failed = true;
			continue;
		}

		lane.mean = mean;
		lane.min = mn;
		scored += lane.scored;
		Log(Level::Notice, std::format("t={} mean={:.3f} min={:.3f} n={} latch={}x{}",
			track, mean, mn, lane.scored, lane.width, lane.height));
	}

	if (m_lanes.empty())
		Log(Level::Error, "no scored pairs");
	else if (scored > 0)
		Log(Level::Debug, std::format("eof tracks={} scored={}", m_lanes.size(), scored));
}

class StormByte::Multimedia::Pipeline::Filter::Report VMAF::Report() const noexcept {
	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data;
	data.emplace(StormByte::Safe::String("model"), m_modelName);
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
	auto key = [prefix](int track, const char* name) {
		if (!prefix)
			return StormByte::Safe::String(name);
		return StormByte::Safe::String(std::format("{}.{}", track, name));
	};

	for (const auto& [track, lanePointer] : m_lanes) {
		const Lane& lane = *lanePointer;
		if (lane.mean) {
			data.emplace(key(track, "vmaf_mean"), std::format("{:.6f}", *lane.mean));
			data.emplace(key(track, "vmaf_min"), std::format("{:.6f}", *lane.min));
		}

		data.emplace(key(track, "frames"), std::to_string(lane.scored));
		data.emplace(key(track, "width"), std::to_string(lane.width));
		data.emplace(key(track, "height"), std::to_string(lane.height));
	}

	if (failed || ok == 0)
		return {Filter::Report::Status::Failed, std::move(data)};
	return {Filter::Report::Status::Ok, std::move(data)};
}
