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
#include <StormByte/multimedia/pipeline/filters/video/bm3d.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Bm3d;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Bm3d::Bm3d(Safe::Shared<StormByte::Logger::Log> log,
	Safe::Optional<double> sigma, Safe::Optional<unsigned> group,
	Safe::Optional<unsigned> range, Safe::Optional<unsigned> bstep) noexcept
	: Filter::Process(std::move(log), Safe::String("bm3d")),
	m_sigmaIn(sigma), m_groupIn(group), m_rangeIn(range), m_bstepIn(bstep),
	m_sigma(0.0), m_group(0), m_range(0), m_bstep(0), m_latched(false) {}

Bm3d::~Bm3d() noexcept = default;

enum Type Bm3d::Media() const noexcept {
	return Type::Video;
}

void Bm3d::Clean() noexcept {
	m_latched = false;
	m_graph.reset();
}

void Bm3d::Setup() noexcept {
	Clean();
}

void Bm3d::Latch(int width, int height) noexcept {
	const bool uhd = width >= 3840 || height >= 2160;
	m_sigma = m_sigmaIn.value_or(uhd ? 2.0 : 3.0);
	m_group = m_groupIn.value_or(uhd ? 8u : 16u);
	m_range = m_rangeIn.value_or(9u);
	m_bstep = m_bstepIn.value_or(4u);
	m_latched = true;
	Log(Level::Debug, std::format(
		"bm3d latch {}x{} sigma={:.3f} group={} range={} bstep={} uhd={}",
		width, height, m_sigma, m_group, m_range, m_bstep, uhd));
}

StormByte::Safe::String Bm3d::Chain() const noexcept {
	return StormByte::Safe::String{std::format(
		"bm3d=sigma={}:block=4:bstep={}:group={}:range={}:estim=basic",
		m_sigma, m_bstep, m_group, m_range)};
}

void Bm3d::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "bm3d: frame has no picture");
		return;
	}

	if (!m_latched)
		Latch(src.Width(), src.Height());

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("bm3d: AVFilterGraph::Open failed");
			return;
		}
		m_graph = Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("bm3d: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out) || !out) {
		Fail("bm3d: AVFilterGraph::Filter failed");
		return;
	}
	if (!out.CopyProps(src)) {
		Fail("bm3d: CopyProps failed");
		return;
	}

	Log(Level::LowLevel, std::format(
		"bm3d {}x{} sigma={:.3f} group={} range={} bstep={} pts={}",
		src.Width(), src.Height(), m_sigma, m_group, m_range, m_bstep, src.Pts()));
	Save(std::move(out));
}
