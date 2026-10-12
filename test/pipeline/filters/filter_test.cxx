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

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/frames.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/psnr.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/ssim.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/xpsnr.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/adeclick.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/afftdn.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/denoise.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/downmix.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/limiter.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/loudnorm.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/resample.hxx>
#ifdef STORMBYTE_TEST_WITH_GPL
#include <StormByte/multimedia/pipeline/filters/audio/rubberband.hxx>
#endif
#include <StormByte/multimedia/pipeline/filters/audio/upmix.hxx>
#include <StormByte/multimedia/pipeline/filters/video/atadenoise.hxx>
#include <StormByte/multimedia/pipeline/filters/video/bm3d.hxx>
#include <StormByte/multimedia/pipeline/filters/video/bwdif.hxx>
#include <StormByte/multimedia/pipeline/filters/video/cas.hxx>
#include <StormByte/multimedia/pipeline/filters/video/crop.hxx>
#include <StormByte/multimedia/pipeline/filters/video/deband.hxx>
#include <StormByte/multimedia/pipeline/filters/video/decimate.hxx>
#include <StormByte/multimedia/pipeline/filters/video/deflicker.hxx>
#include <StormByte/multimedia/pipeline/filters/video/degrain.hxx>
#include <StormByte/multimedia/pipeline/filters/video/fftdnoiz.hxx>
#include <StormByte/multimedia/pipeline/filters/video/fieldmatch.hxx>
#include <StormByte/multimedia/pipeline/filters/video/format.hxx>
#include <StormByte/multimedia/pipeline/filters/video/fps.hxx>
#include <StormByte/multimedia/pipeline/filters/video/hqdn3d.hxx>
#include <StormByte/multimedia/pipeline/filters/video/interpolate.hxx>
#include <StormByte/multimedia/pipeline/filters/video/nlmeans.hxx>
#include <StormByte/multimedia/pipeline/filters/video/pad.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#ifdef STORMBYTE_TEST_WITH_GPL
#include <StormByte/multimedia/pipeline/filters/video/vaguedenoiser.hxx>
#endif
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/filters/video/yadif.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/property/channel_layout.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <cstring>
#include <limits>
#include <string>

extern "C" {
	#include <libavutil/pixfmt.h>
	#include <libavutil/samplefmt.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::string_view_literals;

namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @brief Exercises cadence resolution without mounting a pipeline.
	 */
	struct DecimateCadenceTest {
		/**
		 * @brief Observes one source frame and optional stream cadence.
		 * @param filter Decimate leaf under test.
		 * @param source Source timing.
		 * @param rate Declared cadence, or invalid when missing.
		 */
		static void Observe(Decimate& filter, const StormByte::Multimedia::FFmpeg::AVFrame& source,
			Property::AVRational rate = {0, 1}) {
			filter.Observe(source, rate);
		}

		/**
		 * @brief Resolves current-stream or measured cadence.
		 * @param filter Decimate leaf under test.
		 * @param rate Current stream cadence, or invalid when missing.
		 * @return Resolved cadence, or zero when unavailable.
		 */
		static Property::AVRational Cadence(const Decimate& filter,
			Property::AVRational rate = {0, 1}) {
			return filter.Cadence(rate);
		}
	};
}

namespace {
	template<typename Attach>
	int RunVideo(const char* leaf, Attach&& attach, unsigned width = 0,
		unsigned height = 0, unsigned fps = 0) {
		auto codec = Registry::Instance().FindCodec("VP9");
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(std::string("pipeline/filters/") + leaf + ".mkv");
		auto logger = MakeLogger();
		TEST_PHASE("creating transcoder");
		Transcoder job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")), TestLocation(output), logger, 2000000000LL};
		auto track = job.Video(0);
		track.Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"})
			.BitRate(1500000);
		StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
		options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
		options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
		track.FineTune(std::move(options));
		attach(track, logger);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		if (const int waited = WaitForOptionalCodec(job); waited != 0)
			return waited;
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Video);
		const auto video = stream.Video();
		TEST_REQUIRE(video);
		if (width > 0)
			TEST_REQUIRE(video->Resolution().Width() == width);
		if (height > 0)
			TEST_REQUIRE(video->Resolution().Height() == height);
		if (fps > 0) {
			TEST_REQUIRE(video->FrameRate());
			TEST_REQUIRE(video->FrameRate()->num == static_cast<int>(fps) * video->FrameRate()->den);
		}
		return 0;
	}

	template<typename Attach>
	int RunAudio(const char* leaf, std::string_view fixture, Attach&& attach,
		unsigned channels = 2, unsigned sampleRate = 48000) {
		auto codec = Registry::Instance().FindCodec("FLAC");
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(std::string("pipeline/filters/") + leaf + ".mka");
		auto logger = MakeLogger();
		TEST_PHASE("creating transcoder");
		Transcoder job{TestLocation(FixturePath(fixture)), TestLocation(output), logger, 2000000000LL};
		auto track = job.Audio(0);
		track.Codec(codec.value().get());
		attach(track, logger);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		if (const int waited = WaitForOptionalCodec(job); waited != 0)
			return waited;
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Audio);
		const auto audio = stream.Audio();
		TEST_REQUIRE(audio);
		TEST_REQUIRE(audio->Channels() == channels);
		TEST_REQUIRE(audio->SampleRate() == sampleRate);
		return 0;
	}

	template<typename Attach>
	int RunAnalytics(const char* leaf, Attach&& attach) {
		const auto output = OutputPath(std::string("pipeline/filters/") + leaf + ".mkv");
		auto logger = MakeLogger();
		TEST_PHASE("creating transcoder");
		Transcoder job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")), TestLocation(output), logger, 2000000000LL};
		job.Video(0).Remux();
		attach(job, logger);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
		TEST_REQUIRE(job.Reports().size() >= 1);
		return 0;
	}
}

int test_filter_decimate_cadence_metadata_and_lifecycle() {
	using CadenceTest = Filter::Video::DecimateCadenceTest;
	Filter::Video::Decimate filter{{}};
	FFmpeg::AVFrame source;
	CadenceTest::Observe(filter, source, {30000, 1001});
	filter.Measured();
	filter.Setup();
	TEST_REQUIRE((CadenceTest::Cadence(filter) == Property::AVRational{30000, 1001}));
	TEST_REQUIRE((CadenceTest::Cadence(filter, {25, 1}) == Property::AVRational{25, 1}));
	TEST_REQUIRE((CadenceTest::Cadence(filter, {24, 0}) == Property::AVRational{30000, 1001}));
	filter.Clean();
	TEST_REQUIRE(CadenceTest::Cadence(filter).num == 0);
	return 0;
}

int test_filter_decimate_cadence_rejects_invalid_timing() {
	using CadenceTest = Filter::Video::DecimateCadenceTest;
	Filter::Video::Decimate filter{{}};
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocVideo(16, 16, AV_PIX_FMT_YUV420P));
	source.TimeBase({1, 1000});
	for (const auto pts : {0, 40, 100}) {
		source.Pts(pts);
		CadenceTest::Observe(filter, source);
	}
	filter.Measured();
	TEST_REQUIRE(CadenceTest::Cadence(filter).num == 0);
	filter.Clean();
	for (const auto pts : {40, 40}) {
		source.Pts(pts);
		CadenceTest::Observe(filter, source);
	}
	filter.Measured();
	TEST_REQUIRE(CadenceTest::Cadence(filter).num == 0);
	filter.Clean();
	source.Pts(std::numeric_limits<std::int64_t>::min() + 1);
	CadenceTest::Observe(filter, source);
	source.Pts(std::numeric_limits<std::int64_t>::max());
	CadenceTest::Observe(filter, source);
	filter.Measured();
	TEST_REQUIRE(CadenceTest::Cadence(filter).num == 0);
	filter.Clean();
	source.Pts(std::numeric_limits<std::int64_t>::min());
	source.TimeBase({0, 1});
	CadenceTest::Observe(filter, source);
	filter.Measured();
	TEST_REQUIRE(CadenceTest::Cadence(filter).num == 0);
	return 0;
}

int test_filter_decimate_cadence_source_timing() {
	using CadenceTest = Filter::Video::DecimateCadenceTest;
	Filter::Video::Decimate filter{{}};
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocVideo(16, 16, AV_PIX_FMT_YUV420P));
	source.TimeBase({1, 30000});
	for (int index = 0; index <= 10; ++index) {
		source.Pts(index * 1001);
		CadenceTest::Observe(filter, source, {0, 0});
	}
	filter.Measured();
	filter.Setup();
	TEST_REQUIRE((CadenceTest::Cadence(filter) == Property::AVRational{30000, 1001}));
	filter.Clean();
	source.TimeBase({1, 1000});
	for (int index = 0; index <= 24; ++index) {
		source.Pts((index * 1000 + 12) / 24);
		CadenceTest::Observe(filter, source);
	}
	filter.Measured();
	TEST_REQUIRE((CadenceTest::Cadence(filter) == Property::AVRational{24, 1}));
	filter.Clean();
	source.TimeBase({1, 24000});
	source.Pts(std::numeric_limits<std::int64_t>::min());
	source.DurationTicks(1001);
	CadenceTest::Observe(filter, source);
	CadenceTest::Observe(filter, source);
	filter.Measured();
	TEST_REQUIRE((CadenceTest::Cadence(filter) == Property::AVRational{24000, 1001}));
	filter.Clean();
	source.DurationTicks(0);
	CadenceTest::Observe(filter, source);
	filter.Measured();
	TEST_REQUIRE(CadenceTest::Cadence(filter).num == 0);
	return 0;
}

int test_filter_graph_delayed_output_and_eof() {
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocVideo(16, 16, AV_PIX_FMT_YUV420P));
	source.Pts(0);
	auto graph = FFmpeg::AVFilterGraph::Open(source, "reverse");
	TEST_REQUIRE(graph);
	FFmpeg::AVFrame output;
	TEST_REQUIRE(graph.Filter(source, output));
	TEST_REQUIRE(!output);
	TEST_REQUIRE(graph.Flush(output));
	TEST_REQUIRE(output);
	TEST_REQUIRE(output.Width() == 16 && output.Height() == 16);
	TEST_REQUIRE(graph.Flush(output));
	TEST_REQUIRE(!output);
	TEST_REQUIRE(graph.Flush(output));
	TEST_REQUIRE(!output);
	return 0;
}

int test_filter_graph_fps_count_and_timestamps() {
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocVideo(16, 16, AV_PIX_FMT_YUV420P));
	source.TimeBase({1, 24});
	source.DurationTicks(1);
	auto graph = FFmpeg::AVFilterGraph::Open(source, "fps=fps=25:round=near");
	TEST_REQUIRE(graph);
	unsigned count = 0;
	for (int index = 0; index < 48; ++index) {
		source.Pts(index);
		FFmpeg::AVFrame output;
		TEST_REQUIRE(graph.Filter(source, output));
		while (output) {
			TEST_REQUIRE(count < 50);
			TEST_REQUIRE(output.TimeBase().num == 1 && output.TimeBase().den == 25);
			TEST_REQUIRE(output.Pts() == count);
			++count;
			TEST_REQUIRE(graph.Receive(output));
		}
	}
	for (unsigned remaining = 0; remaining < 51; ++remaining) {
		FFmpeg::AVFrame output;
		TEST_REQUIRE(graph.Flush(output));
		if (!output) {
			TEST_REQUIRE(count == 50);
			return 0;
		}
		TEST_REQUIRE(count < 50);
		TEST_REQUIRE(output.Pts() == count);
		++count;
	}
	TEST_REQUIRE(false);
	return 1;
}

int test_filter_graph_decimate_uses_nominal_cadence() {
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocVideo(16, 16, AV_PIX_FMT_YUV420P));
	for (int plane = 0; plane < 3; ++plane) {
		const int size = plane == 0 ? 16 : 8;
		for (int row = 0; row < size; ++row)
			std::memset(source.Data(plane) + row * source.Linesize(plane), plane == 0 ? 16 : 128, size);
	}
	source.TimeBase({1, 1000});
	source.DurationTicks(42);
	auto graph = FFmpeg::AVFilterGraph::Open(source, "decimate=cycle=5", {24, 1});
	TEST_REQUIRE(graph);
	TEST_REQUIRE(graph.FrameRate().num == 96 && graph.FrameRate().den == 5);
	unsigned count = 0;
	for (int index = 0; index < 10; ++index) {
		source.Pts((index * 1000 + 12) / 24);
		FFmpeg::AVFrame output;
		TEST_REQUIRE(graph.Filter(source, output));
		for (unsigned attempts = 0; output && attempts < 16; ++attempts) {
			TEST_REQUIRE(++count <= 8);
			TEST_REQUIRE(graph.Receive(output));
		}
		TEST_REQUIRE(!output);
	}
	for (unsigned attempts = 0; attempts < 16; ++attempts) {
		FFmpeg::AVFrame output;
		TEST_REQUIRE(graph.Flush(output));
		if (!output) {
			TEST_REQUIRE(count == 8);
			return 0;
		}
		TEST_REQUIRE(++count <= 8);
	}
	TEST_REQUIRE(false);
	return 1;
}

int test_filter_graph_adeclick_flush_is_bounded() {
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocAudio(1024, AV_SAMPLE_FMT_S16, FFmpeg::AVChannelLayout::Default(2), 48000));
	std::memset(source.Data(0), 0, 1024 * 2 * sizeof(std::int16_t));
	source.TimeBase({1, 48000});
	source.DurationTicks(1024);
	auto graph = FFmpeg::AVFilterGraph::Open(source, "adeclick");
	TEST_REQUIRE(graph);
	unsigned samples = 0;
	for (int index = 0; index < 8; ++index) {
		source.Pts(index * 1024);
		FFmpeg::AVFrame output;
		TEST_REQUIRE(graph.Filter(source, output));
		for (unsigned attempts = 0; output && attempts < 32; ++attempts) {
			TEST_REQUIRE(output.NbSamples() > 0);
			samples += static_cast<unsigned>(output.NbSamples());
			TEST_REQUIRE(samples <= 8192);
			TEST_REQUIRE(graph.Receive(output));
		}
		TEST_REQUIRE(!output);
	}
	for (unsigned attempts = 0; attempts < 32; ++attempts) {
		FFmpeg::AVFrame output;
		TEST_REQUIRE(graph.Flush(output));
		if (!output) {
			TEST_REQUIRE(samples == 8192);
			return 0;
		}
		TEST_REQUIRE(output.NbSamples() > 0);
		samples += static_cast<unsigned>(output.NbSamples());
		TEST_REQUIRE(samples <= 8192);
	}
	TEST_REQUIRE(false);
	return 1;
}

int test_filter_zimg_cache_tracks_input_color() {
	FFmpeg::AVFrame source;
	TEST_REQUIRE(source.AllocVideo(16, 16, AV_PIX_FMT_YUV420P10LE));
	for (int plane = 0; plane < 3; ++plane) {
		const int size = plane == 0 ? 16 : 8;
		for (int row = 0; row < size; ++row) {
			auto* pixels = reinterpret_cast<std::uint16_t*>(source.Data(plane) + row * source.Linesize(plane));
			for (int column = 0; column < size; ++column)
				pixels[column] = static_cast<std::uint16_t>(plane == 0 ? 512 : 400 + ((row + column) % 2) * 200);
		}
	}
	FFmpeg::AVFrame::ColorTarget target;
	target.transfer = AVCOL_TRC_LINEAR;
	target.primaries = AVCOL_PRI_BT709;
	target.matrix = AVCOL_SPC_RGB;
	target.range = AVCOL_RANGE_JPEG;
	target.peak = 100.0;
	FFmpeg::AVFrame::ScaleContext cached;
	static constexpr std::array colors{
		std::array<int, 5>{AVCOL_TRC_SMPTE2084, AVCOL_PRI_BT2020, AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCHROMA_LOC_LEFT},
		std::array<int, 5>{AVCOL_TRC_SMPTE2084, AVCOL_PRI_BT2020, AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_JPEG, AVCHROMA_LOC_LEFT},
		std::array<int, 5>{AVCOL_TRC_SMPTE2084, AVCOL_PRI_BT709, AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCHROMA_LOC_LEFT},
		std::array<int, 5>{AVCOL_TRC_ARIB_STD_B67, AVCOL_PRI_BT2020, AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCHROMA_LOC_LEFT},
		std::array<int, 5>{AVCOL_TRC_SMPTE2084, AVCOL_PRI_BT2020, AVCOL_SPC_BT709, AVCOL_RANGE_MPEG, AVCHROMA_LOC_LEFT},
		std::array<int, 5>{AVCOL_TRC_SMPTE2084, AVCOL_PRI_BT2020, AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCHROMA_LOC_BOTTOM},
	};
	for (const auto& color : colors) {
		source.ColorTransfer(color[0]);
		source.ColorPrimaries(color[1]);
		source.ColorSpace(color[2]);
		source.ColorRange(color[3]);
		source.ChromaLocation(color[4]);
		FFmpeg::AVFrame reused;
		reused.Format(AV_PIX_FMT_GBRPF32LE);
		TEST_REQUIRE(source.ScaleTo(cached, reused, 16, 16, FFmpeg::AVFrame::Resample::Default,
			FFmpeg::AVFrame::Scaler::Zimg, target));
		FFmpeg::AVFrame fresh;
		fresh.Format(AV_PIX_FMT_GBRPF32LE);
		FFmpeg::AVFrame::ScaleContext reference;
		TEST_REQUIRE(source.ScaleTo(reference, fresh, 16, 16, FFmpeg::AVFrame::Resample::Default,
			FFmpeg::AVFrame::Scaler::Zimg, target));
		for (int plane = 0; plane < 3; ++plane) {
			for (int row = 0; row < 16; ++row)
				TEST_REQUIRE(std::memcmp(reused.Data(plane) + row * reused.Linesize(plane),
					fresh.Data(plane) + row * fresh.Linesize(plane), 16 * sizeof(float)) == 0);
		}
	}
	return 0;
}

int test_filter_adeclick() {
	return RunAudio("adeclick", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Adeclick>(logger);
	});
}

int test_filter_afftdn() {
	return RunAudio("afftdn", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Afftdn>(logger);
	});
}

int test_filter_denoise() {
	return RunAudio("denoise", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Denoise>(logger);
	});
}

int test_filter_downmix() {
	return RunAudio("downmix", "audio/noise_51.opus", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Downmix>(logger, Property::ChannelLayout::Stereo);
	});
}

int test_filter_limiter() {
	return RunAudio("limiter", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Limiter>(logger);
	});
}

int test_filter_loudnorm() {
	return RunAudio("loudnorm", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Loudnorm>(logger);
	});
}

int test_filter_resample() {
	return RunAudio("resample", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Resample>(logger, StormByte::Safe::Optional<int>{44100});
	}, 2, 44100);
}

int test_filter_rubberband() {
#ifdef STORMBYTE_TEST_WITH_GPL
	return RunAudio("rubberband", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Rubberband>(logger, StormByte::Safe::Optional<double>{1.25});
	});
#else
	return TEST_SKIP;
#endif
}

int test_filter_upmix() {
	return RunAudio("upmix", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Upmix>(logger, Property::ChannelLayout::FivePointOne);
	}, 6, 48000);
}

int test_filter_atadenoise() {
	return RunVideo("atadenoise", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Atadenoise>(logger);
	});
}

int test_filter_bm3d() {
	return RunVideo("bm3d", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Bm3d>(logger);
	});
}

int test_filter_bwdif() {
	return RunVideo("bwdif", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Bwdif>(logger, false);
	});
}

int test_filter_cas() {
	return RunVideo("cas", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Cas>(logger);
	});
}

int test_filter_crop() {
	return RunVideo("crop", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Crop>(logger, 0, 0, 160, 120);
	}, 160, 120);
}

int test_filter_deband() {
	return RunVideo("deband", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Deband>(logger);
	});
}

int test_filter_decimate() {
	return RunVideo("decimate", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Decimate>(logger);
	});
}

int test_filter_deflicker() {
	return RunVideo("deflicker", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Deflicker>(logger);
	});
}

int test_filter_degrain() {
	return RunVideo("degrain", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Degrain>(logger);
	});
}

int test_filter_fftdnoiz() {
	return RunVideo("fftdnoiz", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Fftdnoiz>(logger);
	});
}

int test_filter_fieldmatch() {
	return RunVideo("fieldmatch", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Fieldmatch>(logger);
	});
}

int test_filter_format() {
	return RunVideo("format", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Format>(logger, AV_PIX_FMT_YUV420P);
	});
}

int test_filter_fps() {
	return RunVideo("fps", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Fps>(logger, 25u);
	}, 0, 0, 25);
}

int test_filter_hqdn3d() {
	return RunVideo("hqdn3d", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Hqdn3d>(logger);
	});
}

int test_filter_interpolate() {
	return RunVideo("interpolate", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Interpolate>(logger, 25u);
	}, 0, 0, 25);
}

int test_filter_nlmeans() {
	return RunVideo("nlmeans", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::NlMeans>(logger);
	});
}

int test_filter_pad() {
	return RunVideo("pad", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Pad>(logger, 1920u, 1080u);
	}, 1920, 1080);
}

int test_filter_scale() {
	return RunVideo("scale", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Scale>(logger, 320u, 180u);
	}, 320, 180);
}

int test_filter_vaguedenoiser() {
#ifdef STORMBYTE_TEST_WITH_GPL
	return RunVideo("vaguedenoiser", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::VagueDenoiser>(logger);
	});
#else
	return TEST_SKIP;
#endif
}

int test_filter_watermark() {
	return RunVideo("watermark", [](auto& track, auto& logger) {
		const auto logo = FixturePath("logo/watermark-noise-logo.png");
		track.template Filter<Filter::Video::Watermark>(logger, StormByte::Safe::String{logo.string()},
			Filter::Video::Anchor::TopRight, 30u, 8);
	});
}

int test_filter_yadif() {
	return RunVideo("yadif", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Yadif>(logger, false);
	});
}

int test_filter_count_frames() {
	return RunAnalytics("count-frames", [](Transcoder& job, auto& logger) {
		job.Filter<Filter::Video::CountFrames>(logger);
	});
}

int test_filter_psnr() {
	return RunAnalytics("psnr", [](Transcoder& job, auto& logger) {
		job.Filter<Filter::Video::PSNR>(logger);
	});
}

int test_filter_ssim() {
	return RunAnalytics("ssim", [](Transcoder& job, auto& logger) {
		job.Filter<Filter::Video::SSIM>(logger);
	});
}

int test_filter_xpsnr() {
	return RunAnalytics("xpsnr", [](Transcoder& job, auto& logger) {
		job.Filter<Filter::Video::XPSNR>(logger);
	});
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_filter_decimate_cadence_metadata_and_lifecycle", test_filter_decimate_cadence_metadata_and_lifecycle},
		TestEntry{"test_filter_decimate_cadence_rejects_invalid_timing", test_filter_decimate_cadence_rejects_invalid_timing},
		TestEntry{"test_filter_decimate_cadence_source_timing", test_filter_decimate_cadence_source_timing},
		TestEntry{"test_filter_graph_delayed_output_and_eof", test_filter_graph_delayed_output_and_eof},
		TestEntry{"test_filter_graph_fps_count_and_timestamps", test_filter_graph_fps_count_and_timestamps},
		TestEntry{"test_filter_graph_decimate_uses_nominal_cadence", test_filter_graph_decimate_uses_nominal_cadence},
		TestEntry{"test_filter_graph_adeclick_flush_is_bounded", test_filter_graph_adeclick_flush_is_bounded},
		TestEntry{"test_filter_zimg_cache_tracks_input_color", test_filter_zimg_cache_tracks_input_color},
		TestEntry{"test_filter_adeclick", test_filter_adeclick},
		TestEntry{"test_filter_afftdn", test_filter_afftdn},
		TestEntry{"test_filter_denoise", test_filter_denoise},
		TestEntry{"test_filter_downmix", test_filter_downmix},
		TestEntry{"test_filter_limiter", test_filter_limiter},
		TestEntry{"test_filter_loudnorm", test_filter_loudnorm},
		TestEntry{"test_filter_resample", test_filter_resample},
		TestEntry{"test_filter_rubberband", test_filter_rubberband},
		TestEntry{"test_filter_upmix", test_filter_upmix},
		TestEntry{"test_filter_atadenoise", test_filter_atadenoise},
		TestEntry{"test_filter_bm3d", test_filter_bm3d},
		TestEntry{"test_filter_bwdif", test_filter_bwdif},
		TestEntry{"test_filter_cas", test_filter_cas},
		TestEntry{"test_filter_crop", test_filter_crop},
		TestEntry{"test_filter_deband", test_filter_deband},
		TestEntry{"test_filter_decimate", test_filter_decimate},
		TestEntry{"test_filter_deflicker", test_filter_deflicker},
		TestEntry{"test_filter_degrain", test_filter_degrain},
		TestEntry{"test_filter_fftdnoiz", test_filter_fftdnoiz},
		TestEntry{"test_filter_fieldmatch", test_filter_fieldmatch},
		TestEntry{"test_filter_format", test_filter_format},
		TestEntry{"test_filter_fps", test_filter_fps},
		TestEntry{"test_filter_hqdn3d", test_filter_hqdn3d},
		TestEntry{"test_filter_interpolate", test_filter_interpolate},
		TestEntry{"test_filter_nlmeans", test_filter_nlmeans},
		TestEntry{"test_filter_pad", test_filter_pad},
		TestEntry{"test_filter_scale", test_filter_scale},
		TestEntry{"test_filter_vaguedenoiser", test_filter_vaguedenoiser},
		TestEntry{"test_filter_watermark", test_filter_watermark},
		TestEntry{"test_filter_yadif", test_filter_yadif},
		TestEntry{"test_filter_count_frames", test_filter_count_frames},
		TestEntry{"test_filter_psnr", test_filter_psnr},
		TestEntry{"test_filter_ssim", test_filter_ssim},
		TestEntry{"test_filter_xpsnr", test_filter_xpsnr},
	};
	return RunSelectedTest(argc, argv, tests);
}
