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
#include <StormByte/multimedia/pipeline/filters/audio/afftdn.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Afftdn;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Afftdn::Afftdn(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> nr, StormByte::Safe::Optional<double> nf,
	StormByte::Safe::Optional<bool> trackNoise) noexcept
	: Filter::Process(std::move(log), "afftdn"),
	m_nrIn(nr), m_nfIn(nf), m_trackIn(trackNoise) {}

Afftdn::~Afftdn() noexcept = default;

enum Type Afftdn::Media() const noexcept {
	return Type::Audio;
}

void Afftdn::Clean() noexcept {
	m_graph.reset();
}

void Afftdn::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Afftdn::Chain() const noexcept {
	const double nr = std::clamp(m_nrIn.value_or(12.0), 0.01, 97.0);
	const double nf = std::clamp(m_nfIn.value_or(-50.0), -80.0, -20.0);
	const int tn = m_trackIn.value_or(false) ? 1 : 0;
	return StormByte::Safe::String(std::format("afftdn=nr={}:nf={}:tn={}:nt=w:om=o", nr, nf, tn));
}

void Afftdn::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.SampleRate() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "afftdn: frame has no samples");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("afftdn: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("afftdn: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("afftdn: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"afftdn wait ch={} rate={} pts={}",
			src.Channels(), src.SampleRate(), src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"afftdn ch={} rate={} samples={} pts={}",
		out.Channels(), out.SampleRate(), out.NbSamples(), out.Pts()));
	Save(std::move(out));
}

void Afftdn::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("afftdn: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"afftdn flush ch={} samples={} pts={}",
			out.Channels(), out.NbSamples(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
