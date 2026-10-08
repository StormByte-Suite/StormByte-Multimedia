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
#include <StormByte/multimedia/pipeline/filters/video/deflicker.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Deflicker;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

namespace {
	bool ModeOk(const std::string& mode) noexcept {
		return mode == "am" || mode == "gm" || mode == "hm"
			|| mode == "qm" || mode == "cm" || mode == "pm"
			|| mode == "median";
	}
}

Deflicker::Deflicker(Safe::Shared<StormByte::Logger::Log> log,
	Safe::Optional<unsigned> size,
	Safe::Optional<Safe::String> mode) noexcept
	: Filter::Process(std::move(log), Safe::String("deflicker")),
	m_sizeIn(size), m_modeIn(std::move(mode)) {}

Deflicker::~Deflicker() noexcept = default;

enum Type Deflicker::Media() const noexcept {
	return Type::Video;
}

void Deflicker::Clean() noexcept {
	m_graph.reset();
}

void Deflicker::Setup() noexcept {
	Clean();
}

std::string Deflicker::Chain() const noexcept {
	unsigned size = m_sizeIn.value_or(5u);
	size = std::clamp(size, 2u, 129u);
	std::string mode = std::string(m_modeIn.value_or(Safe::String("am")));
	if (!ModeOk(mode))
		mode = "am";
	return std::format("deflicker=size={}:mode={}", size, mode);
}

void Deflicker::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "deflicker: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("deflicker: hardware frame; decode to software first");
		return;
	}

	const std::string chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("deflicker: AVFilterGraph::Open failed");
			return;
		}
		m_graph = Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("deflicker: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("deflicker: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"deflicker wait {}x{} pts={}",
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
		"deflicker {}x{} pts={}",
		out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Deflicker::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("deflicker: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"deflicker flush {}x{} pts={}",
			out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
