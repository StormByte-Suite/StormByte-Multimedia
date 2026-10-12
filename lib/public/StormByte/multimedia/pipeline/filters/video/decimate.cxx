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
#include <StormByte/multimedia/pipeline/filters/video/decimate.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <format>
#include <limits>
#include <utility>

extern "C" {
	#include <libavutil/avutil.h>
	#include <libavutil/rational.h>
}

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Decimate;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Decimate::Decimate(Safe::Shared<StormByte::Logger::Log> log,
	Safe::Optional<unsigned> cycle,
	Safe::Optional<double> dupthresh,
	Safe::Optional<double> scthresh) noexcept
	: Filter::ProcessTwoPasses(std::move(log), Safe::String("decimate")),
	m_cycleIn(cycle), m_dupIn(dupthresh), m_scIn(scthresh) {}

Decimate::~Decimate() noexcept = default;

enum Type Decimate::Media() const noexcept {
	return Type::Video;
}

void Decimate::Clean() noexcept {
	m_graph.reset();
	m_rate = {0, 1};
	m_timeBase = {0, 1};
	m_lastPts.reset();
	m_ticks = 0;
	m_intervals = 0;
	m_minDelta = 0;
	m_maxDelta = 0;
	m_durationTicks = 0;
	m_durationValid = true;
	m_timingValid = true;
}

void Decimate::Setup() noexcept {
	m_graph.reset();
}

void Decimate::Observe(const FFrame& src, Property::AVRational rate) noexcept {
	if (rate.num > 0 && rate.den > 0) {
		m_rate = rate;
		return;
	}
	const auto clock = src.TimeBase();
	if (clock.num <= 0 || clock.den <= 0) {
		m_timingValid = false;
		return;
	}
	if (m_timeBase.num == 0)
		m_timeBase = clock;
	else if (m_timeBase != clock) {
		m_timingValid = false;
		return;
	}
	const auto duration = src.DurationTicks();
	if (duration <= 0 || (m_durationTicks > 0 && duration != m_durationTicks))
		m_durationValid = false;
	else
		m_durationTicks = duration;
	const auto pts = src.Pts();
	if (pts == AV_NOPTS_VALUE) {
		m_lastPts.reset();
		return;
	}
	if (m_lastPts) {
		if (pts <= *m_lastPts || (*m_lastPts < 0 && pts > std::numeric_limits<std::int64_t>::max() + *m_lastPts)) {
			m_timingValid = false;
			return;
		}
		const auto delta = pts - *m_lastPts;
		if (m_ticks > std::numeric_limits<std::int64_t>::max() - delta
			|| m_intervals == std::numeric_limits<std::int64_t>::max()) {
			m_timingValid = false;
			return;
		}
		m_ticks += delta;
		m_minDelta = m_intervals == 0 ? delta : std::min(m_minDelta, delta);
		m_maxDelta = std::max(m_maxDelta, delta);
		++m_intervals;
	}
	m_lastPts = pts;
}

void Decimate::Measure(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0)
		return;
	Observe(src, frame.Video() && frame.Video()->FrameRate()
		? *frame.Video()->FrameRate() : Property::AVRational{0, 1});
}

void Decimate::Measured() noexcept {
	if (m_rate.num > 0 && m_rate.den > 0)
		return;
	if (!m_timingValid || m_timeBase.num <= 0 || m_timeBase.den <= 0)
		return;
	const auto ticks = m_intervals > 0 ? m_ticks : m_durationTicks;
	const auto intervals = m_intervals > 0 ? m_intervals : 1;
	if (ticks <= 0 || (m_intervals > 0 ? m_maxDelta - m_minDelta > 1 : !m_durationValid))
		return;
	const auto limit = std::numeric_limits<std::int64_t>::max();
	if (intervals > limit / m_timeBase.den || ticks > limit / m_timeBase.num)
		return;
	Property::AVRational resolved;
	if (av_reduce(&resolved.num, &resolved.den, intervals * m_timeBase.den,
		ticks * m_timeBase.num, std::numeric_limits<int>::max()))
		m_rate = resolved;
}

StormByte::Multimedia::Property::AVRational Decimate::Cadence(Property::AVRational rate) const noexcept {
	return rate.num > 0 && rate.den > 0 ? rate : m_rate;
}

StormByte::Safe::String Decimate::Chain() const noexcept {
	unsigned cycle = std::clamp(m_cycleIn.value_or(5u), 2u, 25u);
	const double dup = std::clamp(m_dupIn.value_or(1.1), 0.0, 100.0);
	const double sc = std::clamp(m_scIn.value_or(15.0), 0.0, 100.0);
	return StormByte::Safe::String{std::format("decimate=cycle={}:dupthresh={}:scthresh={}",
		cycle, dup, sc)};
}

void Decimate::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "decimate: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("decimate: hardware frame; decode to software first");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	const auto frameRate = Cadence(frame.Video() && frame.Video()->FrameRate()
		? *frame.Video()->FrameRate() : Property::AVRational{0, 1});
	if (frameRate.num <= 0 || frameRate.den <= 0) {
		Fail("decimate: cannot resolve a constant cadence from stream FPS or frame timing");
		return;
	}
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain, frameRate);
		if (!opened) {
			Fail("decimate: AVFilterGraph::Open failed");
			return;
		}
		m_graph = Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain, frameRate)) {
		Fail("decimate: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("decimate: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Drop();
		Log(Level::LowLevel, std::format(
			"decimate wait {}x{} pts={}",
			src.Width(), src.Height(), src.Pts()));
		return;
	}

	out.ColorRange(src.ColorRange());
	out.ColorSpace(src.ColorSpace());
	out.ColorPrimaries(src.ColorPrimaries());
	out.ColorTransfer(src.ColorTransfer());
	out.ChromaLocation(src.ChromaLocation());
	out.SampleAspectRatio(src.SampleAspectRatio());

	Log(Level::LowLevel, std::format(
		"decimate {}x{} pts={}",
		out.Width(), out.Height(), out.Pts()));
	EmitGraph(*m_graph, std::move(out), m_graph->FrameRate());
}

void Decimate::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("decimate: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"decimate flush {}x{} pts={}",
			out.Width(), out.Height(), out.Pts()));
		EmitFrame(std::move(out), m_graph->FrameRate());
	}
	m_graph.reset();
}
