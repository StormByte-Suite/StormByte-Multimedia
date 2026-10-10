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
#include <StormByte/multimedia/pipeline/filters/video/atadenoise.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Atadenoise;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Atadenoise::Atadenoise(Safe::Shared<StormByte::Logger::Log> log,
	Safe::Optional<unsigned> frames,
	Safe::Optional<double> lumaA,
	Safe::Optional<double> lumaB,
	Safe::Optional<double> chromaA,
	Safe::Optional<double> chromaB) noexcept
	: Filter::Process(std::move(log), Safe::String("atadenoise")),
	m_framesIn(frames), m_laIn(lumaA), m_lbIn(lumaB),
	m_caIn(chromaA), m_cbIn(chromaB) {}

Atadenoise::~Atadenoise() noexcept = default;

enum Type Atadenoise::Media() const noexcept {
	return Type::Video;
}

void Atadenoise::Clean() noexcept {
	m_graph.reset();
}

void Atadenoise::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Atadenoise::Chain() const noexcept {
	const unsigned s = m_framesIn.value_or(9u);
	const double la = m_laIn.value_or(0.02);
	const double lb = m_lbIn.value_or(0.04);
	const double ca = m_caIn.value_or(0.02);
	const double cb = m_cbIn.value_or(0.04);
	return Safe::String(std::format(
		"atadenoise=s={}:0a={}:0b={}:1a={}:1b={}:2a={}:2b={}",
		s, la, lb, ca, cb, ca, cb));
}

void Atadenoise::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "atadenoise: frame has no picture");
		return;
	}

	const Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("atadenoise: AVFilterGraph::Open failed");
			return;
		}
		m_graph = Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("atadenoise: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("atadenoise: AVFilterGraph::Filter failed");
		return;
	}
	if (out.Width() <= 0 || out.Height() <= 0 || out.Format() == FFrame::FormatNone()) {
		Log(Level::LowLevel, std::format(
			"atadenoise wait {}x{} pts={}",
			src.Width(), src.Height(), src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"atadenoise {}x{} pts={}",
		out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Atadenoise::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("atadenoise: AVFilterGraph::Flush failed");
			break;
		}
		if (out.Width() <= 0 || out.Height() <= 0 || out.Format() == FFrame::FormatNone())
			break;
		Log(Level::LowLevel, std::format(
			"atadenoise flush {}x{} pts={}",
			out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
