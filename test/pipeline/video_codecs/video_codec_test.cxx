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
#include "../hardware_capability.hxx"
#include "runners.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/filters/video/format.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <tables/decoder/table.hxx>
#include <tables/encoder/table.hxx>

#include <libavutil/pixfmt.h>

extern "C" {
	#include <libavformat/avformat.h>
}

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	int CheckVideoOutput(const std::filesystem::path& output, std::string_view containerName,
		std::string_view codecName) {
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == containerName);
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Index() == 0);
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(stream.Codec().Name() == codecName);
		const auto video = stream.Video();
		TEST_REQUIRE(video);
		TEST_REQUIRE(video.value().Resolution().Width() == 320);
		TEST_REQUIRE(video.value().Resolution().Height() == 192);
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}

	int CheckVideoCodec(const std::filesystem::path& input, const std::filesystem::path& output,
		std::string_view containerName, std::string_view codecName, std::string_view encoder,
		std::string_view decoder = {}, AVPixelFormat sourceFormat = AV_PIX_FMT_NONE) {
		TEST_PHASE("opening media file for inspection");
		auto source = File::Open(StormByte::Safe::String{input.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(!source.value().Streams().empty());
		TEST_REQUIRE(source.value().Streams()[0].Type() == StormByte::Multimedia::Type::Video);
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		if (sourceFormat != AV_PIX_FMT_NONE) {
			const auto* concrete = avcodec_find_encoder_by_name(std::string{encoder}.c_str());
			if (!concrete)
				return TEST_SKIP;
			if (const int result = CheckHardwareEncoderCapability(concrete, 320, 192, sourceFormat); result != 0)
				return result;
		}
		auto logger = MakeLogger();
		{
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
			auto track = job.Video(0);
			track.Codec(codec.value().get())
				.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder});
			if (!decoder.empty())
				track.Implementation(ImplementationSide::Decoder, StormByte::Safe::String{decoder});
			track.Filter<Filter::Video::Scale>(logger, 320u, 192u);
			if (sourceFormat != AV_PIX_FMT_NONE)
				track.Filter<Filter::Video::Format>(logger, sourceFormat);
			if (encoder == "libwebp" || encoder == "libwebp_anim")
				track.Filter<Filter::Video::Format>(logger, AV_PIX_FMT_YUV420P);
			else if (encoder == "libsvtav1")
				track.Preset(StormByte::Safe::String{"12"}).CRF(40);
			else if (encoder == "libx264" || encoder == "libx265")
				track.Preset(StormByte::Safe::String{"ultrafast"}).CRF(30);
			else if (encoder == "libaom-av1" || encoder == "libvpx" || encoder == "libvpx-vp9") {
				StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
				options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
				if (encoder != "libaom-av1")
					options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
				track.FineTune(std::move(options)).BitRate(500000);
			}
			else
				track.BitRate(500000);
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("starting transcoder run");
			job.Run();
			const int result = WaitForOptionalCodec(job);
			if (result != 0)
				return result;
		}
		return CheckVideoOutput(output, containerName, codecName);
	}

	int CheckVideoEncode(std::string_view destination, std::string_view containerName,
		std::string_view codecName, std::string_view encoder) {
		return CheckVideoCodec(FixturePath("video/bluray_like_hdr10.mp4"), OutputPath(destination),
			containerName, codecName, encoder);
	}

	int CheckNvencHevcEncode(std::string_view source, std::string_view destination,
		AVPixelFormat sourceFormat, int expectedProfile) {
		const auto* encoder = avcodec_find_encoder_by_name("hevc_nvenc");
		if (!encoder)
			return TEST_SKIP;
		if (const int result = CheckHardwareEncoderCapability(encoder, 320, 192, sourceFormat); result != 0)
			return result;
		const auto output = OutputPath(destination);
		if (const int result = CheckVideoCodec(FixturePath(source), output, "Matroska", "H.265",
			"hevc_nvenc"); result != 0)
			return result;

		AVFormatContext* native = nullptr;
		const auto closeInput = [](AVFormatContext* context) { avformat_close_input(&context); };
		const int opened = avformat_open_input(&native, output.string().c_str(), nullptr, nullptr);
		std::unique_ptr<AVFormatContext, decltype(closeInput)> inspected(native, closeInput);
		TEST_REQUIRE(opened >= 0);
		TEST_REQUIRE(avformat_find_stream_info(inspected.get(), nullptr) >= 0);
		TEST_REQUIRE(inspected->nb_streams == 1);
		TEST_REQUIRE(inspected->streams[0]->codecpar->codec_id == AV_CODEC_ID_HEVC);
		TEST_REQUIRE(inspected->streams[0]->codecpar->profile == expectedProfile);
		return 0;
	}

	int CheckAv1Decode(std::string_view decoder, std::string_view encodedName, std::string_view decodedName) {
		const auto encoded = OutputPath(encodedName);
		const int result = CheckVideoCodec(FixturePath("video/bluray_like_hdr10.mp4"), encoded,
			"Matroska", "AV1", "libsvtav1");
		if (result != 0)
			return result;
		return CheckVideoCodec(encoded, OutputPath(decodedName), "WebM", "VP9", "libvpx-vp9", decoder);
	}
}

int test_svt_av1_encodes_matroska() {
	return CheckVideoEncode("pipeline/video-codecs/svt-av1.mkv", "Matroska", "AV1", "libsvtav1");
}

int test_svt_av1_encodes_mp4() {
	return CheckVideoEncode("pipeline/video-codecs/svt-av1.mp4", "MP4", "AV1", "libsvtav1");
}

int test_aom_av1_encodes_webm() {
	return CheckVideoEncode("pipeline/video-codecs/aom-av1.webm", "WebM", "AV1", "libaom-av1");
}

int test_dav1d_decodes_svt_av1() {
	return CheckAv1Decode("libdav1d", "pipeline/video-codecs/dav1d-source.mkv",
		"pipeline/video-codecs/dav1d-decoded.webm");
}

int test_aom_decodes_svt_av1() {
	return CheckAv1Decode("libaom-av1", "pipeline/video-codecs/aom-source.mkv",
		"pipeline/video-codecs/aom-decoded.webm");
}

int test_libvpx_encodes_vp8_webm() {
	return CheckVideoEncode("pipeline/video-codecs/vp8.webm", "WebM", "VP8", "libvpx");
}

int test_libvpx_encodes_vp9_webm() {
	return CheckVideoEncode("pipeline/video-codecs/vp9.webm", "WebM", "VP9", "libvpx-vp9");
}

int test_x264_encodes_h264_mp4() {
	return CheckVideoEncode("pipeline/video-codecs/x264.mp4", "MP4", "H.264", "libx264");
}

int test_openh264_encodes_h264_matroska() {
	return CheckVideoEncode("pipeline/video-codecs/openh264.mkv", "Matroska", "H.264", "libopenh264");
}

int test_kvazaar_encodes_hevc_matroska() {
	return CheckVideoEncode("pipeline/video-codecs/kvazaar.mkv", "Matroska", "H.265", "libkvazaar");
}

int test_x265_encodes_hevc_matroska() {
	return CheckVideoEncode("pipeline/video-codecs/x265.mkv", "Matroska", "H.265", "libx265");
}

int test_nvenc_feature_contract() {
	const auto& encoders = Tables::Encoder::Video();
	const auto findEncoder = [&encoders](std::string_view name) {
		return std::find_if(encoders.begin(), encoders.end(), [name](const auto& row) {
			return name == row.name;
		});
	};
	const auto h264 = findEncoder("h264_nvenc");
	const auto hevc = findEncoder("hevc_nvenc");
	const auto av1 = findEncoder("av1_nvenc");
	TEST_REQUIRE(h264 != encoders.end());
	TEST_REQUIRE(hevc != encoders.end());
	TEST_REQUIRE(av1 != encoders.end());
	TEST_REQUIRE(!h264->features.Has(Feature::TenBit));
	TEST_REQUIRE(!h264->features.Has(Feature::HDR10));
	TEST_REQUIRE(hevc->features.Has(Feature::TenBit));
	TEST_REQUIRE(hevc->features.Has(Feature::HDR10));
	TEST_REQUIRE(!hevc->features.Has(Feature::HDR10Plus));
	TEST_REQUIRE(!hevc->features.Has(Feature::DOVI));
	TEST_REQUIRE(av1->features.Has(Feature::TenBit));
	TEST_REQUIRE(av1->features.Has(Feature::HDR10));
	TEST_REQUIRE(!av1->features.Has(Feature::HDR10Plus));
	TEST_REQUIRE(!av1->features.Has(Feature::DOVI));
	for (const auto& row : Tables::Decoder::Video()) {
		TEST_REQUIRE(std::string_view{row.name}.find("_nvdec") == std::string_view::npos);
		TEST_REQUIRE(std::string_view{row.name}.find("_cuvid") == std::string_view::npos);
	}
	return 0;
}

int test_hevc_feature_access_matches_available_encoders() {
	auto hevc = Registry::Instance().FindCodec("H.265");
	auto alias = Registry::Instance().FindCodec("hevc");
	TEST_REQUIRE(hevc);
	TEST_REQUIRE(alias);
	const auto available = [](const char* name) {
		return avcodec_find_encoder_by_name(name) != nullptr;
	};
	const bool x265 = available("libx265");
	const bool hdr10 = x265 || available("hevc_nvenc") || available("hevc_qsv")
		|| available("hevc_vaapi");
	const auto& codec = hevc->get();
	TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::HDR10) == hdr10);
	TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::HDR10 | Feature::TenBit) == hdr10);
	TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::DOVI) == x265);
	TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::HDR10Plus) == x265);
	TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::HDR10 | Feature::DOVI) == x265);
	TEST_REQUIRE(alias->get().HasAccess(Operation::Write, Feature::HDR10) == hdr10);
	if (available("libkvazaar")) {
		TEST_REQUIRE(codec.HasAccess(Operation::Write));
		TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::None));
		TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::MultiThreaded));
	}
	std::cout << "HEVC availability: kvazaar=" << available("libkvazaar")
		<< ", x265=" << x265 << ", HDR10 encoder=" << hdr10 << '\n';
	return 0;
}

int test_feature_access_checks_each_direction_independently() {
	auto hevc = Registry::Instance().FindCodec("H.265");
	auto eac3 = Registry::Instance().FindCodec("E-AC3");
	TEST_REQUIRE(hevc);
	TEST_REQUIRE(eac3);
	const bool nativeHevc = avcodec_find_decoder_by_name("hevc") != nullptr;
	const bool hdrDecoder = nativeHevc || avcodec_find_decoder_by_name("hevc_qsv") != nullptr
		|| avcodec_find_decoder_by_name("hevc_vaapi") != nullptr;
	TEST_REQUIRE(hevc->get().HasAccess(Operation::Read, Feature::HDR10) == hdrDecoder);
	TEST_REQUIRE(hevc->get().HasAccess(Operation::Read, Feature::DOVI) == nativeHevc);
	TEST_REQUIRE(hevc->get().HasAccess(Operation::Read, Feature::HDR10 | Feature::DOVI) == nativeHevc);
	TEST_REQUIRE(eac3->get().HasAccess(Operation::Read, Feature::SurroundSound)
		== (avcodec_find_decoder_by_name("eac3") != nullptr));
	TEST_REQUIRE(eac3->get().HasAccess(Operation::Write, Feature::SurroundSound)
		== (avcodec_find_encoder_by_name("eac3") != nullptr));
	for (const auto ref : {hevc.value(), eac3.value()}) {
		const auto& codec = ref.get();
		for (const auto need : {Features{Feature::HDR10}, Features{Feature::DOVI},
			Features{Feature::SurroundSound}, Features{Feature::HDR10 | Feature::TenBit}}) {
			TEST_REQUIRE(codec.HasAccess(Operation::Read | Operation::Write, need)
				== (codec.HasAccess(Operation::Read, need) && codec.HasAccess(Operation::Write, need)));
		}
	}
	return 0;
}

int test_nvenc_encodes_h264_mp4() {
	const auto* encoder = avcodec_find_encoder_by_name("h264_nvenc");
	if (!encoder)
		return TEST_SKIP;
	if (const int result = CheckHardwareEncoderCapability(encoder, 320, 192, AV_PIX_FMT_NV12); result != 0)
		return result;
	return CheckVideoCodec(FixturePath("video/xvid_mp3_stereo.avi"), OutputPath("pipeline/video-codecs/nvenc-h264.mp4"),
		"MP4", "H.264", "h264_nvenc");
}

int test_nvenc_encodes_h264_10bit_matroska() {
	return CheckVideoCodec(FixturePath("video/bluray_like_hdr10.mp4"), OutputPath("pipeline/video-codecs/nvenc-h264-10bit.mkv"),
		"Matroska", "H.264", "h264_nvenc", {}, AV_PIX_FMT_YUV420P10LE);
}

int test_nvenc_encodes_hevc_main_matroska() {
	return CheckNvencHevcEncode("video/xvid_mp3_stereo.avi", "pipeline/video-codecs/nvenc-hevc-main.mkv",
		AV_PIX_FMT_NV12, AV_PROFILE_HEVC_MAIN);
}

int test_nvenc_encodes_hevc_main10_matroska() {
	return CheckNvencHevcEncode("video/hdr10_metadata_source.mkv", "pipeline/video-codecs/nvenc-hevc-main10.mkv",
		AV_PIX_FMT_YUV420P10LE, AV_PROFILE_HEVC_MAIN_10);
}

int test_nvenc_encodes_av1_matroska() {
	return CheckVideoCodec(FixturePath("video/bluray_like_hdr10.mp4"), OutputPath("pipeline/video-codecs/nvenc-av1.mkv"),
		"Matroska", "AV1", "av1_nvenc", {}, AV_PIX_FMT_YUV420P10LE);
}

int test_vaapi_encodes_hevc_main_matroska() {
	const auto input = FixturePath("video/xvid_mp3_stereo.avi");
	auto source = File::Open(TestLocation(input));
	TEST_REQUIRE(source && !source.value().Streams().empty());
	const auto sourceVideo = source.value().Streams()[0].Video();
	TEST_REQUIRE(sourceVideo && !sourceVideo.value().HDR10());
	TEST_REQUIRE(sourceVideo.value().Color().PixelFormat() == Property::PixelFormat::YUV420P);
	const auto output = OutputPath("pipeline/video-codecs/vaapi-hevc-main.mkv");
	if (const int result = CheckVideoCodec(input, output, "Matroska", "H.265", "hevc_vaapi", {},
		AV_PIX_FMT_NV12); result != 0)
		return result;
	auto encoded = File::Open(TestLocation(output));
	TEST_REQUIRE(encoded && encoded.value().Streams().size() == 1);
	const auto encodedVideo = encoded.value().Streams()[0].Video();
	TEST_REQUIRE(encodedVideo && !encodedVideo.value().HDR10());
	TEST_REQUIRE(encodedVideo.value().Color().PixelFormat() == Property::PixelFormat::YUV420P);
	AVFormatContext* native = nullptr;
	const auto closeInput = [](AVFormatContext* context) { avformat_close_input(&context); };
	const int opened = avformat_open_input(&native, output.string().c_str(), nullptr, nullptr);
	std::unique_ptr<AVFormatContext, decltype(closeInput)> inspected(native, closeInput);
	TEST_REQUIRE(opened >= 0);
	TEST_REQUIRE(avformat_find_stream_info(inspected.get(), nullptr) >= 0);
	TEST_REQUIRE(inspected->nb_streams == 1);
	TEST_REQUIRE(inspected->streams[0]->codecpar->profile == AV_PROFILE_HEVC_MAIN);
	return 0;
}

int test_libwebp_anim_encodes_webp() {
	return CheckVideoEncode("pipeline/video-codecs/webp-anim.webp", "WebP", "WebP", "libwebp_anim");
}

int test_libwebp_decodes_animated_webp() {
	const auto encoded = OutputPath("pipeline/video-codecs/webp-decode-source.webp");
	const int result = CheckVideoCodec(FixturePath("video/bluray_like_hdr10.mp4"), encoded,
		"WebP", "WebP", "libwebp_anim");
	if (result != 0)
		return result;
	return CheckVideoCodec(encoded, OutputPath("pipeline/video-codecs/webp-decoded.webm"),
		"WebM", "VP9", "libvpx-vp9", "webp");
}

int RunPipelineVideoCodecTests(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_svt_av1_encodes_matroska", test_svt_av1_encodes_matroska},
		TestEntry{"test_svt_av1_encodes_mp4", test_svt_av1_encodes_mp4},
		TestEntry{"test_aom_av1_encodes_webm", test_aom_av1_encodes_webm},
		TestEntry{"test_dav1d_decodes_svt_av1", test_dav1d_decodes_svt_av1},
		TestEntry{"test_aom_decodes_svt_av1", test_aom_decodes_svt_av1},
		TestEntry{"test_libvpx_encodes_vp8_webm", test_libvpx_encodes_vp8_webm},
		TestEntry{"test_libvpx_encodes_vp9_webm", test_libvpx_encodes_vp9_webm},
		TestEntry{"test_x264_encodes_h264_mp4", test_x264_encodes_h264_mp4},
		TestEntry{"test_openh264_encodes_h264_matroska", test_openh264_encodes_h264_matroska},
		TestEntry{"test_kvazaar_encodes_hevc_matroska", test_kvazaar_encodes_hevc_matroska},
		TestEntry{"test_x265_encodes_hevc_matroska", test_x265_encodes_hevc_matroska},
		TestEntry{"test_nvenc_feature_contract", test_nvenc_feature_contract},
		TestEntry{"test_hevc_feature_access_matches_available_encoders", test_hevc_feature_access_matches_available_encoders},
		TestEntry{"test_feature_access_checks_each_direction_independently", test_feature_access_checks_each_direction_independently},
		TestEntry{"test_nvenc_encodes_h264_mp4", test_nvenc_encodes_h264_mp4},
		TestEntry{"test_nvenc_encodes_h264_10bit_matroska", test_nvenc_encodes_h264_10bit_matroska},
		TestEntry{"test_nvenc_encodes_hevc_main_matroska", test_nvenc_encodes_hevc_main_matroska},
		TestEntry{"test_nvenc_encodes_hevc_main10_matroska", test_nvenc_encodes_hevc_main10_matroska},
		TestEntry{"test_nvenc_encodes_av1_matroska", test_nvenc_encodes_av1_matroska},
		TestEntry{"test_vaapi_encodes_hevc_main_matroska", test_vaapi_encodes_hevc_main_matroska},
		TestEntry{"test_libwebp_anim_encodes_webp", test_libwebp_anim_encodes_webp},
		TestEntry{"test_libwebp_decodes_animated_webp", test_libwebp_decodes_animated_webp},
	};
	return RunSelectedTest(argc, argv, tests);
}
