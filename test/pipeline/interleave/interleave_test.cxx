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
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string_view>
#include <thread>

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

	bool Paced(const AVStream& stream) noexcept;
	double Seconds(std::int64_t ticks, AVRational base) noexcept;

	class SlowSubtitles final: public Filter::Process {
		public:
			SlowSubtitles(): Filter::Process({}, "slow-subtitle-test") {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Subtitle;
			}

			void Clean() noexcept override {}

			void Setup() noexcept override {}

			void Process(const Frame&) noexcept override {
				std::this_thread::sleep_for(std::chrono::milliseconds{150});
			}
	};

	class EmptySubtitles final: public Filter::Process {
		public:
			EmptySubtitles(): Filter::Process({}, "empty-subtitle-test") {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Subtitle;
			}

			void Clean() noexcept override {}

			void Setup() noexcept override {}

			void Process(const Frame& frame) noexcept override {
				auto& payload = const_cast<Frame&>(frame).Payload();
				static_cast<void>(payload.Drop(payload.Available()));
			}
	};

	int MakeSubtitleInput(const std::filesystem::path& output) {
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, FixturePath(Fixture).string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> media{raw};
		TEST_REQUIRE(avformat_find_stream_info(media.get(), nullptr) >= 0);
		raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, FixturePath("video/anime_like.mkv").string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> subtitles{raw};
		TEST_REQUIRE(avformat_find_stream_info(subtitles.get(), nullptr) >= 0);
		AVFormatContext* mux = nullptr;
		TEST_REQUIRE(avformat_alloc_output_context2(&mux, nullptr, "matroska", output.string().c_str()) >= 0);
		const auto close = [](AVFormatContext* context) {
			avio_closep(&context->pb);
			avformat_free_context(context);
		};
		std::unique_ptr<AVFormatContext, decltype(close)> destination{mux, close};
		for (int index = 0; index != 4; ++index) {
			const AVStream* source = index < 2 ? media->streams[index] : subtitles->streams[2];
			auto* stream = avformat_new_stream(mux, nullptr);
			TEST_REQUIRE(stream && avcodec_parameters_copy(stream->codecpar, source->codecpar) >= 0);
			stream->time_base = source->time_base;
			if (index >= 2)
				TEST_REQUIRE(av_dict_set(&stream->metadata, "language", "eng", 0) >= 0);
		}
		TEST_REQUIRE(avio_open(&mux->pb, output.string().c_str(), AVIO_FLAG_WRITE) >= 0);
		TEST_REQUIRE(avformat_write_header(mux, nullptr) >= 0);
		std::vector<std::unique_ptr<AVPacket, PacketFree>> packets;
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);
		int result = 0;
		while ((result = av_read_frame(media.get(), packet.get())) == 0) {
			const int index = packet->stream_index;
			if (index < 2) {
				av_packet_rescale_ts(packet.get(), media->streams[index]->time_base, mux->streams[index]->time_base);
				packets.emplace_back(av_packet_clone(packet.get()));
				TEST_REQUIRE(packets.back());
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		while ((result = av_read_frame(subtitles.get(), packet.get())) == 0) {
			if (packet->stream_index == 2) {
				for (const int index : {2, 3}) {
					const std::array offsets = index == 2
						? std::array{2000, 6000, 10000} : std::array{1000, 9000, 11800};
					for (const int offset : offsets) {
						// The last show has no clear packet: EOF must deliver the held cue.
						if (offset == 11800 && packet->pts != 0)
							continue;
						std::unique_ptr<AVPacket, PacketFree> copy{av_packet_clone(packet.get())};
						TEST_REQUIRE(copy);
						av_packet_rescale_ts(copy.get(), subtitles->streams[2]->time_base, mux->streams[index]->time_base);
						const auto shift = av_rescale_q(offset, AVRational{1, 1000}, mux->streams[index]->time_base);
						copy->pts += shift;
						copy->dts = copy->pts;
						copy->stream_index = index;
						packets.push_back(std::move(copy));
					}
				}
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		std::stable_sort(packets.begin(), packets.end(), [mux](const auto& a, const auto& b) {
			return av_compare_ts(a->dts, mux->streams[a->stream_index]->time_base,
				b->dts, mux->streams[b->stream_index]->time_base) < 0;
		});
		for (auto& pending : packets)
			TEST_REQUIRE(av_interleaved_write_frame(mux, pending.get()) >= 0);
		TEST_REQUIRE(av_write_trailer(mux) >= 0);
		return 0;
	}

	int CheckSubtitles(const std::filesystem::path& output, bool empty) {
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, output.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{raw};
		TEST_REQUIRE(avformat_find_stream_info(input.get(), nullptr) >= 0);
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);
		std::map<int, std::vector<double>> starts;
		double finalDuration = 0;
		double latestMedia = -1;
		double worstLate = 0;
		int result = 0;
		while ((result = av_read_frame(input.get(), packet.get())) == 0) {
			const auto& stream = *input->streams[packet->stream_index];
			if (packet->pts != AV_NOPTS_VALUE) {
				const double time = Seconds(packet->pts, stream.time_base);
				if (Paced(stream))
					latestMedia = std::max(latestMedia, time);
				else if (stream.codecpar->codec_id == AV_CODEC_ID_ASS) {
					TEST_REQUIRE(packet->size > 0 && packet->duration > 0);
					worstLate = std::max(worstLate, latestMedia - time);
					starts[packet->stream_index].push_back(time);
					if (packet->stream_index == 2 && std::abs(time - 11.8) < 0.01)
						finalDuration = Seconds(packet->duration, stream.time_base);
				}
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		std::cout << "[Test] worst subtitle arrival delay: " << worstLate << " s\n";
		TEST_REQUIRE(worstLate <= MaxSpreadSeconds);
		TEST_REQUIRE(starts[2].size() == (empty ? 0u : 3u));
		TEST_REQUIRE(starts[3].size() == 3);
		for (std::size_t i = 0; i != 3; ++i)
			TEST_REQUIRE(std::abs(starts[3][i] - (2.0 + 4.0 * i)) < 0.01);
		if (!empty) {
			TEST_REQUIRE(std::abs(starts[2].back() - 11.8) < 0.01);
			TEST_REQUIRE(std::abs(finalDuration - 2.0) < 0.01);
		}
		return 0;
	}

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

namespace {
	int CheckLongMovTextGaps(bool fragmented, bool mixedTimeBases) {
		const auto output = OutputPath("pipeline/interleave/long-mov-text-gaps.mp4");
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_alloc_output_context2(&raw, nullptr, "mp4", output.string().c_str()) >= 0);
		raw->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;
		const auto close = [](AVFormatContext* context) {
			avio_closep(&context->pb);
			avformat_free_context(context);
		};
		std::unique_ptr<AVFormatContext, decltype(close)> destination{raw, close};
		for (int index = 0; index != (mixedTimeBases ? 2 : 1); ++index) {
			auto* stream = avformat_new_stream(raw, nullptr);
			TEST_REQUIRE(stream);
			stream->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
			stream->codecpar->codec_id = AV_CODEC_ID_MOV_TEXT;
			stream->time_base = AVRational{1, index == 0 ? 1000000 : 1000};
		}
		TEST_REQUIRE(avio_open(&raw->pb, output.string().c_str(), AVIO_FLAG_WRITE) >= 0);
		AVDictionary* options = nullptr;
		if (fragmented)
			TEST_REQUIRE(av_dict_set(&options, "movflags", "frag_custom+empty_moov", 0) >= 0);
		const int header = avformat_write_header(raw, &options);
		av_dict_free(&options);
		TEST_REQUIRE(header >= 0);

		// Just below, exactly at, and above the signed 32-bit delta limit,
		// followed by a gap requiring more than one empty sample.
		constexpr std::int64_t Limit = std::numeric_limits<int>::max();
		const std::array<std::int64_t, 5> times{
			1000000, 1500000 + Limit - 1, 2000000 + 2 * Limit - 1,
			2500000 + 3 * Limit, 3000000 + 6 * Limit};
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);
		for (std::size_t cue = 0; cue != times.size(); ++cue) {
			for (unsigned index = 0; index != raw->nb_streams; ++index) {
				TEST_REQUIRE(av_new_packet(packet.get(), 3) >= 0);
				packet->data[0] = 0;
				packet->data[1] = 1;
				packet->data[2] = static_cast<std::uint8_t>('A' + cue);
				packet->pts = packet->dts = av_rescale_q(times[cue], AVRational{1, 1000000},
					raw->streams[index]->time_base);
				packet->duration = av_rescale_q(500000, AVRational{1, 1000000}, raw->streams[index]->time_base);
				packet->stream_index = static_cast<int>(index);
				TEST_REQUIRE(av_write_frame(raw, packet.get()) >= 0);
				av_packet_unref(packet.get());
			}
			if (fragmented)
				TEST_REQUIRE(av_write_frame(raw, nullptr) >= 0);
		}
		TEST_REQUIRE(av_write_trailer(raw) >= 0);
		destination.reset();

		AVFormatContext* source = nullptr;
		TEST_REQUIRE(avformat_open_input(&source, output.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{source};
		TEST_REQUIRE(avformat_find_stream_info(source, nullptr) >= 0);
		std::map<int, std::size_t> counts;
		int result = 0;
		while ((result = av_read_frame(source, packet.get())) == 0) {
			if (packet->size > 2) {
				const auto index = packet->stream_index;
				const auto cue = counts[index]++;
				TEST_REQUIRE(cue < times.size());
				const auto base = source->streams[index]->time_base;
				const auto expected = av_rescale_q(times[cue], AVRational{1, 1000000}, base);
				TEST_REQUIRE(packet->pts == expected && packet->dts == expected);
				TEST_REQUIRE(packet->duration == av_rescale_q(500000, AVRational{1, 1000000}, base));
				TEST_REQUIRE(packet->size == 3 && packet->data[2] == 'A' + cue);
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		TEST_REQUIRE(counts.size() == (mixedTimeBases ? 2u : 1u));
		for (const auto& [index, count] : counts)
			TEST_REQUIRE(count == times.size());
		return 0;
	}

	int RunSubtitleInterleave(bool empty, bool cancel = false) {
		const auto ass = Registry::Instance().FindCodec("ASS");
		TEST_REQUIRE(ass && ass->get().HasAccess(Access{Operation::Write}));
		const auto input = OutputPath("pipeline/interleave/sparse-pgs-input.mkv");
		const auto output = OutputPath("pipeline/interleave/sparse-pgs-output.mkv");
		TEST_REQUIRE(MakeSubtitleInput(input) == 0);
		auto logger = MakeLogger();
		Transcoder job{TestLocation(input), TestLocation(output), logger, 12012000000LL};
		job.Video(0).Remux();
		job.Audio(1).Remux();
		auto sparse = job.Subtitle(3);
		sparse.Codec(ass->get()).Filter<SlowSubtitles>();
		if (empty)
			sparse.Filter<EmptySubtitles>();
		job.Subtitle(2).Codec(ass->get()).Filter<SlowSubtitles>();
		job.Subtitle(2).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		if (cancel) {
			std::this_thread::sleep_for(std::chrono::milliseconds{20});
			job.Cancel();
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
			while (job.Status() != Status::Aborted && job.Status() != Status::Error
				&& std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds{10});
			// A running plan can report its explicit cancellation as an error.
			if (job.Status() == Status::Error) {
				const auto error = job.Error();
				TEST_REQUIRE(error && TestView(*error) == "transcode cancelled");
			}
			else
				TEST_REQUIRE(job.Status() == Status::Aborted);
			return 0;
		}
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
		TEST_REQUIRE(CheckInterleave(output) == 0);
		return CheckSubtitles(output, empty);
	}
}

int test_interleave_sparse_ocr_with_remux() {
	return RunSubtitleInterleave(false);
}

int test_interleave_empty_ocr_with_remux() {
	return RunSubtitleInterleave(true);
}

int test_interleave_cancel_pending_ocr() {
	return RunSubtitleInterleave(false, true);
}

int test_mov_text_preserves_long_gaps() {
	return CheckLongMovTextGaps(false, false);
}

int test_mov_text_preserves_long_gaps_mixed_time_bases() {
	return CheckLongMovTextGaps(false, true);
}

int test_mov_text_preserves_long_gaps_fragmented() {
	return CheckLongMovTextGaps(true, false);
}

int RunPipelineInterleaveTests(int argc, char** argv) {
	const std::array tests{
		TestEntry{"test_mov_text_preserves_long_gaps", test_mov_text_preserves_long_gaps},
		TestEntry{"test_mov_text_preserves_long_gaps_mixed_time_bases", test_mov_text_preserves_long_gaps_mixed_time_bases},
		TestEntry{"test_mov_text_preserves_long_gaps_fragmented", test_mov_text_preserves_long_gaps_fragmented},
		TestEntry{"test_interleave_sparse_ocr_with_remux", test_interleave_sparse_ocr_with_remux},
		TestEntry{"test_interleave_empty_ocr_with_remux", test_interleave_empty_ocr_with_remux},
		TestEntry{"test_interleave_cancel_pending_ocr", test_interleave_cancel_pending_ocr},
		TestEntry{"test_interleave_encoded_video_and_audio", test_interleave_encoded_video_and_audio},
		TestEntry{"test_interleave_encoded_video_remuxed_audio", test_interleave_encoded_video_remuxed_audio},
		TestEntry{"test_interleave_remuxed_video_loudnorm_audio", test_interleave_remuxed_video_loudnorm_audio},
		TestEntry{"test_interleave_encoded_video_loudnorm_audio", test_interleave_encoded_video_loudnorm_audio},
	};
	return RunSelectedTest(argc, argv, tests);
}
