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

#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/buffer/io/telemetry.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/loudnorm.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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

	struct FrameFree {
		void operator()(AVFrame* frame) const noexcept {
			av_frame_free(&frame);
		}
	};

	struct CodecFree {
		void operator()(AVCodecContext* context) const noexcept {
			avcodec_free_context(&context);
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
	struct DecodedTiming {
		std::atomic<unsigned> Count{0};
		std::atomic<bool> Valid{true};
		std::int64_t Previous = -1;
	};

	class CheckPresentationTimes final: public Filter::Process {
		public:
			explicit CheckPresentationTimes(DecodedTiming& timing):
				Filter::Process({}, "check-presentation-times"), m_timing(timing) {}
			StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
			void Clean() noexcept override {}
			void Setup() noexcept override {}
			void Process(const Frame& frame) noexcept override {
				if (!frame.Pts() || frame.Pts()->Nanoseconds().count() <= m_timing.Previous)
					m_timing.Valid.store(false);
				if (frame.Pts())
					m_timing.Previous = frame.Pts()->Nanoseconds().count();
				m_timing.Count.fetch_add(1);
				std::this_thread::sleep_for(std::chrono::milliseconds{5});
			}
		private:
			DecodedTiming& m_timing;
	};

	int ReadVideoTimes(const std::filesystem::path& path, std::vector<std::int64_t>& times,
		unsigned& corrected) {
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, path.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{raw};
		TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
		const int index = av_find_best_stream(raw, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		TEST_REQUIRE(index >= 0);
		const auto* stream = raw->streams[index];
		const auto* codec = avcodec_find_decoder(stream->codecpar->codec_id);
		TEST_REQUIRE(codec);
		std::unique_ptr<AVCodecContext, CodecFree> decoder{avcodec_alloc_context3(codec)};
		TEST_REQUIRE(decoder);
		TEST_REQUIRE(avcodec_parameters_to_context(decoder.get(), stream->codecpar) >= 0);
		decoder->pkt_timebase = stream->time_base;
		TEST_REQUIRE(avcodec_open2(decoder.get(), codec, nullptr) >= 0);
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		std::unique_ptr<AVFrame, FrameFree> frame{av_frame_alloc()};
		TEST_REQUIRE(packet && frame);
		std::int64_t next = AV_NOPTS_VALUE;
		const auto receive = [&]() {
			int result;
			while ((result = avcodec_receive_frame(decoder.get(), frame.get())) == 0) {
				auto pts = frame->best_effort_timestamp;
				if (pts != AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE && pts != frame->pts)
					++corrected;
				if (pts == AV_NOPTS_VALUE)
					pts = frame->pts != AV_NOPTS_VALUE ? frame->pts : next;
				if (pts == AV_NOPTS_VALUE)
					return AVERROR_INVALIDDATA;
				next = frame->duration > 0 ? pts + frame->duration : AV_NOPTS_VALUE;
				times.push_back(av_rescale_q(pts, stream->time_base, AVRational{1, 1000000000}));
				av_frame_unref(frame.get());
			}
			return result;
		};
		int result;
		while ((result = av_read_frame(raw, packet.get())) == 0) {
			if (packet->stream_index == index) {
				TEST_REQUIRE(avcodec_send_packet(decoder.get(), packet.get()) == 0);
				TEST_REQUIRE(receive() == AVERROR(EAGAIN));
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		TEST_REQUIRE(avcodec_send_packet(decoder.get(), nullptr) == 0);
		TEST_REQUIRE(receive() == AVERROR_EOF);
		return 0;
	}

	struct CapturedTimes {
		std::vector<std::int64_t> Values;
		bool Missing = false;
	};

	class CapturePresentationTimes final: public Filter::Process {
		public:
			explicit CapturePresentationTimes(CapturedTimes& timing):
				Filter::Process({}, "capture-presentation-times"), m_timing(timing) {}
			StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
			void Clean() noexcept override {}
			void Setup() noexcept override {}
			void Process(const Frame& frame) noexcept override {
				if (!frame.Pts())
					m_timing.Missing = true;
				else
					m_timing.Values.push_back(frame.Pts()->Nanoseconds().count());
			}
		private:
			CapturedTimes& m_timing;
	};

	class ReplaceVideoFrame final: public Filter::Process {
		public:
			ReplaceVideoFrame(): Filter::Process({}, "replace-video-frame") {}
			StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
		protected:
			void Clean() noexcept override {}
			void Setup() noexcept override {}
			void Process(const Frame&) noexcept override {
				StormByte::Multimedia::FFmpeg::AVFrame replacement;
				const auto& source = AVFrame();
				if (!replacement.AllocVideo(source.Width(), source.Height(), source.Format())
					|| !replacement.Copy(source)) {
					Fail("could not replace video frame");
					return;
				}
				Save(std::move(replacement));
			}
	};

	struct AudioPacket {
		std::vector<std::uint8_t> Data;
		std::int64_t Pts;
		std::int64_t Dts;
	};

	int ReadAudioPackets(const std::filesystem::path& path, std::vector<AudioPacket>& packets) {
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, path.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{raw};
		TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
		const int index = av_find_best_stream(raw, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
		TEST_REQUIRE(index >= 0);
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);
		int result;
		while ((result = av_read_frame(raw, packet.get())) == 0) {
			if (packet->stream_index == index) {
				TEST_REQUIRE(packet->pts != AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE);
				const auto base = raw->streams[index]->time_base;
				packets.push_back({{packet->data, packet->data + packet->size},
					av_rescale_q(packet->pts, base, AVRational{1, 1000000}),
					av_rescale_q(packet->dts, base, AVRational{1, 1000000})});
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		return 0;
	}

	int CheckDecodedTimeline(std::string_view fixture, std::string_view outputName,
		std::string_view codecName, std::string_view encoder, bool audio, bool expectCorrection,
		bool replaceFrame = false) {
		if (!avcodec_find_encoder_by_name(std::string{encoder}.c_str()))
			return TEST_SKIP;
		const auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec && codec->get().HasAccess(Access{Operation::Write}));
		const auto source = FixturePath(fixture);
		const auto output = OutputPath(outputName);
		std::vector<std::int64_t> expected;
		unsigned corrected = 0;
		TEST_REQUIRE(ReadVideoTimes(source, expected, corrected) == 0);
		TEST_REQUIRE(!expected.empty());
		if (expectCorrection) {
			TEST_REQUIRE(corrected > 0);
			TEST_REQUIRE(expected.size() == 74);
		}
		else {
			TEST_REQUIRE(expected.size() == 33);
			TEST_REQUIRE(expected[1] - expected[0] != expected[2] - expected[1]);
			if (fixture == "video/vfr_subframe_pts.mkv" || fixture == "video/vfr_90k_pts.mp4") {
				TEST_REQUIRE(expected[0] == 0);
				TEST_REQUIRE(expected[1] == 45000000);
				TEST_REQUIRE(expected[2] == 80000000);
				TEST_REQUIRE(expected[3] == 125000000);
			}
		}
		auto logger = MakeLogger();
		CapturedTimes captured;
		Transcoder job{TestLocation(source), TestLocation(output), logger};
		auto track = job.Video(0);
		track.Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder})
			.Acceleration(DecodeAcceleration::Software).Filter<CapturePresentationTimes>(captured);
		if (replaceFrame)
			track.Filter<ReplaceVideoFrame>();
		if (encoder == "libsvtav1")
			track.Preset(StormByte::Safe::String{"12"}).CRF(40);
		else if (encoder == "libx264")
			track.Preset(StormByte::Safe::String{"ultrafast"}).CRF(30);
		else if (encoder == "libvpx-vp9") {
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
			options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
			options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
			track.FineTune(std::move(options));
		}
		if (audio)
			job.Audio(1).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
		TEST_REQUIRE(!captured.Missing);
		TEST_REQUIRE(captured.Values == expected);
		std::vector<std::int64_t> actual;
		unsigned outputCorrected = 0;
		TEST_REQUIRE(ReadVideoTimes(output, actual, outputCorrected) == 0);
		TEST_REQUIRE(actual == expected);
		if (fixture == "video/vfr_subframe_pts.mkv" && encoder == "libsvtav1") {
			AVFormatContext* raw = nullptr;
			TEST_REQUIRE(avformat_open_input(&raw, output.string().c_str(), nullptr, nullptr) == 0);
			std::unique_ptr<AVFormatContext, InputCloser> input{raw};
			TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
			const int index = av_find_best_stream(raw, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
			TEST_REQUIRE(index >= 0);
			std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
			TEST_REQUIRE(packet);
			std::size_t count = 0;
			int result;
			while ((result = av_read_frame(raw, packet.get())) == 0) {
				if (packet->stream_index == index) {
					TEST_REQUIRE(av_rescale_q(packet->duration, raw->streams[index]->time_base,
						AVRational{1, 1000000000}) == 40000000);
					++count;
				}
				av_packet_unref(packet.get());
			}
			TEST_REQUIRE(result == AVERROR_EOF);
			TEST_REQUIRE(count == expected.size());
		}
		if (audio) {
			std::vector<AudioPacket> original;
			std::vector<AudioPacket> remuxed;
			TEST_REQUIRE(ReadAudioPackets(source, original) == 0);
			TEST_REQUIRE(ReadAudioPackets(output, remuxed) == 0);
			TEST_REQUIRE(!original.empty() && original.size() == remuxed.size());
			for (std::size_t i = 0; i < original.size(); ++i) {
				TEST_REQUIRE(original[i].Data == remuxed[i].Data);
				TEST_REQUIRE(std::abs(original[i].Pts - remuxed[i].Pts) <= 1000);
				TEST_REQUIRE(std::abs(original[i].Dts - remuxed[i].Dts) <= 1000);
			}
		}
		return 0;
	}

	int CheckLongMovTextGaps(bool fragmented, bool mixedTimeBases) {
#ifdef STORMBYTE_MM_TEST_FFMPEG_SYSTEM
		// SYSTEM releases through 9.0.2 lack the bundled mov_text gap patch.
		const std::string_view version = av_version_info();
		std::array<unsigned, 3> release{};
		const char* cursor = version.data();
		const char* end = cursor + version.size();
		bool known = true;
		for (std::size_t index = 0; index < release.size(); ++index) {
			const auto parsed = std::from_chars(cursor, end, release[index]);
			if (parsed.ec != std::errc{}) {
				known = false;
				break;
			}
			cursor = parsed.ptr;
			if (cursor == end || *cursor != '.')
				break;
			++cursor;
		}
		if (known && release <= std::array<unsigned, 3>{9, 0, 2}) {
			std::cout << "System FFmpeg " << version
				<< " lacks the bundled long mov_text gap patch." << std::endl;
			return TEST_SKIP;
		}
#endif
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
			TEST_REQUIRE(job.Status() == Status::Aborted);
			TEST_REQUIRE(!job.Error());
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

int test_avi_missing_pts_preserves_timeline_and_progress() {
	const auto output = OutputPath("pipeline/interleave/missing-pts.mkv");
	auto codec = Registry::Instance().FindCodec("VP9");
	TEST_REQUIRE(codec && codec->get().HasAccess(Access{Operation::Write}));
	auto logger = MakeLogger();
	DecodedTiming timing;
	Transcoder job{TestLocation(FixturePath("video/missing_pts_bframes.avi")), TestLocation(output), logger};
	auto track = job.Video(0);
	track.Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"});
	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
	options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
	options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
	track.FineTune(std::move(options));
	track.Filter<CheckPresentationTimes>(timing);
	job.Audio(1).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	bool advanced = false;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
	while (job.Status() != Status::Done && job.Status() != Status::Error
		&& std::chrono::steady_clock::now() < deadline) {
		if (const auto progress = job.Progress())
			advanced |= progress->Main() > 0 && progress->Main() < 100;
		std::this_thread::sleep_for(std::chrono::milliseconds{2});
	}
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	TEST_REQUIRE(timing.Valid.load());
	TEST_REQUIRE(timing.Count.load() == 50);
	TEST_REQUIRE(advanced);
	AVFormatContext* raw = nullptr;
	TEST_REQUIRE(avformat_open_input(&raw, output.string().c_str(), nullptr, nullptr) == 0);
	std::unique_ptr<AVFormatContext, InputCloser> input{raw};
	TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
	TEST_REQUIRE(raw->duration > 1900000 && raw->duration < 2200000);
	std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
	TEST_REQUIRE(packet);
	double previous = -1;
	unsigned count = 0;
	int result = 0;
	while ((result = av_read_frame(raw, packet.get())) == 0) {
		if (packet->stream_index == 0) {
			TEST_REQUIRE(packet->pts != AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE);
			const double pts = Seconds(packet->pts, raw->streams[0]->time_base);
			TEST_REQUIRE(pts > previous && pts >= 0 && pts < 2.2);
			previous = pts;
			++count;
		}
		av_packet_unref(packet.get());
	}
	TEST_REQUIRE(result == AVERROR_EOF && count == 50);
	return 0;
}

int test_avi_reordered_pts_svt_timeline() {
	return CheckDecodedTimeline("video/late_bframes_dx50.avi", "pipeline/interleave/late-b-svt.mkv",
		"AV1", "libsvtav1", true, true);
}

int test_avi_reordered_pts_vp9_timeline() {
	return CheckDecodedTimeline("video/late_bframes_dx50.avi", "pipeline/interleave/late-b-vp9.mkv",
		"VP9", "libvpx-vp9", true, true);
}

int test_vfr_preserves_presentation_timeline() {
	return CheckDecodedTimeline("video/vfr_pts.mkv", "pipeline/interleave/vfr-h264.mp4",
		"H.264", "libx264", false, false);
}

int test_vfr_subframe_h264_timeline() {
	return CheckDecodedTimeline("video/vfr_subframe_pts.mkv", "pipeline/interleave/vfr-subframe-h264.mp4",
		"H.264", "libx264", false, false);
}

int test_vfr_subframe_replaced_frame_timeline() {
	return CheckDecodedTimeline("video/vfr_subframe_pts.mkv", "pipeline/interleave/vfr-replaced-h264.mp4",
		"H.264", "libx264", false, false, true);
}

int test_vfr_subframe_svt_timeline() {
	return CheckDecodedTimeline("video/vfr_subframe_pts.mkv", "pipeline/interleave/vfr-subframe-svt.mkv",
		"AV1", "libsvtav1", false, false);
}

int test_vfr_subframe_vp9_timeline() {
	return CheckDecodedTimeline("video/vfr_subframe_pts.mkv", "pipeline/interleave/vfr-subframe-vp9.mkv",
		"VP9", "libvpx-vp9", false, false);
}

int test_vfr_90k_mpeg4_timeline() {
	return CheckDecodedTimeline("video/vfr_90k_pts.mp4", "pipeline/interleave/vfr-90k-mpeg4.mkv",
		"MPEG-4 Part 2", "mpeg4", false, false);
}

int test_vfr_90k_xvid_timeline() {
	return CheckDecodedTimeline("video/vfr_90k_pts.mp4", "pipeline/interleave/vfr-90k-xvid.mkv",
		"MPEG-4 Part 2", "libxvid", false, false);
}

int test_vfr_90k_mpeg4_unrepresentable() {
	if (!avcodec_find_encoder_by_name("mpeg4"))
		return TEST_SKIP;
	const auto codec = Registry::Instance().FindCodec("MPEG-4 Part 2");
	TEST_REQUIRE(codec && codec->get().HasAccess(Access{Operation::Write}));
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath("video/vfr_90k_unrepresentable.mp4")),
		TestLocation(OutputPath("pipeline/interleave/vfr-90k-unrepresentable.mkv")), logger};
	job.Video(0).Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"mpeg4"})
		.Acceleration(DecodeAcceleration::Software);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job, "video encoder time base cannot represent the source presentation timestamp");
}

int test_cfr_microsecond_clock_mpeg4_timeline() {
	if (!avcodec_find_encoder_by_name("mpeg4"))
		return TEST_SKIP;
	const auto codec = Registry::Instance().FindCodec("MPEG-4 Part 2");
	TEST_REQUIRE(codec && codec->get().HasAccess(Access{Operation::Write}));
	const auto source = FixturePath("video/cfr_24fps_microseconds.mp4");
	const auto output = OutputPath("pipeline/interleave/cfr-24-mpeg4.mp4");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(source), TestLocation(output), logger};
	job.Video(0).Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"mpeg4"})
		.Acceleration(DecodeAcceleration::Software);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	std::vector<std::int64_t> expected, actual;
	unsigned corrected = 0;
	TEST_REQUIRE(ReadVideoTimes(source, expected, corrected) == 0);
	TEST_REQUIRE(ReadVideoTimes(output, actual, corrected) == 0);
	TEST_REQUIRE(expected.size() == 24 && actual.size() == expected.size());
	for (std::size_t i = 0; i < expected.size(); ++i)
		TEST_REQUIRE(av_rescale_q(actual[i], AVRational{1, 1000000000}, AVRational{1, 1000000})
			== av_rescale_q(expected[i], AVRational{1, 1000000000}, AVRational{1, 1000000}));
	return 0;
}

// -------------------
// Progressive output
// -------------------
namespace {
	struct VideoGate {
		std::atomic<bool> Reached{false};
		std::atomic<bool> Release{false};
		bool FailOnRelease = false;
	};

	class PauseVideo final: public Filter::Process {
		public:
			explicit PauseVideo(VideoGate& gate):
				Filter::Process({}, "pause-video-test"), m_gate(gate) {}

			PauseVideo(const PauseVideo&) = delete;

			PauseVideo(PauseVideo&&) = delete;

			~PauseVideo() noexcept override = default;

			PauseVideo& operator=(const PauseVideo&) = delete;

			PauseVideo& operator=(PauseVideo&&) = delete;

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Video;
			}

			void Clean() noexcept override {}

			void Setup() noexcept override {}

			void Process(const Frame& frame) noexcept override {
				if (!frame.Pts() || frame.Pts()->Nanoseconds().count() < 8000000000LL)
					return;
				m_gate.Reached.store(true);
				while (!m_gate.Release.load())
					std::this_thread::sleep_for(std::chrono::milliseconds{1});
				if (m_gate.FailOnRelease)
					Fail("in-flight video filter failed");
			}

		private:
			VideoGate& m_gate;	///< Borrowed test gate, released before job teardown.
	};

	struct ReleaseVideo {
		VideoGate& Gate;

		~ReleaseVideo() noexcept {
			Gate.Release.store(true);
		}
	};

	class CancellationJob final: public Transcoder {
		public:
			using Transcoder::Transcoder;
			std::atomic<unsigned> AbortedCalls{0};
			std::atomic<unsigned> ErrorCalls{0};

		protected:
			void OnAborted() noexcept override {
				AbortedCalls.fetch_add(1);
			}

			void OnError(const StormByte::Safe::String&) noexcept override {
				ErrorCalls.fetch_add(1);
			}
	};

	int MakeSparseInput(const std::filesystem::path& output, bool empty, bool skewed = false) {
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, FixturePath(Fixture).string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{raw};
		TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
		AVFormatContext* mux = nullptr;
		TEST_REQUIRE(avformat_alloc_output_context2(&mux, nullptr, "matroska", output.string().c_str()) >= 0);
		const auto close = [](AVFormatContext* context) {
			avio_closep(&context->pb);
			avformat_free_context(context);
		};
		std::unique_ptr<AVFormatContext, decltype(close)> destination{mux, close};
		for (int index = 0; index != 4; ++index) {
			auto* stream = avformat_new_stream(mux, nullptr);
			TEST_REQUIRE(stream);
			if (index < 2) {
				TEST_REQUIRE(avcodec_parameters_copy(stream->codecpar, input->streams[index]->codecpar) >= 0);
				stream->time_base = input->streams[index]->time_base;
			}
			else {
				stream->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
				stream->codecpar->codec_id = AV_CODEC_ID_SUBRIP;
				stream->time_base = AVRational{1, 1000};
			}
		}
		TEST_REQUIRE(avio_open(&mux->pb, output.string().c_str(), AVIO_FLAG_WRITE) >= 0);
		TEST_REQUIRE(avformat_write_header(mux, nullptr) >= 0);
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);
		bool cue = empty;
		int result = 0;
		// Bypass source interleaving to exercise a cue arriving 800 ms behind A/V.
		while ((result = av_read_frame(input.get(), packet.get())) == 0) {
			const int index = packet->stream_index;
			if (index < 2) {
				av_packet_rescale_ts(packet.get(), input->streams[index]->time_base, mux->streams[index]->time_base);
				if (!cue && Seconds(packet->dts, mux->streams[index]->time_base) >= (skewed ? 10.8 : 10)) {
					std::unique_ptr<AVPacket, PacketFree> subtitle{av_packet_alloc()};
					TEST_REQUIRE(subtitle && av_new_packet(subtitle.get(), 4) >= 0);
					std::memcpy(subtitle->data, "late", 4);
					subtitle->stream_index = 2;
					subtitle->pts = subtitle->dts = av_rescale_q(10, AVRational{1, 1}, mux->streams[2]->time_base);
					subtitle->duration = av_rescale_q(1, AVRational{1, 1}, mux->streams[2]->time_base);
					TEST_REQUIRE((skewed ? av_write_frame(mux, subtitle.get())
						: av_interleaved_write_frame(mux, subtitle.get())) >= 0);
					cue = true;
				}
				TEST_REQUIRE((skewed ? av_write_frame(mux, packet.get())
					: av_interleaved_write_frame(mux, packet.get())) >= 0);
			}
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF && cue);
		TEST_REQUIRE(av_write_trailer(mux) >= 0);
		return 0;
	}

	int CheckSparseOutput(const std::filesystem::path& source, const std::filesystem::path& path, bool empty,
		bool remux = false) {
		using Payloads = std::vector<std::vector<std::uint8_t>>;
		Payloads expectedAudio;
		AVFormatContext* raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, source.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> original{raw};
		TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
		std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
		TEST_REQUIRE(packet);
		unsigned expectedVideo = 0;
		int result = 0;
		while ((result = av_read_frame(raw, packet.get())) == 0) {
			if (packet->stream_index == 0)
				++expectedVideo;
			else if (packet->stream_index == 1)
				expectedAudio.emplace_back(packet->data, packet->data + packet->size);
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF && expectedVideo > 0 && !expectedAudio.empty());
		raw = nullptr;
		TEST_REQUIRE(avformat_open_input(&raw, path.string().c_str(), nullptr, nullptr) == 0);
		std::unique_ptr<AVFormatContext, InputCloser> input{raw};
		TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0 && raw->nb_streams == 4);
		TEST_REQUIRE(raw->streams[0]->codecpar->codec_id
			== (remux ? original->streams[0]->codecpar->codec_id : AV_CODEC_ID_HEVC));
		TEST_REQUIRE(raw->duration >= 11900000 && raw->duration <= 12200000);
		std::map<int, std::int64_t> previous;
		std::array<unsigned, 4> count{};
		Payloads actualAudio;
		double latestMedia = -1;
		while ((result = av_read_frame(raw, packet.get())) == 0) {
			const int index = packet->stream_index;
			const auto stamp = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;
			TEST_REQUIRE(index >= 0 && index < 4);
			++count[index];
			const auto dts = packet->dts;
			if (dts != AV_NOPTS_VALUE) {
				if (const auto found = previous.find(index); found != previous.end())
					TEST_REQUIRE(dts >= found->second);
				previous[index] = dts;
			}
			if (index == 2) {
				TEST_REQUIRE(packet->size == 4 && std::memcmp(packet->data, "late", 4) == 0);
				TEST_REQUIRE(std::abs(Seconds(packet->pts, raw->streams[index]->time_base) - 10) < 0.1);
				TEST_REQUIRE(std::abs(Seconds(packet->duration, raw->streams[index]->time_base) - 1) < 0.01);
				if (latestMedia - Seconds(stamp, raw->streams[index]->time_base) > MaxSpreadSeconds)
					std::cerr << "[DETAIL] sparse subtitle timestamp=" << Seconds(stamp, raw->streams[index]->time_base)
						<< " latest A/V timestamp=" << latestMedia << " limit=" << MaxSpreadSeconds << '\n';
				TEST_REQUIRE(latestMedia - Seconds(stamp, raw->streams[index]->time_base) <= MaxSpreadSeconds);
			}
			else if (index == 1)
				actualAudio.emplace_back(packet->data, packet->data + packet->size);
			if (Paced(*raw->streams[index]) && stamp != AV_NOPTS_VALUE)
				latestMedia = std::max(latestMedia, Seconds(stamp, raw->streams[index]->time_base));
			av_packet_unref(packet.get());
		}
		TEST_REQUIRE(result == AVERROR_EOF);
		TEST_REQUIRE(count[0] == expectedVideo && actualAudio == expectedAudio);
		TEST_REQUIRE(count[2] == (empty ? 0u : 1u) && count[3] == 0);
		return CheckInterleave(path);
	}

	int RunSparseRetention(bool empty, bool cancel, bool deep = false, bool paused = false, bool fail = false) {
		if (!avcodec_find_encoder_by_name("libx265"))
			return TEST_SKIP;
		const auto codec = Registry::Instance().FindCodec("H.265");
		TEST_REQUIRE(codec);
		const std::string name = fail ? "retention-failure" : paused ? "retention-paused-cancel" : cancel ? "retention-cancel"
			: deep ? "retention-deep" : empty ? "retention-empty" : "retention-sparse";
		const auto input = OutputPath("pipeline/interleave/" + name + "-input.mkv");
		const auto output = OutputPath("pipeline/interleave/" + name + "-output.mkv");
		TEST_REQUIRE(MakeSparseInput(input, empty) == 0);
		VideoGate gate;
		gate.FailOnRelease = fail;
		StormByte::Buffer::ByteBudget budget{StormByte::ByteSize{1024 * 1024}};
		StormByte::Safe::Shared<const JobTelemetry> retained;
		auto teardownStarted = std::chrono::steady_clock::now();
		{
			namespace IO = StormByte::Buffer::IO;
			auto writer = StormByte::Safe::Unique<IO::BufferedLocationWriter>::MakePointer<IO::BufferedFileWriter>(
				TestLocation(output), IO::WriterParameters{IO::MaxMemory{0}, IO::WriteChunk{0}});
			CancellationJob job{TestLocation(input), std::move(writer), MakeLogger(), 12012000000LL};
			ReleaseVideo release{gate};
			BufferingPolicy buffering;
			buffering.VideoFrames(QueueLimit::Bounded(2));
			buffering.AudioFrames(QueueLimit::Bounded(1));
			buffering.Packets(QueueLimit::Bounded(deep ? 4096 : 1));
			buffering.DemuxPending(QueueLimit::Bounded(1));
			buffering.QueueBytes(StormByte::ByteSize{256 * 1024});
			buffering.Budget(budget);
			job.Buffering(buffering);
			job.Resources(ResourceUsage::Low);
			auto video = job.Video(0);
			video.Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libx265"})
				.Preset(StormByte::Safe::String{"ultrafast"}).CRF(35);
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
			options.emplace(StormByte::Safe::String{"rc-lookahead"}, StormByte::Safe::String{"24"});
			options.emplace(StormByte::Safe::String{"bframes"}, StormByte::Safe::String{"4"});
			video.FineTune(std::move(options)).Filter<PauseVideo>(gate);
			job.Audio(1).Remux();
			job.Subtitle(2).Remux();
			job.Subtitle(3).Remux();
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			retained = job.Telemetry();
			job.Run();
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
			while (!gate.Reached.load() && job.Status() != Status::Error
				&& std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds{2});
			TEST_REQUIRE(gate.Reached.load());
			const auto telemetry = job.OutputTelemetry();
			TEST_REQUIRE(telemetry);
			const auto io = StormByte::Safe::DynamicPointerCast<StormByte::Buffer::IO::WriteTelemetry>(telemetry);
			TEST_REQUIRE(io);
			// A paused producer prevents EOF/trailer from masquerading as progressive output.
			while (io->Origin() < StormByte::ByteSize{32768}
				&& std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds{2});
			std::cout << "[Test] bytes accepted before video resumes: " << telemetry->Accepted() << '\n';
			TEST_REQUIRE(telemetry->Accepted() >= StormByte::ByteSize{32768});
			TEST_REQUIRE(io->Origin() >= StormByte::ByteSize{32768});
			const auto accounting = budget.Snapshot();
			TEST_REQUIRE(accounting.Peak > StormByte::ByteSize{0} && accounting.Peak <= accounting.Limit);
			TEST_REQUIRE(job.Status() == Status::Running);
			const auto empty_subtitle_idle = [](const TopologySnapshot& snapshot) {
				for (const auto& stage : snapshot.Stages) {
					if (stage.Kind != TopologyStageKind::Remuxer || stage.Track != 3)
						continue;
					for (const auto& worker : stage.Activity.Workers)
						if (worker.Available && worker.Role == ActivityRole::Main
							&& (worker.State == ActivityState::WaitingInput || worker.State == ActivityState::Finished))
							return true;
				}
				return false;
			};
			// Deep queues can reach input EOF before video pauses; allow the empty remuxer to finish.
			auto live = retained->Topology();
			while (!empty_subtitle_idle(live) && std::chrono::steady_clock::now() < deadline) {
				std::this_thread::sleep_for(std::chrono::milliseconds{2});
				live = retained->Topology();
			}
			TEST_REQUIRE(empty_subtitle_idle(live));
			for (const auto& stage : live.Stages) {
				TEST_REQUIRE(ActivityConserved(stage.Activity));
				if (stage.Kind != TopologyStageKind::Remuxer || stage.Track != 3)
					continue;
				TEST_REQUIRE(stage.Activity.Available && stage.Activity.Complete);
				for (const auto& ref : stage.Queues)
					if (ref.Role == TopologyQueueRole::Input)
						for (const auto& queue : live.Queues)
							if (queue.Id == ref.Queue)
								TEST_REQUIRE(queue.Snapshot.Elements == StormByte::Size{0});
			}
			if (paused) {
				job.Pause();
				TEST_REQUIRE(job.Status() == Status::Paused);
			}
			const auto cancelStarted = std::chrono::steady_clock::now();
			if (cancel) {
				job.Cancel();
				job.Cancel();
				job.Resume();
			}
			gate.Release.store(true);
			if (cancel || fail) {
				const auto stopped = std::chrono::steady_clock::now() + std::chrono::seconds{5};
				while (job.AbortedCalls.load() == 0 && job.ErrorCalls.load() == 0
					&& std::chrono::steady_clock::now() < stopped)
					std::this_thread::sleep_for(std::chrono::milliseconds{2});
				TEST_REQUIRE(std::chrono::steady_clock::now() - cancelStarted < std::chrono::seconds{5});
				TEST_REQUIRE(job.Status() == (fail ? Status::Error : Status::Aborted));
				TEST_REQUIRE(job.AbortedCalls.load() == (fail ? 0u : 1u));
				TEST_REQUIRE(job.ErrorCalls.load() == (fail ? 1u : 0u));
				TEST_REQUIRE(job.Plan()->State() == (fail ? Plan::State::Failed : Plan::State::Aborted));
				if (fail) {
					TEST_REQUIRE(job.Error() && TestView(*job.Error()) == "in-flight video filter failed");
					job.Cancel();
					TEST_REQUIRE(job.Status() == Status::Error);
					TEST_REQUIRE(job.Error() && TestView(*job.Error()) == "in-flight video filter failed");
				}
				else
					TEST_REQUIRE(!job.Error());
				TEST_REQUIRE(budget.Snapshot().Reserved == StormByte::ByteSize{0});
			}
			else
				TEST_REQUIRE(WaitForTranscoder(job) == 0);
			teardownStarted = std::chrono::steady_clock::now();
		}
		TEST_REQUIRE(std::chrono::steady_clock::now() - teardownStarted < std::chrono::seconds{5});
		TEST_REQUIRE(budget.Snapshot().Reserved == StormByte::ByteSize{0});
		TEST_REQUIRE(!budget.Snapshot().Closed);
		const auto final_activity = retained->Topology();
		TEST_REQUIRE(final_activity.State == TopologySnapshotState::Final);
		for (const auto& stage : final_activity.Stages) {
			TEST_REQUIRE(ActivityConserved(stage.Activity));
			for (const auto& worker : stage.Activity.Workers)
				if (worker.Available)
					TEST_REQUIRE(worker.State == ActivityState::Finished || worker.State == ActivityState::Failed);
		}
		if (cancel) {
			{
				Transcoder next{TestLocation(input), TestLocation(output), MakeLogger(), 12012000000LL};
				BufferingPolicy buffering;
				buffering.Packets(QueueLimit::Bounded(1));
				buffering.DemuxPending(QueueLimit::Bounded(1));
				buffering.QueueBytes(StormByte::ByteSize{256 * 1024});
				buffering.Budget(budget);
				next.Buffering(buffering);
				next.Video(0).Remux();
				next.Audio(1).Remux();
				next.Subtitle(2).Remux();
				next.Subtitle(3).Remux();
				TEST_REQUIRE(CheckTranscoderConfigured(next) == 0);
				next.Run();
				TEST_REQUIRE(WaitForTranscoder(next) == 0);
			}
			TEST_REQUIRE(budget.Snapshot().Reserved == StormByte::ByteSize{0});
			TEST_REQUIRE(!budget.Snapshot().Closed);
			return CheckSparseOutput(input, output, empty, true);
		}
		return fail ? 0 : CheckSparseOutput(input, output, empty);
	}
}

int test_interleave_cancel_retained_mixed_packets() {
	return RunSparseRetention(false, true);
}

int test_interleave_cancel_paused_mixed_packets() {
	return RunSparseRetention(false, true, false, true);
}

int test_interleave_cancel_preserves_inflight_failure() {
	return RunSparseRetention(false, true, false, false, true);
}

int test_interleave_inflight_failure_without_cancel() {
	return RunSparseRetention(false, false, false, false, true);
}

int test_interleave_deep_queues_do_not_extend_retention() {
	return RunSparseRetention(false, false, true);
}

int test_interleave_empty_subtitles_with_delayed_video() {
	return RunSparseRetention(true, false);
}

int test_interleave_sparse_subtitles_with_delayed_video() {
	return RunSparseRetention(false, false);
}

int test_interleave_sparse_remux_source_skew() {
	const auto input = OutputPath("pipeline/interleave/remux-skew-input.mkv");
	const auto output = OutputPath("pipeline/interleave/remux-skew-output.mkv");
	TEST_REQUIRE(MakeSparseInput(input, false, true) == 0);
	for (const unsigned packets : {1u, 4096u}) {
		{
			Transcoder job{TestLocation(input), TestLocation(output), MakeLogger(), 12012000000LL};
			BufferingPolicy buffering;
			buffering.Packets(QueueLimit::Bounded(packets));
			buffering.DemuxPending(QueueLimit::Bounded(1));
			job.Buffering(buffering);
			job.Video(0).Remux();
			job.Audio(1).Remux();
			job.Subtitle(2).Remux();
			job.Subtitle(3).Remux();
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		TEST_REQUIRE(CheckSparseOutput(input, output, false, true) == 0);
	}
	return 0;
}

// -------------------
// Queue
// -------------------
int test_three_track_remux_with_small_shared_queues() {
	using StormByte::ByteSize;
	const auto inputPath = FixturePath("video/ffv1_two_flac.mkv");
	const auto outputPath = OutputPath("pipeline/interleave/small-shared-queues.mkv");
	StormByte::Buffer::ByteBudget budget{ByteSize{8 * 1024 * 1024}};
	BufferingPolicy buffering;
	buffering.VideoFrames(QueueLimit::Bounded(2));
	buffering.AudioFrames(QueueLimit::Bounded(1));
	buffering.Packets(QueueLimit::Bounded(1));
	buffering.DemuxPending(QueueLimit::Bounded(1));
	buffering.QueueBytes(ByteSize{1024 * 1024});
	buffering.Budget(budget);
	using Payloads = std::array<std::vector<std::vector<std::uint8_t>>, 3>;
	Payloads expected;
	for (std::size_t iteration = 0; iteration < 24; ++iteration) {
		StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
		if (iteration % 2 == 0) {
			Transcoder job{TestLocation(inputPath), TestLocation(outputPath), MakeLogger(), 3000000000LL};
			job.Buffering(buffering);
			job.Video(0).Remux();
			job.Audio(1).Remux();
			job.Audio(2).Remux();
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		else {
			auto plan = StormByte::Safe::MakeShared<Plan>(
				TestLocation(inputPath), TestLocation(outputPath), 3000000000LL);
			TEST_REQUIRE(plan->Buffering(buffering));
			plan->add(Track{0, StormByte::Multimedia::Type::Video});
			plan->add(Track{1, StormByte::Multimedia::Type::Audio});
			plan->add(Track{2, StormByte::Multimedia::Type::Audio});
			TEST_REQUIRE(plan->Check());
			Demuxer demuxer{noLogger};
			Remuxer video{noLogger, 0};
			Remuxer audioOne{noLogger, 1};
			Remuxer audioTwo{noLogger, 2};
			Muxer muxer{noLogger};
			ManualPipelineStop teardown{&demuxer, &video, &audioOne, &audioTwo, &muxer};
			demuxer >> muxer;
			demuxer >> video >> muxer;
			demuxer >> audioOne >> muxer;
			demuxer >> audioTwo >> muxer;
			plan >> demuxer;
			for (Step* stage : std::array<Step*, 5>{&demuxer, &video, &audioOne, &audioTwo, &muxer})
				TEST_REQUIRE(WaitForManualPipeline(*stage) == 0);
			TEST_REQUIRE(plan->State() == Plan::State::Finished);
			const std::array<Remuxer*, 3> remuxers{&video, &audioOne, &audioTwo};
			for (int track = 0; track < 3; ++track) {
				for (const Step* stage : std::array<const Step*, 2>{remuxers[track], &muxer}) {
					const auto queue = stage->QueueSnapshot(track);
					TEST_REQUIRE(queue.Exists);
					TEST_REQUIRE(queue.Limits.ElementsLimited());
					TEST_REQUIRE(queue.Limits.Elements() == StormByte::Size{1});
					TEST_REQUIRE(queue.Limits.BytesLimited());
					TEST_REQUIRE(queue.Limits.Bytes() == ByteSize{1024 * 1024});
					TEST_REQUIRE(queue.PeakElements == StormByte::Size{1});
					TEST_REQUIRE(queue.PeakBytes > ByteSize{0} && queue.PeakBytes <= queue.Limits.Bytes());
					TEST_REQUIRE(queue.Elements == StormByte::Size{0} && queue.Bytes == ByteSize{0});
				}
			}
		}
		const auto accounting = budget.Snapshot();
		TEST_REQUIRE(accounting.Limited && accounting.Limit == ByteSize{8 * 1024 * 1024});
		TEST_REQUIRE(accounting.Peak > ByteSize{0} && accounting.Peak <= accounting.Limit);
		TEST_REQUIRE(accounting.Reserved == ByteSize{0} && !accounting.Closed);
		for (const bool source : {true, false}) {
			if (source && iteration > 0)
				continue;
			const auto& path = source ? inputPath : outputPath;
			AVFormatContext* raw = nullptr;
			TEST_REQUIRE(avformat_open_input(&raw, path.string().c_str(), nullptr, nullptr) == 0);
			std::unique_ptr<AVFormatContext, InputCloser> input{raw};
			TEST_REQUIRE(avformat_find_stream_info(raw, nullptr) >= 0);
			TEST_REQUIRE(raw->nb_streams == 3);
			TEST_REQUIRE(raw->streams[0]->codecpar->codec_id == AV_CODEC_ID_FFV1);
			TEST_REQUIRE(raw->streams[0]->codecpar->width == 320 && raw->streams[0]->codecpar->height == 180);
			TEST_REQUIRE(raw->streams[1]->codecpar->codec_id == AV_CODEC_ID_FLAC);
			TEST_REQUIRE(raw->streams[2]->codecpar->codec_id == AV_CODEC_ID_FLAC);
			TEST_REQUIRE(raw->duration >= 2990000 && raw->duration <= 3010000);
			std::unique_ptr<AVPacket, PacketFree> packet{av_packet_alloc()};
			TEST_REQUIRE(packet);
			Payloads actual;
			int result = 0;
			while ((result = av_read_frame(raw, packet.get())) == 0) {
				TEST_REQUIRE(packet->stream_index >= 0 && packet->stream_index < 3);
				TEST_REQUIRE(packet->size > 0);
				actual[packet->stream_index].emplace_back(packet->data, packet->data + packet->size);
				av_packet_unref(packet.get());
			}
			TEST_REQUIRE(result == AVERROR_EOF);
			if (source) {
				expected = std::move(actual);
				for (const auto& track : expected)
					TEST_REQUIRE(!track.empty());
			}
			else
				TEST_REQUIRE(actual == expected);
		}
	}
	return 0;
}

int RunPipelineInterleaveTests(int argc, char** argv) {
	const std::array tests{
		TestEntry{"test_avi_missing_pts_preserves_timeline_and_progress", test_avi_missing_pts_preserves_timeline_and_progress},
		TestEntry{"test_avi_reordered_pts_svt_timeline", test_avi_reordered_pts_svt_timeline},
		TestEntry{"test_avi_reordered_pts_vp9_timeline", test_avi_reordered_pts_vp9_timeline},
		TestEntry{"test_vfr_preserves_presentation_timeline", test_vfr_preserves_presentation_timeline},
		TestEntry{"test_vfr_subframe_h264_timeline", test_vfr_subframe_h264_timeline},
		TestEntry{"test_vfr_subframe_replaced_frame_timeline", test_vfr_subframe_replaced_frame_timeline},
		TestEntry{"test_vfr_subframe_svt_timeline", test_vfr_subframe_svt_timeline},
		TestEntry{"test_vfr_subframe_vp9_timeline", test_vfr_subframe_vp9_timeline},
		TestEntry{"test_vfr_90k_mpeg4_timeline", test_vfr_90k_mpeg4_timeline},
		TestEntry{"test_vfr_90k_xvid_timeline", test_vfr_90k_xvid_timeline},
		TestEntry{"test_vfr_90k_mpeg4_unrepresentable", test_vfr_90k_mpeg4_unrepresentable},
		TestEntry{"test_cfr_microsecond_clock_mpeg4_timeline", test_cfr_microsecond_clock_mpeg4_timeline},
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
		// -------------------
		// Progressive output
		// -------------------
		TestEntry{"test_interleave_cancel_retained_mixed_packets", test_interleave_cancel_retained_mixed_packets},
		TestEntry{"test_interleave_cancel_paused_mixed_packets", test_interleave_cancel_paused_mixed_packets},
		TestEntry{"test_interleave_cancel_preserves_inflight_failure", test_interleave_cancel_preserves_inflight_failure},
		TestEntry{"test_interleave_inflight_failure_without_cancel", test_interleave_inflight_failure_without_cancel},
		TestEntry{"test_interleave_deep_queues_do_not_extend_retention", test_interleave_deep_queues_do_not_extend_retention},
		TestEntry{"test_interleave_empty_subtitles_with_delayed_video", test_interleave_empty_subtitles_with_delayed_video},
		TestEntry{"test_interleave_sparse_subtitles_with_delayed_video", test_interleave_sparse_subtitles_with_delayed_video},
		TestEntry{"test_interleave_sparse_remux_source_skew", test_interleave_sparse_remux_source_skew},
		// -------------------
		// Queue
		// -------------------
		TestEntry{"test_three_track_remux_with_small_shared_queues", test_three_track_remux_with_small_shared_queues},
	};
	return RunSelectedTest(argc, argv, tests);
}
