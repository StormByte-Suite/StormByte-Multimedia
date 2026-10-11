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
#include <StormByte/multimedia/pipeline/filters/audio/rubberband.hxx>
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
#include <StormByte/multimedia/pipeline/filters/video/vaguedenoiser.hxx>
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/filters/video/yadif.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/property/channel_layout.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <string>

extern "C" {
	#include <libavutil/pixfmt.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::string_view_literals;

namespace {
	template<typename Attach>
	int RunVideo(const char* leaf, Attach&& attach) {
		auto codec = Registry::Instance().FindCodec("H.264");
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(std::string("pipeline/filters/") + leaf + ".mkv");
		auto logger = MakeLogger();
		TEST_PHASE("creating transcoder");
		Transcoder job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")), TestLocation(output), logger, 2000000000LL};
		auto track = job.Video(0);
		track.Codec(codec.value().get())
			.Implementation(StormByte::Safe::String{"libx264"})
			.Preset(StormByte::Safe::String{"ultrafast"}).CRF(30);
		attach(track, logger);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		if (const int waited = WaitForOptionalCodec(job); waited != 0)
			return waited;
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(!opened.value().Streams().empty());
		return 0;
	}

	template<typename Attach>
	int RunAudio(const char* leaf, std::string_view fixture, Attach&& attach) {
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
		TEST_REQUIRE(!opened.value().Streams().empty());
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
	});
}

int test_filter_rubberband() {
	return RunAudio("rubberband", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Rubberband>(logger, StormByte::Safe::Optional<double>{1.25});
	});
}

int test_filter_upmix() {
	return RunAudio("upmix", "audio/noise_stereo.wav", [](auto& track, auto& logger) {
		track.template Filter<Filter::Audio::Upmix>(logger, Property::ChannelLayout::FivePointOne);
	});
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
	});
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
	});
}

int test_filter_hqdn3d() {
	return RunVideo("hqdn3d", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Hqdn3d>(logger);
	});
}

int test_filter_interpolate() {
	return RunVideo("interpolate", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Interpolate>(logger, 25u);
	});
}

int test_filter_nlmeans() {
	return RunVideo("nlmeans", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::NlMeans>(logger);
	});
}

int test_filter_pad() {
	return RunVideo("pad", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Pad>(logger, 1920u, 1080u);
	});
}

int test_filter_scale() {
	return RunVideo("scale", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::Scale>(logger, 320u, 180u);
	});
}

int test_filter_vaguedenoiser() {
	return RunVideo("vaguedenoiser", [](auto& track, auto& logger) {
		track.template Filter<Filter::Video::VagueDenoiser>(logger);
	});
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
