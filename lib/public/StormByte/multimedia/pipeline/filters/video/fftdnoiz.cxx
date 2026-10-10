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
#include <StormByte/multimedia/pipeline/filters/video/fftdnoiz.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Fftdnoiz;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Fftdnoiz::Fftdnoiz(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> sigma,
	StormByte::Safe::Optional<unsigned> prev,
	StormByte::Safe::Optional<unsigned> next) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("fftdnoiz")),
	m_sigmaIn(std::move(sigma)), m_prevIn(std::move(prev)), m_nextIn(std::move(next)) {}

Fftdnoiz::~Fftdnoiz() noexcept = default;

enum Type Fftdnoiz::Media() const noexcept {
	return Type::Video;
}

void Fftdnoiz::Clean() noexcept {
	m_graph.reset();
}

void Fftdnoiz::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Fftdnoiz::Chain() const noexcept {
	const double sigma = std::clamp(m_sigmaIn.value_or(1.0), 0.0, 100.0);
	const unsigned prev = std::min(m_prevIn.value_or(1u), 1u);
	const unsigned next = std::min(m_nextIn.value_or(1u), 1u);
	return StormByte::Safe::String(std::format(
		"fftdnoiz=sigma={}:prev={}:next={}:block=32:overlap=0.5",
		sigma, prev, next));
}

void Fftdnoiz::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "fftdnoiz: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("fftdnoiz: hardware frame; decode to software first");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("fftdnoiz: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("fftdnoiz: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("fftdnoiz: AVFilterGraph::Filter failed");
		return;
	}
	if (out.Width() <= 0 || out.Height() <= 0 || out.Format() == FFrame::FormatNone()) {
		Log(Level::LowLevel, std::format(
			"fftdnoiz wait {}x{} pts={}",
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
		"fftdnoiz {}x{} pts={}",
		out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Fftdnoiz::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("fftdnoiz: AVFilterGraph::Flush failed");
			break;
		}
		if (out.Width() <= 0 || out.Height() <= 0 || out.Format() == FFrame::FormatNone())
			break;
		Log(Level::LowLevel, std::format(
			"fftdnoiz flush {}x{} pts={}",
			out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
