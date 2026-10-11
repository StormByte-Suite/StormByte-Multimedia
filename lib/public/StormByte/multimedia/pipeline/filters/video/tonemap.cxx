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

#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/tonemap.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

extern "C" {
	#include <libavutil/pixfmt.h>
}

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Video::Tonemap;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

namespace {
	constexpr int TrcPq = AVCOL_TRC_SMPTE2084;
	constexpr int TrcHlg = AVCOL_TRC_ARIB_STD_B67;

	FFrame::ColorTarget LinearTarget() noexcept {
		FFrame::ColorTarget color;
		color.transfer = AVCOL_TRC_LINEAR;
		color.primaries = AVCOL_PRI_BT709;
		color.matrix = AVCOL_SPC_RGB;
		color.range = AVCOL_RANGE_JPEG;
		color.peak = 100.0;
		return color;
	}

	FFrame::ColorTarget SdrTarget() noexcept {
		FFrame::ColorTarget color;
		color.transfer = AVCOL_TRC_BT709;
		color.primaries = AVCOL_PRI_BT709;
		color.matrix = AVCOL_SPC_BT709;
		color.range = AVCOL_RANGE_MPEG;
		color.peak = 0;
		return color;
	}

	bool HasPicture(const FFrame& frame) noexcept {
		return frame.Width() > 0 && frame.Height() > 0;
	}
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

StormByte::Safe::String Tonemap::Chain() const noexcept {
	return StormByte::Safe::String(std::format(
		"tonemap=tonemap={}:desat=0", static_cast<std::string_view>(Operator())));
}

void Tonemap::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Video)
		return;
	const FFrame& src = AVFrame();
	if (!src || !HasPicture(src)) {
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

	FFrame linear;
	linear.Format(AV_PIX_FMT_GBRPF32LE);
	if (!src.ScaleTo(ScalingContext(), linear, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Zimg, LinearTarget())) {
		Fail("tonemap: linear conversion failed");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(linear, chain);
		if (!opened) {
			Fail("tonemap: curve filter is unavailable");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(linear, chain)) {
		Fail("tonemap: curve filter reset failed");
		return;
	}

	FFrame curved;
	if (!m_graph->Filter(linear, curved)) {
		Fail("tonemap: curve filter failed");
		return;
	}
	if (!HasPicture(curved)) {
		Log(Level::LowLevel, std::format(
			"tonemap wait {}x{} pts={}", src.Width(), src.Height(), src.Pts()));
		return;
	}
	curved.ColorTransfer(AVCOL_TRC_LINEAR);
	curved.ColorPrimaries(AVCOL_PRI_BT709);
	curved.ColorSpace(AVCOL_SPC_RGB);
	curved.ColorRange(AVCOL_RANGE_JPEG);

	FFrame out;
	out.Format(AV_PIX_FMT_YUV420P);
	if (!curved.ScaleTo(ScalingContext(), out, src.Width(), src.Height(),
			FFrame::Resample::Default, FFrame::Scaler::Zimg, SdrTarget())) {
		Fail("tonemap: BT.709 conversion failed");
		return;
	}
	out.ChromaLocation(src.ChromaLocation());
	out.SampleAspectRatio(src.SampleAspectRatio());
	out.Pts(src.Pts());
	out.DurationTicks(src.DurationTicks());

	Log(Level::LowLevel, std::format(
		"tonemap {} {}x{} pts={}",
		static_cast<std::string_view>(Operator()), out.Width(), out.Height(), out.Pts()));
	Save(std::move(out));
}

void Tonemap::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame curved;
		if (!m_graph->Flush(curved)) {
			Fail("tonemap: curve flush failed");
			break;
		}
		if (!HasPicture(curved))
			break;
		curved.ColorTransfer(AVCOL_TRC_LINEAR);
		curved.ColorPrimaries(AVCOL_PRI_BT709);
		curved.ColorSpace(AVCOL_SPC_RGB);
		curved.ColorRange(AVCOL_RANGE_JPEG);
		FFrame out;
		out.Format(AV_PIX_FMT_YUV420P);
		if (!curved.ScaleTo(ScalingContext(), out, curved.Width(), curved.Height(),
				FFrame::Resample::Default, FFrame::Scaler::Zimg, SdrTarget())) {
			Fail("tonemap: BT.709 conversion failed");
			break;
		}
		out.Pts(curved.Pts());
		out.DurationTicks(curved.DurationTicks());
		Log(Level::LowLevel, std::format(
			"tonemap flush {}x{} pts={}", out.Width(), out.Height(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}