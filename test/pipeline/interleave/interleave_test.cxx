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

#include "../helpers.hxx"
#include "runners.hxx"

#include <StormByte/multimedia/pipeline/filters/audio/loudnorm.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string_view>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

/*
 * A player seeks to a video cue and decodes every track from that byte offset. If audio for
 * second N sits next to video for second N - latency, the seek lands mute until audio catches
 * up. These cases hold the muxer to file order: no audio or video packet may be written
 * further ahead of the other track than MaxSpreadSeconds, and timestamps must stay continuous.
 */
namespace {
	constexpr std::string_view Fixture = "video/interleave_long.mkv";
	constexpr double MaxSpreadSeconds = 0.5;
	constexpr double MaxAudioGapSeconds = 0.25;

	struct InputCloser {
		void operator()(AVFormatContext* context) const noexcept {
			avformat_close_input(&context);
		}
	};

	struct PacketFree {
		void operator()(AVPacket* packet) const noexcept {
			av_packet_free(&packet);
		}
	};

	bool Paced(const AVStream& stream) noexcept {
		const auto type = stream.codecpar->codec_type;
		return (type == AVMEDIA_TYPE_VIDEO || type == AVMEDIA_TYPE_AUDIO)
			&& (stream.disposition & AV_DISPOSITION_ATTACHED_PIC) == 0;
	}

	double Seconds(std::int64_t ticks, AVRational base) noexcept {
		return static_cast<double>(ticks) * av_q2d(base);
	}

	int CheckInterleave(const std::filesystem::path& output) {
		TEST_PHASE("reading output packets in file order");
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, output.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{raw};
		TEST_REQUIRE(avformat_find_stream_info(input.get(), nullptr) >= 0);
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);

		struct Read {
			int stream;
			double time;
			double duration;
		};
		std::vector<Read> reads;
		std::map<int, double> first;
		int read = 0;
		while ((read = av_read_frame(input.get(), packet.get())) == 0) {
			const AVStream& stream = *input->streams[packet->stream_index];
			const std::int64_t stamp = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;
			if (Paced(stream) && stamp != AV_NOPTS_VALUE) {
				const double time = Seconds(stamp, stream.time_base);
				reads.push_back({packet->stream_index, time, Seconds(packet->duration, stream.time_base)});
				first.try_emplace(packet->stream_index, time);
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(read == AVERROR_EOF);
		TEST_REQUIRE(first.size() >= 2);

		// A track not reached yet in the file sits at its own first timestamp.
		std::map<int, double> latest = first;
		std::map<int, double> previous;
		double worst = 0.0;
		for (const auto& entry : reads) {
			latest[entry.stream] = std::max(latest[entry.stream], entry.time);
			double low = std::numeric_limits<double>::max();
			double high = std::numeric_limits<double>::lowest();
			for (const auto& [index, time] : latest) {
				low = std::min(low, time);
				high = std::max(high, time);
			}
			worst = std::max(worst, high - low);

			const AVStream& stream = *input->streams[entry.stream];
			if (stream.codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
				if (const auto it = previous.find(entry.stream); it != previous.end()) {
					const double step = entry.time - it->second;
					if (step < 0.0 || step > MaxAudioGapSeconds + entry.duration) {
						std::cerr << "[ASSERT] audio stream " << entry.stream << " jumps from "
							<< it->second << " to " << entry.time << std::endl;
						return 1;
					}
				}
				previous[entry.stream] = entry.time;
			}
		}
		std::cout << "[Test] worst audio/video spread in file order: " << worst << " s" << std::endl;
		TEST_REQUIRE(worst <= MaxSpreadSeconds);
		return 0;
	}

	// Holds the whole clip in lookahead so the video encoder lags audio by about its length.
	int ConfigureLaggingVideo(Transcoder& job) {
		auto av1 = Registry::Instance().FindCodec("AV1");
		TEST_REQUIRE(av1);
		if (!av1.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
		options.emplace(StormByte::Safe::String{"lookahead"}, StormByte::Safe::String{"120"});
		job.Video(0).Codec(av1.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libsvtav1"})
			.Preset(StormByte::Safe::String{"12"})
			.CRF(50)
			.FineTune(std::move(options));
		return 0;
	}

	int ConfigureAac(Transcoder& job, const StormByte::Safe::Shared<StormByte::Logger::Log>& logger, bool loudnorm) {
		auto aac = Registry::Instance().FindCodec("AAC");
		TEST_REQUIRE(aac);
		if (!aac.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		auto audio = job.Audio(1);
		audio.Codec(aac.value().get());
		if (loudnorm)
			audio.Filter<Filter::Audio::Loudnorm>(logger);
		return 0;
	}

	int Run(Transcoder& job, const std::filesystem::path& output) {
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		if (const int result = WaitForOptionalCodec(job); result != 0)
			return result;
		return CheckInterleave(output);
	}
}

int test_interleave_encoded_video_and_audio() {
	const auto output = OutputPath("pipeline/interleave/encode-both.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath(Fixture)), TestLocation(output), logger, 12012000000LL};
	if (const int result = ConfigureLaggingVideo(job); result != 0)
		return result;
	if (const int result = ConfigureAac(job, logger, false); result != 0)
		return result;
	return Run(job, output);
}

int test_interleave_encoded_video_remuxed_audio() {
	const auto output = OutputPath("pipeline/interleave/encode-video.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath(Fixture)), TestLocation(output), logger, 12012000000LL};
	if (const int result = ConfigureLaggingVideo(job); result != 0)
		return result;
	job.Audio(1).Remux();
	return Run(job, output);
}

int test_interleave_remuxed_video_loudnorm_audio() {
	const auto output = OutputPath("pipeline/interleave/loudnorm-audio.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath(Fixture)), TestLocation(output), logger, 12012000000LL};
	job.Video(0).Remux();
	if (const int result = ConfigureAac(job, logger, true); result != 0)
		return result;
	return Run(job, output);
}

int test_interleave_encoded_video_loudnorm_audio() {
	const auto output = OutputPath("pipeline/interleave/encode-loudnorm.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath(Fixture)), TestLocation(output), logger, 12012000000LL};
	if (const int result = ConfigureLaggingVideo(job); result != 0)
		return result;
	if (const int result = ConfigureAac(job, logger, true); result != 0)
		return result;
	return Run(job, output);
}

int RunPipelineInterleaveTests(int argc, char** argv) {
	const std::array tests{
		TestEntry{"test_interleave_encoded_video_and_audio", test_interleave_encoded_video_and_audio},
		TestEntry{"test_interleave_encoded_video_remuxed_audio", test_interleave_encoded_video_remuxed_audio},
		TestEntry{"test_interleave_remuxed_video_loudnorm_audio", test_interleave_remuxed_video_loudnorm_audio},
		TestEntry{"test_interleave_encoded_video_loudnorm_audio", test_interleave_encoded_video_loudnorm_audio},
	};
	return RunSelectedTest(argc, argv, tests);
}
