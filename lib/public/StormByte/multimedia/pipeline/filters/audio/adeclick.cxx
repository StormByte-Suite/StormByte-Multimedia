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
#include <StormByte/multimedia/pipeline/filters/audio/adeclick.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Adeclick;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Adeclick::Adeclick(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> threshold) noexcept
:	Filter::Process(std::move(log), "adeclick"),
	m_thrIn(threshold) {}

Adeclick::~Adeclick() noexcept = default;

enum Type Adeclick::Media() const noexcept {
	return Type::Audio;
}

void Adeclick::Clean() noexcept {
	m_graph.reset();
}

void Adeclick::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Adeclick::Chain() const noexcept {
	if (!m_thrIn)
		return StormByte::Safe::String("adeclick");
	const double t = std::clamp(*m_thrIn, 1.0, 100.0);
	return StormByte::Safe::String(std::format("adeclick=threshold={}", t));
}

void Adeclick::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.SampleRate() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "adeclick: frame has no samples");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("adeclick: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("adeclick: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("adeclick: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"adeclick wait ch={} rate={} pts={}",
			src.Channels(), src.SampleRate(), src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"adeclick ch={} rate={} samples={} pts={}",
		out.Channels(), out.SampleRate(), out.NbSamples(), out.Pts()));
	Save(std::move(out));
}

void Adeclick::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("adeclick: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"adeclick flush ch={} samples={} pts={}",
			out.Channels(), out.NbSamples(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
