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
#include <StormByte/multimedia/pipeline/filters/video/pad.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Pad;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Pad::Pad(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	std::uint32_t width, std::uint32_t height) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("pad")),
	m_width(width), m_height(height) {}

Pad::~Pad() noexcept = default;

enum Type Pad::Media() const noexcept {
	return Type::Video;
}

void Pad::Clean() noexcept {
	m_graph.reset();
}

void Pad::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Pad::Chain() const noexcept {
	return StormByte::Safe::String(std::format(
		"pad=width={}:height={}:x=(ow-iw)/2:y=(oh-ih)/2:color=black",
		m_width, m_height));
}

void Pad::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "pad: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("pad: hardware frame; decode to software first");
		return;
	}
	if (m_width == 0 || m_height == 0) {
		Fail("pad: destination size must be positive");
		return;
	}
	if (static_cast<std::uint32_t>(src.Width()) > m_width
		|| static_cast<std::uint32_t>(src.Height()) > m_height) {
		Fail("pad: destination smaller than source; use Crop or Scale");
		return;
	}
	if (static_cast<std::uint32_t>(src.Width()) == m_width
		&& static_cast<std::uint32_t>(src.Height()) == m_height) {
		Log(Level::LowLevel, std::format(
			"pad no-op {}x{} pts={}", src.Width(), src.Height(), src.Pts()));
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("pad: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::Heap::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("pad: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("pad: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"pad wait {}x{} pts={}", src.Width(), src.Height(), src.Pts()));
		return;
	}

	out.ColorRange(src.ColorRange());
	out.ColorSpace(src.ColorSpace());
	out.ColorPrimaries(src.ColorPrimaries());
	out.ColorTransfer(src.ColorTransfer());
	out.ChromaLocation(src.ChromaLocation());
	out.SampleAspectRatio(src.SampleAspectRatio());

	Log(Level::LowLevel, std::format(
		"pad {}x{} -> {}x{} pts={}",
		src.Width(), src.Height(), out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Pad::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("pad: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"pad flush {}x{} pts={}", out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
