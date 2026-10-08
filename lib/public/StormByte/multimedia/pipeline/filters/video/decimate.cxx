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
#include <StormByte/multimedia/pipeline/filters/video/decimate.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Decimate;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Decimate::Decimate(Safe::Shared<StormByte::Logger::Log> log,
	Safe::Optional<unsigned> cycle,
	Safe::Optional<double> dupthresh,
	Safe::Optional<double> scthresh) noexcept
	: Filter::Process(std::move(log), Safe::String("decimate")),
	m_cycleIn(cycle), m_dupIn(dupthresh), m_scIn(scthresh) {}

Decimate::~Decimate() noexcept = default;

enum Type Decimate::Media() const noexcept {
	return Type::Video;
}

void Decimate::Clean() noexcept {
	m_graph.reset();
}

void Decimate::Setup() noexcept {
	Clean();
}

std::string Decimate::Chain() const noexcept {
	unsigned cycle = std::clamp(m_cycleIn.value_or(5u), 2u, 25u);
	const double dup = std::clamp(m_dupIn.value_or(1.1), 0.0, 100.0);
	const double sc = std::clamp(m_scIn.value_or(15.0), 0.0, 100.0);
	return std::format("decimate=cycle={}:dupthresh={}:scthresh={}",
		cycle, dup, sc);
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

	const std::string chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("decimate: AVFilterGraph::Open failed");
			return;
		}
		m_graph = Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("decimate: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("decimate: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
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
	Save(std::move(out));
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
		Save(std::move(out));
	}
	m_graph.reset();
}
