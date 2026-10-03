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
#include <StormByte/multimedia/pipeline/filters/audio/downmix.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <utility>

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Downmix;
using StormByte::Multimedia::Property::ChannelCount;
using StormByte::Multimedia::Property::ChannelLayout;
using StormByte::Multimedia::Property::ToString;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

Downmix::Downmix(std::shared_ptr<StormByte::Logger::Log> log,
	ChannelLayout target) noexcept
	: Filter::Process(std::move(log), "downmix"),
	m_target(target) {}

enum Type Downmix::Media() const noexcept {
	return Type::Audio;
}

void Downmix::Clean() noexcept {
	m_graph.reset();
	m_pan = false;
}

void Downmix::Setup() noexcept {
	Clean();
}

std::string_view Downmix::LayoutName() const noexcept {
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

std::string Downmix::AformatChain() const noexcept {
	const auto name = LayoutName();
	if (name.empty())
		return {};
	return std::format("aformat=channel_layouts={}", name);
}

std::string Downmix::PanChain() const noexcept {
	const auto name = LayoutName();
	if (name.empty())
		return {};
	return std::format("pan={}|FL=FL+0.707*LFE|FR=FR+0.707*LFE", name);
}

bool Downmix::EnsureGraph(const FFrame& src, std::string_view chain) noexcept {
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

void Downmix::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.SampleRate() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "downmix: frame has no samples");
		return;
	}

	const unsigned want = ChannelCount(m_target);
	const int have = src.Channels();
	if (want == 0) {
		Fail("downmix: unknown target layout");
		return;
	}
	if (have == static_cast<int>(want)) {
		Log(Level::LowLevel, std::format(
			"downmix no-op {}ch pts={}", have, src.Pts()));
		return;
	}
	if (have < static_cast<int>(want)) {
		Fail(std::format(
			"downmix: {} channels cannot reach {}",
			have, ToString(m_target)));
		return;
	}

	const std::string aformat = AformatChain();
	if (aformat.empty()) {
		Fail("downmix: unknown target layout");
		return;
	}

	if (!m_pan) {
		if (!EnsureGraph(src, aformat)) {
			Log(Level::Warning,
				"downmix: aformat failed; folding LFE into FL/FR");
			m_graph.reset();
			m_pan = true;
		}
	}
	if (m_pan) {
		const std::string pan = PanChain();
		if (!EnsureGraph(src, pan)) {
			Fail("downmix: AVFilterGraph::Open failed");
			return;
		}
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("downmix: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"downmix wait {}ch->{} pts={}",
			have, ToString(m_target), src.Pts()));
		return;
	}

	Log(Level::LowLevel, std::format(
		"downmix {}ch->{} ({}) pts={}",
		have, ToString(m_target),
		m_pan ? "pan" : "aformat", out.Pts()));
	Save(std::move(out));
}

void Downmix::Eof() noexcept {
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("downmix: AVFilterGraph::Flush failed");
			break;
		}
		if (out.NbSamples() <= 0)
			break;
		Log(Level::LowLevel, std::format(
			"downmix flush {}ch pts={}",
			out.Channels(), out.Pts()));
		Save(std::move(out));
	}
	m_graph.reset();
}
