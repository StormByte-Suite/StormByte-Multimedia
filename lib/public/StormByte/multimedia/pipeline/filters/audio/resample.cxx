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
#include <StormByte/multimedia/pipeline/filters/audio/resample.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Resample;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Resample::Resample(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<int> rate) noexcept
	: Filter::Process(std::move(log), "resample"),
	m_rateIn(rate) {}

Resample::~Resample() noexcept = default;

enum Type Resample::Media() const noexcept {
	return Type::Audio;
}

void Resample::Clean() noexcept {
	m_graph.reset();
}

void Resample::Setup() noexcept {
	Clean();
}

int Resample::Rate() const noexcept {
	return m_rateIn.value_or(48000);
}

StormByte::Safe::String Resample::Chain() const noexcept {
	return StormByte::Safe::String(std::format("aresample=osr={}", Rate()));
}

void Resample::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.SampleRate() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "resample: frame has no samples");
		return;
	}

	const int want = Rate();
	if (want <= 0) {
		Fail("resample: output rate must be positive");
		return;
	}
	if (src.SampleRate() == want) {
		Log(Level::LowLevel, std::format(
			"resample no-op {} Hz pts={}", want, src.Pts()));
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("resample: AVFilterGraph::Open failed");
			return;
		}
		m_graph = std::make_unique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("resample: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("resample: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"resample wait {}->{} Hz pts={}",
			src.SampleRate(), want, src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"resample {}->{} Hz samples={} pts={}",
		src.SampleRate(), out.SampleRate(), out.NbSamples(), out.Pts()));
	Save(std::move(out));
}

void Resample::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("resample: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"resample flush {} Hz samples={} pts={}",
			out.SampleRate(), out.NbSamples(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
