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
#include <StormByte/multimedia/pipeline/filters/video/vaguedenoiser.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::VagueDenoiser;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

VagueDenoiser::VagueDenoiser(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> threshold,
	StormByte::Safe::Optional<unsigned> steps,
	StormByte::Safe::Optional<double> percent) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("vaguedenoiser")),
	m_thrIn(std::move(threshold)), m_stepsIn(std::move(steps)), m_pctIn(std::move(percent)) {}

VagueDenoiser::~VagueDenoiser() noexcept = default;

enum Type VagueDenoiser::Media() const noexcept {
	return Type::Video;
}

void VagueDenoiser::Clean() noexcept {
	m_graph.reset();
}

void VagueDenoiser::Setup() noexcept {
	Clean();
}

StormByte::Safe::String VagueDenoiser::Chain() const noexcept {
	const double thr = m_thrIn.value_or(2.0);
	const unsigned steps = m_stepsIn.value_or(6u);
	const double pct = m_pctIn.value_or(85.0);
	return StormByte::Safe::String(std::format(
		"vaguedenoiser=threshold={}:nsteps={}:percent={}:method=garrote",
		thr, steps, pct));
}

void VagueDenoiser::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "vaguedenoiser: frame has no picture");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("vaguedenoiser: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("vaguedenoiser: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("vaguedenoiser: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"vaguedenoiser wait {}x{} pts={}",
			src.Width(), src.Height(), src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"vaguedenoiser {}x{} pts={}",
		out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}
