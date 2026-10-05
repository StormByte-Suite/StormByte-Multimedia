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
#include <StormByte/multimedia/pipeline/filters/video/tonemap.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Tonemap;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

namespace {
	// libavutil AVColorTransferCharacteristic
	constexpr int TrcPq  = 16;	// AVCOL_TRC_SMPTE2084
	constexpr int TrcHlg = 18;	// AVCOL_TRC_ARIB_STD_B67
	constexpr int TrcBt709 = 1;	// AVCOL_TRC_BT709
	constexpr int PriBt709 = 1;	// AVCOL_PRI_BT709
	constexpr int SpcBt709 = 1;	// AVCOL_SPC_BT709
	constexpr int RangeTv  = 1;	// AVCOL_RANGE_MPEG
}

Tonemap::Tonemap(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<StormByte::Safe::String> op) noexcept
	: Filter::Process(std::move(log), StormByte::Safe::String("tonemap")),
	m_opIn(std::move(op)) {}

Tonemap::~Tonemap() noexcept = default;

enum Type Tonemap::Media() const noexcept {
	return Type::Video;
}

void Tonemap::Clean() noexcept {
	m_graph.reset();
}

void Tonemap::Setup() noexcept {
	Clean();
}

bool Tonemap::NeedsMap(const FFrame& src) noexcept {
	const int trc = src.ColorTransfer();
	return trc == TrcPq || trc == TrcHlg;
}

StormByte::Safe::String Tonemap::Operator() const noexcept {
	if (!m_opIn || m_opIn->empty())
		return StormByte::Safe::String("hable");
	const StormByte::Safe::String op = *m_opIn;
	if (op == "hable" || op == "mobius" || op == "reinhard"
		|| op == "gamma" || op == "clip" || op == "linear")
		return op;
	return StormByte::Safe::String("hable");
}

StormByte::Safe::String Tonemap::Chain(const FFrame& src) const noexcept {
	const char* tin = src.ColorTransfer() == TrcHlg ? ":tin=arib-std-b67" : "";
	return StormByte::Safe::String(std::format(
		"zscale=t=linear:npl=100{},format=gbrpf32le,"
		"zscale=p=bt709,tonemap=tonemap={}:desat=0,"
		"zscale=t=bt709:m=bt709:r=tv,format=yuv420p",
		tin, static_cast<std::string_view>(Operator())));
}

void Tonemap::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.Width() <= 0 || src.Height() <= 0) {
		Log(Level::Warning, "tonemap: frame has no picture");
		return;
	}
	if (src.Hardware()) {
		Fail("tonemap: hardware frame; decode to software first");
		return;
	}
	if (!NeedsMap(src)) {
		Log(Level::LowLevel, std::format(
			"tonemap no-op trc={} pts={}", src.ColorTransfer(), src.Pts()));
		return;
	}

	const StormByte::Safe::String chain = Chain(src);
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("tonemap: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::Heap::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("tonemap: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("tonemap: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"tonemap wait {}x{} pts={}", src.Width(), src.Height(), src.Pts()));
		return;
	}

	out.ColorRange(RangeTv);
	out.ColorSpace(SpcBt709);
	out.ColorPrimaries(PriBt709);
	out.ColorTransfer(TrcBt709);
	out.ChromaLocation(src.ChromaLocation());
	out.SampleAspectRatio(src.SampleAspectRatio());

	Log(Level::LowLevel, std::format(
		"tonemap {} {}x{} pts={}",
		static_cast<std::string_view>(Operator()), out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Tonemap::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("tonemap: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		out.ColorRange(RangeTv);
		out.ColorSpace(SpcBt709);
		out.ColorPrimaries(PriBt709);
		out.ColorTransfer(TrcBt709);
		Log(Level::LowLevel, std::format(
			"tonemap flush {}x{} pts={}", out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
