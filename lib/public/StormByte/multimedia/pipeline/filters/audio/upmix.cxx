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

#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/upmix.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Upmix;
using StormByte::Multimedia::Property::ChannelCount;
using StormByte::Multimedia::Property::ChannelLayout;
using StormByte::Multimedia::Property::ToString;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Upmix::Upmix(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	ChannelLayout target) noexcept
	: Filter::Process(std::move(log), "upmix"),
	m_target(target) {}

Upmix::~Upmix() noexcept = default;

enum Type Upmix::Media() const noexcept {
	return Type::Audio;
}

void Upmix::Clean() noexcept {
	m_graph.reset();
	m_mode = Mode::None;
}

void Upmix::Setup() noexcept {
	Clean();
}

std::string_view Upmix::LayoutName() const noexcept {
	switch (m_target) {
		case ChannelLayout::Mono:				return "mono";
		case ChannelLayout::Stereo:				return "stereo";
		case ChannelLayout::TwoPointOne:		return "2.1";
		case ChannelLayout::ThreePointZero:		return "3.0";
		case ChannelLayout::FourPointZero:		return "4.0";
		case ChannelLayout::Quad:				return "quad";
		case ChannelLayout::FivePointZero:		return "5.0";
		case ChannelLayout::FivePointOne:		return "5.1";
		case ChannelLayout::SixPointOne:		return "6.1";
		case ChannelLayout::SevenPointOne:		return "7.1";
		case ChannelLayout::SevenPointOneW:		return "7.1(wide)";
		case ChannelLayout::Octagonal:			return "octagonal";
		case ChannelLayout::TwentyTwoPointTwo:	return "22.2";
		default:								return {};
	}
}

bool Upmix::TargetHasLfe() const noexcept {
	switch (m_target) {
		case ChannelLayout::TwoPointOne:
		case ChannelLayout::FivePointOne:
		case ChannelLayout::SixPointOne:
		case ChannelLayout::SevenPointOne:
		case ChannelLayout::SevenPointOneW:
		case ChannelLayout::TwentyTwoPointTwo:
			return true;
		default:
			return false;
	}
}

StormByte::Safe::String Upmix::SurroundChain(const FFrame& src) const noexcept {
	const auto out = LayoutName();
	if (out.empty())
		return {};
	const auto inLayout = src.ChannelLayout();
	const StormByte::Safe::String in = inLayout ? inLayout.Describe() : StormByte::Safe::String{};
	const int lfe = TargetHasLfe() ? 1 : 0;
	if (!in.empty())
		return StormByte::Safe::String(std::format(
			"surround=chl_in={}:chl_out={}:lfe={}:lfe_mode=add:lfe_low=128:lfe_high=256",
			in, out, lfe));
	return StormByte::Safe::String(std::format(
		"surround=chl_out={}:lfe={}:lfe_mode=add:lfe_low=128:lfe_high=256",
		out, lfe));
}

StormByte::Safe::String Upmix::AformatChain() const noexcept {
	const auto name = LayoutName();
	if (name.empty())
		return {};
	return StormByte::Safe::String(std::format("aformat=channel_layouts={}", name));
}

StormByte::Safe::String Upmix::PanChain() const noexcept {
	const auto name = LayoutName();
	if (name.empty())
		return {};
	if (TargetHasLfe())
		return StormByte::Safe::String(std::format(
			"pan={}|FL=FL|FR=FR|FC=0.707*FL+0.707*FR|LFE=0.5*FL+0.5*FR|BL=FL|BR=FR|SL=FL|SR=FR",
			name));
	return StormByte::Safe::String(std::format(
		"pan={}|FL=FL|FR=FR|FC=0.707*FL+0.707*FR|BL=FL|BR=FR|SL=FL|SR=FR",
		name));
}

bool Upmix::EnsureGraph(const FFrame& src, std::string_view chain) noexcept {
	if (chain.empty())
		return false;
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened)
			return false;
		m_graph = std::make_unique<FGraph>(std::move(opened));
		return true;
	}
	return m_graph->Ensure(src, chain);
}

void Upmix::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.SampleRate() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "upmix: frame has no samples");
		return;
	}

	const unsigned want = ChannelCount(m_target);
	const int have = src.Channels();
	if (want == 0) {
		Fail("upmix: unknown target layout");
		return;
	}
	if (have == static_cast<int>(want)) {
		Log(Level::LowLevel, std::format(
			"upmix no-op {}ch pts={}", have, src.Pts()));
		return;
	}
	if (have > static_cast<int>(want)) {
		Fail(std::format(
			"upmix: {} channels cannot shrink to {}; use Downmix",
			have, ToString(m_target)));
		return;
	}

	if (m_mode == Mode::None || m_mode == Mode::Surround) {
		const StormByte::Safe::String surround = SurroundChain(src);
		if (EnsureGraph(src, surround)) {
			m_mode = Mode::Surround;
		} else {
			Log(Level::Warning,
				"upmix: surround failed; trying aformat");
			m_graph.reset();
			m_mode = Mode::Aformat;
		}
	}
	if (m_mode == Mode::Aformat) {
		if (EnsureGraph(src, AformatChain())) {
			/* keep */
		} else {
			Log(Level::Warning,
				"upmix: aformat failed; pan with summed LFE");
			m_graph.reset();
			m_mode = Mode::Pan;
		}
	}
	if (m_mode == Mode::Pan) {
		if (!EnsureGraph(src, PanChain())) {
			Fail("upmix: AVFilterGraph::Open failed");
			return;
		}
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("upmix: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"upmix wait {}ch->{} pts={}",
			have, ToString(m_target), src.Pts()));
		return;
	}

	const char* how = "surround";
	if (m_mode == Mode::Aformat)
		how = "aformat";
	else if (m_mode == Mode::Pan)
		how = "pan";
	Log(Level::LowLevel, std::format(
		"upmix {}ch->{} ({}) pts={}",
		have, ToString(m_target), how, out.Pts()));
	Save(std::move(out));
}

void Upmix::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("upmix: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Log(Level::LowLevel, std::format(
			"upmix flush {}ch pts={}",
			out.Channels(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
