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

#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>

#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

extern "C" {
	#include <libavfilter/avfilter.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
	#include <libavutil/opt.h>
	#include <libavutil/pixfmt.h>
	#include <libavutil/samplefmt.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVFilterGraph::AVFilterGraph(::AVFilterGraph* graph) noexcept
: AVPointer(graph) {}

FFmpeg::AVFilterGraph::AVFilterGraph(AVFilterGraph&& other) noexcept
: AVPointer(std::move(other)),
	m_src(other.m_src),
	m_sink(other.m_sink),
	m_w(other.m_w),
	m_h(other.m_h),
	m_fmt(other.m_fmt),
	m_rate(other.m_rate),
	m_ch(other.m_ch),
	m_mask(other.m_mask),
	m_graph(std::move(other.m_graph)),
	m_closed(other.m_closed),
	m_endPts(other.m_endPts),
	m_timeBase(other.m_timeBase),
	m_frameRate(other.m_frameRate) {
	other.m_src = nullptr;
	other.m_sink = nullptr;
	other.m_w = 0;
	other.m_h = 0;
	other.m_fmt = 0;
	other.m_rate = 0;
	other.m_ch = 0;
	other.m_mask = 0;
	other.m_closed = false;
}

FFmpeg::AVFilterGraph::~AVFilterGraph() noexcept {
	Free();
}

FFmpeg::AVFilterGraph& FFmpeg::AVFilterGraph::operator=(AVFilterGraph&& other) noexcept {
	if (this != &other) {
		Free();
		AVPointer::operator=(std::move(other));
		m_src = other.m_src;
		m_sink = other.m_sink;
		m_w = other.m_w;
		m_h = other.m_h;
		m_fmt = other.m_fmt;
		m_rate = other.m_rate;
		m_ch = other.m_ch;
		m_mask = other.m_mask;
		m_graph = std::move(other.m_graph);
		m_closed = other.m_closed;
		m_endPts = other.m_endPts;
		m_timeBase = other.m_timeBase;
		m_frameRate = other.m_frameRate;
		other.m_src = nullptr;
		other.m_sink = nullptr;
		other.m_w = 0;
		other.m_h = 0;
		other.m_fmt = 0;
		other.m_rate = 0;
		other.m_ch = 0;
		other.m_mask = 0;
		other.m_closed = false;
	}
	return *this;
}

FFmpeg::AVFilterGraph::operator bool() const noexcept {
	return m_ptr != nullptr && m_src != nullptr && m_sink != nullptr;
}

FFmpeg::AVFilterGraph FFmpeg::AVFilterGraph::Open(const AVFrame& src,
	std::string_view graph) noexcept {
	return Open(src, graph, FFmpeg::AVRational{0, 1});
}

FFmpeg::AVFilterGraph FFmpeg::AVFilterGraph::Open(const AVFrame& src,
	std::string_view graph, FFmpeg::AVRational frameRate) noexcept {
	if (!src || graph.empty())
		return AVFilterGraph(nullptr);
	if (src.Hardware())
		return AVFilterGraph(nullptr);

	const bool video = src.Width() > 0 && src.Height() > 0;
	const bool audio = src.SampleRate() > 0 && src.Channels() > 0;
	if (video == audio)
		return AVFilterGraph(nullptr);

	const AVFilter* buffersrc = avfilter_get_by_name(video ? "buffer" : "abuffer");
	const AVFilter* buffersink = avfilter_get_by_name(video ? "buffersink" : "abuffersink");
	if (!buffersrc || !buffersink)
		return AVFilterGraph(nullptr);

	::AVFilterGraph* raw = avfilter_graph_alloc();
	if (!raw)
		return AVFilterGraph(nullptr);

	char args[512];
	const auto timeBase = src.TimeBase().Valid() ? src.TimeBase()
		: FFmpeg::AVRational{1, video ? 1 : src.SampleRate()};
	if (video) {
		const auto sar = src.SampleAspectRatio();
		int sarN = sar.Valid() ? sar.num : 1;
		int sarD = sar.Valid() ? sar.den : 1;
		if (sarN <= 0)
			sarN = 1;
		if (sarD <= 0)
			sarD = 1;
		std::snprintf(args, sizeof(args),
			"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			src.Width(), src.Height(), src.Format(), timeBase.num, timeBase.den, sarN, sarD);
		auto rate = frameRate;
		if (!rate.Valid() && src.DurationTicks() > 0 && src.DurationTicks() <= std::numeric_limits<int>::max()) {
			const auto duration = av_mul_q(::AVRational{timeBase.num, timeBase.den},
				::AVRational{static_cast<int>(src.DurationTicks()), 1});
			const auto inverse = av_inv_q(duration);
			rate = FFmpeg::AVRational{inverse.num, inverse.den};
		}
		if (rate.Valid()) {
			const auto used = std::char_traits<char>::length(args);
			std::snprintf(args + used, sizeof(args) - used, ":frame_rate=%d/%d", rate.num, rate.den);
		}
	} else {
		const char* fmt = av_get_sample_fmt_name(static_cast<AVSampleFormat>(src.Format()));
		if (!fmt) {
			avfilter_graph_free(&raw);
			return AVFilterGraph(nullptr);
		}
		char layout[128] = {};
		std::string described{static_cast<std::string_view>(src.ChannelLayout().Describe())};
		if (described.empty())
			described = static_cast<std::string_view>(
				FFmpeg::AVChannelLayout::Default(src.Channels()).Describe());
		if (described.empty())
			described = std::to_string(src.Channels());
		std::snprintf(layout, sizeof(layout), "%s", described.c_str());
		std::snprintf(args, sizeof(args),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
			timeBase.num, timeBase.den, src.SampleRate(), fmt, layout);
	}

	AVFilterContext* in = nullptr;
	AVFilterContext* out = nullptr;
	AVFilterInOut* inputs = nullptr;
	AVFilterInOut* outputs = nullptr;

	auto fail = [&]() {
		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);
		avfilter_graph_free(&raw);
		return AVFilterGraph(nullptr);
	};

	if (avfilter_graph_create_filter(&in, buffersrc, "in", args, nullptr, raw) < 0)
		return fail();
	if (avfilter_graph_create_filter(&out, buffersink, "out", nullptr, nullptr, raw) < 0)
		return fail();

	outputs = avfilter_inout_alloc();
	inputs = avfilter_inout_alloc();
	if (!outputs || !inputs)
		return fail();

	outputs->name = av_strdup("in");
	outputs->filter_ctx = in;
	outputs->pad_idx = 0;
	outputs->next = nullptr;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = out;
	inputs->pad_idx = 0;
	inputs->next = nullptr;

	const std::string chain(graph);
	if (avfilter_graph_parse_ptr(raw, chain.c_str(), &inputs, &outputs, nullptr) < 0)
		return fail();
	if (avfilter_graph_config(raw, nullptr) < 0)
		return fail();

	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	AVFilterGraph wrap(raw);
	wrap.m_src = in;
	wrap.m_sink = out;
	wrap.m_w = video ? src.Width() : 0;
	wrap.m_h = video ? src.Height() : 0;
	wrap.m_fmt = src.Format();
	wrap.m_rate = audio ? src.SampleRate() : 0;
	wrap.m_ch = audio ? src.Channels() : 0;
	wrap.m_mask = audio ? src.ChannelLayout().Mask() : 0;
	wrap.m_graph = chain;
	wrap.m_closed = false;
	wrap.m_timeBase = timeBase;
	wrap.m_frameRate = frameRate;
	return wrap;
}

bool FFmpeg::AVFilterGraph::Ensure(const AVFrame& src, std::string_view graph) noexcept {
	return Ensure(src, graph, FFmpeg::AVRational{0, 1});
}

bool FFmpeg::AVFilterGraph::Ensure(const AVFrame& src, std::string_view graph, FFmpeg::AVRational frameRate) noexcept {
	const bool video = src.Width() > 0 && src.Height() > 0;
	const auto timeBase = src.TimeBase().Valid() ? src.TimeBase()
		: FFmpeg::AVRational{1, video ? 1 : src.SampleRate()};
	const bool same = m_ptr && m_src && m_sink && !m_closed
		&& m_timeBase.num == timeBase.num && m_timeBase.den == timeBase.den
		&& m_frameRate.num == frameRate.num && m_frameRate.den == frameRate.den
		&& m_graph == StormByte::Safe::String{graph}
		&& m_fmt == src.Format()
		&& m_w == (video ? src.Width() : 0)
		&& m_h == (video ? src.Height() : 0)
		&& m_rate == (video ? 0 : src.SampleRate())
		&& m_ch == (video ? 0 : src.Channels())
		&& m_mask == (video ? 0ull : src.ChannelLayout().Mask());
	if (same)
		return true;
	AVFilterGraph next = Open(src, graph, frameRate);
	if (!next)
		return false;
	*this = std::move(next);
	return true;
}

FFmpeg::AVRational FFmpeg::AVFilterGraph::FrameRate() const noexcept {
	if (!m_sink)
		return FFmpeg::AVRational{0, 1};
	const auto rate = av_buffersink_get_frame_rate(m_sink);
	return FFmpeg::AVRational{rate.num, rate.den};
}

bool FFmpeg::AVFilterGraph::Filter(const AVFrame& src, AVFrame& dst) const noexcept {
	if (!m_ptr || !m_src || !m_sink || m_closed || !src.Get())
		return false;
	if (av_buffersrc_add_frame_flags(m_src, const_cast<::AVFrame*>(src.Get()),
			AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
		return false;
	if (src.Pts() != AV_NOPTS_VALUE)
		m_endPts = src.Pts() + (src.DurationTicks() > 0 ? src.DurationTicks() : 0);
	return Receive(dst);
}

bool FFmpeg::AVFilterGraph::Receive(AVFrame& dst) const noexcept {
	if (!m_ptr || !m_sink)
		return false;
	if (!dst.Get())
		dst.Reset(av_frame_alloc());
	if (!dst.Get())
		return false;
	av_frame_unref(dst.Get());
	const int rc = av_buffersink_get_frame(m_sink, dst.Get());
	if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
		dst.Reset(nullptr);
		return true;
	}
	if (rc >= 0) {
		const auto timeBase = av_buffersink_get_time_base(m_sink);
		dst.TimeBase(FFmpeg::AVRational{timeBase.num, timeBase.den});
	}
	return rc >= 0;
}

bool FFmpeg::AVFilterGraph::Flush(AVFrame& dst) noexcept {
	if (!m_ptr || !m_src || !m_sink)
		return false;
	if (!m_closed) {
		if (av_buffersrc_close(m_src, m_endPts, 0) < 0)
			return false;
		m_closed = true;
	}
	return Receive(dst);
}

void FFmpeg::AVFilterGraph::Free() noexcept {
	m_src = nullptr;
	m_sink = nullptr;
	m_w = 0;
	m_h = 0;
	m_fmt = 0;
	m_rate = 0;
	m_ch = 0;
	m_mask = 0;
	m_graph.clear();
	m_closed = false;
	m_endPts = AV_NOPTS_VALUE;
	m_timeBase = FFmpeg::AVRational{0, 1};
	m_frameRate = FFmpeg::AVRational{0, 1};
	if (m_ptr)
		avfilter_graph_free(&m_ptr);
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVFilterGraph>;
