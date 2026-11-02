/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Multimedia.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 *
 * One-off fixture authoring tool; not built or run by the test suite.
 * Stream-copies the start of a Matroska file while storing one media kind's
 * packets a fixed media time after the other kind's packets of the same
 * timestamp, producing deliberately skewed physical interleave.
 */
extern "C" {
#include <libavformat/avformat.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

int main(int argc, char** argv) {
	if (argc != 6) {
		std::fprintf(stderr, "usage: %s input output audio|video skew-seconds limit-seconds\n", argv[0]);
		return 2;
	}
	const AVMediaType lagging = std::strcmp(argv[3], "audio") == 0 ? AVMEDIA_TYPE_AUDIO : AVMEDIA_TYPE_VIDEO;
	const double skew = std::atof(argv[4]), limit = std::atof(argv[5]);
	AVFormatContext* in = nullptr;
	AVFormatContext* out = nullptr;
	if (avformat_open_input(&in, argv[1], nullptr, nullptr) < 0 || avformat_find_stream_info(in, nullptr) < 0
		|| avformat_alloc_output_context2(&out, nullptr, "matroska", argv[2]) < 0)
		return 1;
	for (unsigned i = 0; i < in->nb_streams; ++i) {
		AVStream* stream = avformat_new_stream(out, nullptr);
		if (!stream || avcodec_parameters_copy(stream->codecpar, in->streams[i]->codecpar) < 0)
			return 1;
		stream->codecpar->codec_tag = 0;
		stream->time_base = in->streams[i]->time_base;
	}
	out->flags |= AVFMT_FLAG_BITEXACT;
	if (avio_open(&out->pb, argv[2], AVIO_FLAG_WRITE) < 0 || avformat_write_header(out, nullptr) < 0)
		return 1;
	std::deque<AVPacket*> held;
	const auto seconds = [&](const AVPacket* packet) {
		return packet->dts * av_q2d(in->streams[packet->stream_index]->time_base);
	};
	const auto write = [&](AVPacket* packet) {
		av_packet_rescale_ts(packet, in->streams[packet->stream_index]->time_base,
			out->streams[packet->stream_index]->time_base);
		packet->pos = -1;
		const int result = av_write_frame(out, packet);
		av_packet_free(&packet);
		return result >= 0;
	};
	AVPacket* packet = av_packet_alloc();
	while (packet && av_read_frame(in, packet) >= 0) {
		if (packet->dts == AV_NOPTS_VALUE || seconds(packet) >= limit) {
			av_packet_unref(packet);
			continue;
		}
		AVPacket* owned = av_packet_clone(packet);
		av_packet_unref(packet);
		if (!owned)
			return 1;
		if (in->streams[owned->stream_index]->codecpar->codec_type == lagging) {
			held.push_back(owned);
			continue;
		}
		while (!held.empty() && seconds(held.front()) + skew <= seconds(owned)) {
			if (!write(held.front()))
				return 1;
			held.pop_front();
		}
		if (!write(owned))
			return 1;
	}
	for (AVPacket* rest : held)
		if (!write(rest))
			return 1;
	av_packet_free(&packet);
	if (av_write_trailer(out) < 0)
		return 1;
	avio_closep(&out->pb);
	avformat_free_context(out);
	avformat_close_input(&in);
	return 0;
}
