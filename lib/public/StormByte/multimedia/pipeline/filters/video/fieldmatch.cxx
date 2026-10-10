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
#include <StormByte/multimedia/pipeline/filters/video/fieldmatch.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Fieldmatch;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

namespace {
	bool OrderOk(const std::string& v) noexcept {
		return v == "auto" || v == "tff" || v == "bff";
	}

	bool ModeOk(const std::string& v) noexcept {
		return v == "pc" || v == "pc_n" || v == "pc_u"
			|| v == "pc_n_ub" || v == "pcn" || v == "pcn_ub";
	}

	bool CombOk(const std::string& v) noexcept {
		return v == "none" || v == "sc" || v == "full";
	}
}

Fieldmatch::Fieldmatch(Safe::Shared<StormByte::Logger::Log> log,
	Safe::Optional<Safe::String> order,
	Safe::Optional<Safe::String> mode,
	Safe::Optional<Safe::String> combmatch) noexcept
	: Filter::Process(std::move(log), Safe::String("fieldmatch")),
	m_orderIn(std::move(order)),
	m_modeIn(std::move(mode)),
	m_combIn(std::move(combmatch)) {}

Fieldmatch::~Fieldmatch() noexcept = default;

enum Type Fieldmatch::Media() const noexcept {
	return Type::Video;
}

void Fieldmatch::Clean() noexcept {
	m_graph.reset();
}

void Fieldmatch::Setup() noexcept {
	Clean();
}

StormByte::Safe::String Fieldmatch::Chain() const noexcept {
	std::string order = std::string(m_orderIn.value_or(Safe::String("auto")));
	if (!OrderOk(order))
		order = "auto";
	std::string mode = std::string(m_modeIn.value_or(Safe::String("pc_n")));
	if (!ModeOk(mode))
		mode = "pc_n";
	std::string comb = std::string(m_combIn.value_or(Safe::String("sc")));
	if (!CombOk(comb))
		comb = "sc";
	return StormByte::Safe::String{std::format("fieldmatch=order={}:mode={}:combmatch={}",
		order, mode, comb)};
}

void Fieldmatch::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "fieldmatch: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("fieldmatch: hardware frame; decode to software first");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("fieldmatch: AVFilterGraph::Open failed");
			return;
		}
		m_graph = Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("fieldmatch: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("fieldmatch: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"fieldmatch wait {}x{} pts={}",
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
		"fieldmatch {}x{} pts={}",
		out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Fieldmatch::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("fieldmatch: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"fieldmatch flush {}x{} pts={}",
			out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
