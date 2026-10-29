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
#include <StormByte/multimedia/pipeline/filters/video/tonemap.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
	int CheckEncodedVideoOutput(const std::filesystem::path& output, std::string_view expectedCodec) {
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& result = opened.value();
		TEST_REQUIRE(result.Streams().size() == 1);
		const auto video = result.Streams()[0];
		TEST_REQUIRE(video.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(video.Codec().Name() == expectedCodec);
		const auto properties = video.Video();
		TEST_REQUIRE(properties);
		TEST_REQUIRE(properties.value().Resolution().Width() == 1920);
		TEST_REQUIRE(properties.value().Resolution().Height() == 1080);
		return 0;
	}

	bool SamePoint(const StormByte::Multimedia::Property::Point& left,
		const StormByte::Multimedia::Property::Point& right) {
		return left.X() == right.X() && left.Y() == right.Y();
	}

	int CheckHdr10EncodeOutput(const std::filesystem::path& output, std::string_view expectedCodec) {
		const auto sourcePath = FixturePath("video/hdr10_metadata_source.mkv");
		TEST_PHASE("opening media file for inspection");
		auto source = File::Open(StormByte::Safe::String{sourcePath.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(source.value().Streams().size() == 1);
		const auto sourceVideo = source.value().Streams()[0].Video();
		TEST_REQUIRE(sourceVideo && sourceVideo.value().HDR10());
		TEST_REQUIRE(sourceVideo.value().HDR10()->Origin() == StormByte::Multimedia::Property::HDR10::Source::Metadata);

		TEST_PHASE("opening media file for inspection");
		auto encoded = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(encoded);
		TEST_REQUIRE(encoded.value().Container().Name() == "Matroska");
		TEST_REQUIRE(encoded.value().Streams().size() == 1);
		const auto stream = encoded.value().Streams()[0];
		TEST_REQUIRE(stream.Codec().Name() == expectedCodec);
		const auto encodedVideo = stream.Video();
		TEST_REQUIRE(encodedVideo && encodedVideo.value().HDR10());
		TEST_REQUIRE(encodedVideo.value().Resolution().Width() == sourceVideo.value().Resolution().Width());
		TEST_REQUIRE(encodedVideo.value().Resolution().Height() == sourceVideo.value().Resolution().Height());
		TEST_REQUIRE(encodedVideo.value().Color().PixelFormat() == sourceVideo.value().Color().PixelFormat());
		TEST_REQUIRE(encodedVideo.value().Color().Space() == sourceVideo.value().Color().Space());
		TEST_REQUIRE(encodedVideo.value().Color().Primaries() == sourceVideo.value().Color().Primaries());
		TEST_REQUIRE(encodedVideo.value().Color().Transfer() == sourceVideo.value().Color().Transfer());
		const auto& before = sourceVideo.value().HDR10().value();
		const auto& after = encodedVideo.value().HDR10().value();
		TEST_REQUIRE(after.Origin() == StormByte::Multimedia::Property::HDR10::Source::Metadata);
		TEST_REQUIRE(SamePoint(after.Red(), before.Red()));
		TEST_REQUIRE(SamePoint(after.Green(), before.Green()));
		TEST_REQUIRE(SamePoint(after.Blue(), before.Blue()));
		TEST_REQUIRE(SamePoint(after.White(), before.White()));
		TEST_REQUIRE(SamePoint(after.Luminance(), before.Luminance()));
		TEST_REQUIRE(after.LightLevel().has_value() == before.LightLevel().has_value());
		if (before.LightLevel())
			TEST_REQUIRE(SamePoint(after.LightLevel().value(), before.LightLevel().value()));
		return 0;
	}

	int CheckHdr10Encode(std::string_view destination, std::string_view codecName, std::string_view encoder) {
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;

		const std::string implementation{encoder};
		if (const auto* concrete = avcodec_find_encoder_by_name(implementation.c_str());
			concrete && avcodec_get_hw_config(concrete, 0)) {
			auto source = File::Open(TestLocation(FixturePath("video/hdr10_metadata_source.mkv")));
			TEST_REQUIRE(source && source.value().Streams().size() == 1);
			const auto video = source.value().Streams()[0].Video();
			TEST_REQUIRE(video);
			if (const int result = CheckHardwareEncoderCapability(concrete, video.value().Resolution().Width(),
				video.value().Resolution().Height()); result != 0)
				return result;
		}

		const auto output = OutputPath(destination);
		auto logger = MakeLogger();
		TEST_PHASE("creating transcoder");
		Transcoder job{TestLocation(FixturePath("video/hdr10_metadata_source.mkv")), TestLocation(output), logger, 2000000000LL};
		auto track = job.Video(0);
		track.Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder});
		if (encoder == "libx265")
			track.Preset(StormByte::Safe::String{"ultrafast"}).CRF(30);
		else if (encoder == "libsvtav1")
			track.Preset(StormByte::Safe::String{"12"}).CRF(40);
		else if (encoder == "libaom-av1" || encoder == "libvpx-vp9") {
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
			options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
			if (encoder == "libvpx-vp9")
				options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
			track.FineTune(std::move(options)).BitRate(500000);
		}
		else
			track.BitRate(500000);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		if (const int result = WaitForOptionalCodec(job); result != 0)
			return result;
		return CheckHdr10EncodeOutput(output, codecName);
	}

	class UnenforcedPlan final: public Plan {
		public:
			UnenforcedPlan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
				StormByte::Safe::Optional<std::int64_t> duration,
				const StormByte::Safe::Function<void(double)>* progress) noexcept
			: Plan(std::move(reader), std::move(writer), std::move(duration), progress) {
				Policies(~Policy::EnforceFeatures);
			}

			UnenforcedPlan(StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
				const class Container& container,
				StormByte::Safe::Optional<std::int64_t> duration,
				const StormByte::Safe::Function<void(double)>* progress) noexcept
			: Plan(std::move(reader), std::move(writer), container, std::move(duration), progress) {
				Policies(~Policy::EnforceFeatures);
			}
	};

	class UnenforcedJob final: public Transcoder {
		public:
			using Transcoder::Transcoder;

			StormByte::Safe::Shared<class Plan> EmptyPlan(
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader> reader,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter> writer,
				StormByte::Safe::Optional<std::int64_t> duration,
				const class Container* container) const noexcept override {
				const auto progress = DurationProgress();
				if (container)
					return StormByte::Safe::Shared<class Plan>::MakePointer<UnenforcedPlan>(
						std::move(reader), std::move(writer), *container, std::move(duration), &progress);
				return StormByte::Safe::Shared<class Plan>::MakePointer<UnenforcedPlan>(
					std::move(reader), std::move(writer), std::move(duration), &progress);
			}
	};
}

int test_transcoder_hevc_encode_output_file_properties() {
	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	TEST_REQUIRE(hevc);
	if (!hevc.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-hevc.mkv");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/hdr10_metadata_source.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Video(0).Codec(hevc.value().get())
		.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libx265"})
		.Preset(StormByte::Safe::String{"ultrafast"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	if (const int result = WaitForOptionalCodec(job); result != 0)
		return result;
	return CheckEncodedVideoOutput(output, "H.265");
}

int test_transcoder_kvazaar_pads_unaligned_video() {
	if (!avcodec_find_encoder_by_name("libkvazaar"))
		return TEST_SKIP;
	const auto codec = Registry::Instance().FindCodec("H.265");
	TEST_REQUIRE(codec);
	const auto input = FixturePath("video/kvazaar_unaligned_704x396.avi");
	const auto output = OutputPath("pipeline/kvazaar-padded.mkv");
	auto source = File::Open(StormByte::Safe::String{input.string()});
	TEST_REQUIRE(source && source->Streams().size() == 1);
	const auto original = source->Streams()[0].Video();
	TEST_REQUIRE(original);
	TEST_REQUIRE(original->Resolution().Width() == 704 && original->Resolution().Height() == 396);
	auto logger = MakeLogger();
	Transcoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
	job.Resources(ResourceUsage::Low);
	job.Video(0).Codec(codec->get())
		.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libkvazaar"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded && encoded->Streams().size() == 1);
	TEST_REQUIRE(encoded->Streams()[0].Codec().Name() == "H.265");
	const auto video = encoded->Streams()[0].Video();
	TEST_REQUIRE(video);
	TEST_REQUIRE(video->Resolution().Width() == 704 && video->Resolution().Height() == 400);
	TEST_REQUIRE(video->SampleAspectRatio() == original->SampleAspectRatio());
	return 0;
}

int test_transcoder_hevc_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-hevc-hdr10.mkv", "H.265", "libx265");
}

int test_transcoder_svt_av1_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-svt-av1-hdr10.mkv", "AV1", "libsvtav1");
}

int test_transcoder_aom_av1_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-aom-av1-hdr10.mkv", "AV1", "libaom-av1");
}

int test_transcoder_vp9_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-vp9-hdr10.mkv", "VP9", "libvpx-vp9");
}

int test_transcoder_hevc_nvenc_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-hevc-nvenc-hdr10.mkv", "H.265", "hevc_nvenc");
}

int test_transcoder_hevc_qsv_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-hevc-qsv-hdr10.mkv", "H.265", "hevc_qsv");
}

int test_transcoder_hevc_vaapi_encode_preserves_hdr10_metadata() {
	return CheckHdr10Encode("pipeline/encoded-hevc-vaapi-hdr10.mkv", "H.265", "hevc_vaapi");
}

int test_transcoder_enforce_features_rejects_hdr10_loss() {
	auto vp8 = Registry::Instance().FindCodec("VP8");
	TEST_REQUIRE(vp8);
	if (!vp8.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	const auto output = OutputPath("pipeline/enforce-features-reject-hdr10.mkv");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/hdr10_metadata_source.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Video(0).Codec(vp8.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	return WaitForTranscoderFailure(job, "drops source features");
}

int test_transcoder_without_enforce_features_drops_hdr10() {
	auto h264 = Registry::Instance().FindCodec("H.264");
	TEST_REQUIRE(h264);
	if (!h264.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto sourcePath = FixturePath("video/hdr10_metadata_source.mkv");
	const auto output = OutputPath("pipeline/hdr10-dropped-h264.mkv");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	UnenforcedJob job{TestLocation(sourcePath), TestLocation(output), logger, 2000000000LL};
	job.Video(0).Codec(h264.value().get())
		.Implementation(StormByte::Safe::String{"libx264"})
		.Preset(StormByte::Safe::String{"ultrafast"}).CRF(30)
		.Filter<Filter::Video::Tonemap>(logger);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	if (const int waited = WaitForOptionalCodec(job); waited != 0)
		return waited;
	TEST_REQUIRE(job.Plan());
	TEST_REQUIRE(!job.Plan()->Policies().Has(Plan::Policy::EnforceFeatures));

	TEST_PHASE("opening media file for inspection");
	auto source = File::Open(StormByte::Safe::String{sourcePath.string()});
	TEST_REQUIRE(source);
	const auto sourceVideo = source.value().Streams()[0].Video();
	TEST_REQUIRE(sourceVideo && sourceVideo.value().HDR10());
	TEST_REQUIRE(sourceVideo.value().HDR10()->Origin() == StormByte::Multimedia::Property::HDR10::Source::Metadata);

	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded);
	TEST_REQUIRE(encoded.value().Streams().size() == 1);
	const auto stream = encoded.value().Streams()[0];
	TEST_REQUIRE(stream.Codec().Name() == "H.264");
	const auto encodedVideo = stream.Video();
	TEST_REQUIRE(encodedVideo);
	TEST_REQUIRE(!encodedVideo.value().HDR10());
	const auto& color = encodedVideo.value().Color();
	TEST_REQUIRE(color.PixelFormat() == Property::PixelFormat::YUV420P);
	TEST_REQUIRE(color.Range() == Property::Range::TV);
	TEST_REQUIRE(color.Space() == Property::Space::BT709);
	TEST_REQUIRE(color.Primaries() == Property::Primaries::BT709);
	TEST_REQUIRE(color.Transfer() == Property::Transfer::BT709);
	return 0;
}

int RunPipelineVideoTests(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_kvazaar_pads_unaligned_video", test_transcoder_kvazaar_pads_unaligned_video},
		TestEntry{"test_transcoder_hevc_encode_output_file_properties", test_transcoder_hevc_encode_output_file_properties},
		TestEntry{"test_transcoder_hevc_encode_preserves_hdr10_metadata", test_transcoder_hevc_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_svt_av1_encode_preserves_hdr10_metadata", test_transcoder_svt_av1_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_aom_av1_encode_preserves_hdr10_metadata", test_transcoder_aom_av1_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_vp9_encode_preserves_hdr10_metadata", test_transcoder_vp9_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_hevc_nvenc_encode_preserves_hdr10_metadata", test_transcoder_hevc_nvenc_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_hevc_qsv_encode_preserves_hdr10_metadata", test_transcoder_hevc_qsv_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_hevc_vaapi_encode_preserves_hdr10_metadata", test_transcoder_hevc_vaapi_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_enforce_features_rejects_hdr10_loss", test_transcoder_enforce_features_rejects_hdr10_loss},
		TestEntry{"test_transcoder_without_enforce_features_drops_hdr10", test_transcoder_without_enforce_features_drops_hdr10},
	};
	return RunSelectedTest(argc, argv, tests);
}
