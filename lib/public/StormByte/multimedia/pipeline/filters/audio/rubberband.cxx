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
#include <StormByte/multimedia/pipeline/filters/audio/rubberband.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Rubberband;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Rubberband::Rubberband(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> tempo, StormByte::Safe::Optional<double> pitch) noexcept
	: Filter::Process(std::move(log), "rubberband"),
	m_tempoIn(tempo), m_pitchIn(pitch) {}

Rubberband::~Rubberband() noexcept = default;

enum Type Rubberband::Media() const noexcept {
	return Type::Audio;
}

void Rubberband::Clean() noexcept {
	m_graph.reset();
}

void Rubberband::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Rubberband::Chain() const noexcept {
	const double tempo = m_tempoIn.value_or(1.0);
	const double pitch = m_pitchIn.value_or(1.0);
	return StormByte::Safe::String(std::format("rubberband=tempo={}:pitch={}", tempo, pitch));
}

void Rubberband::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.SampleRate() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "rubberband: frame has no audio");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("rubberband: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("rubberband: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("rubberband: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"rubberband wait ch={} rate={} pts={}",
			src.Channels(), src.SampleRate(), src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"rubberband ch={} rate={} samples={} pts={}",
		out.Channels(), out.SampleRate(), out.NbSamples(), out.Pts()));
	Save(std::move(out));
}
