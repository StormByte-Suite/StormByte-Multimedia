#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
	int CheckEncodedVideoOutput(const std::filesystem::path& output, std::string_view expectedCodec) {
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
	auto source = File::Open(StormByte::Safe::String{sourcePath.string()});
	TEST_REQUIRE(source);
	TEST_REQUIRE(source.value().Streams().size() == 1);
	const auto sourceVideo = source.value().Streams()[0].Video();
	TEST_REQUIRE(sourceVideo && sourceVideo.value().HDR10());
	TEST_REQUIRE(sourceVideo.value().HDR10()->Origin() == StormByte::Multimedia::Property::HDR10::Source::Metadata);

	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded);
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
}

int test_transcoder_hevc_encode_output_file_properties() {
	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	TEST_REQUIRE(hevc);
	if (!hevc.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-hevc.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/hdr10_metadata_source.mkv"), output, logger, 2000000000LL};
	job.Video(0).Codec(hevc.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckEncodedVideoOutput(output, "H.265");
}

int test_transcoder_hevc_encode_preserves_hdr10_metadata() {
	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	TEST_REQUIRE(hevc);
	if (!hevc.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-hevc-hdr10.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/hdr10_metadata_source.mkv"), output, logger, 2000000000LL};
	job.Video(0).Codec(hevc.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckHdr10EncodeOutput(output, "H.265");
}

int test_transcoder_vp9_encode_preserves_hdr10_metadata() {
	auto& registry = Registry::Instance();
	auto vp9 = registry.FindCodec("VP9");
	TEST_REQUIRE(vp9);
	if (!vp9.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-vp9-hdr10.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/hdr10_metadata_source.mkv"), output, logger, 2000000000LL};
	job.Video(0).Codec(vp9.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckHdr10EncodeOutput(output, "VP9");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_hevc_encode_output_file_properties", test_transcoder_hevc_encode_output_file_properties},
		TestEntry{"test_transcoder_hevc_encode_preserves_hdr10_metadata", test_transcoder_hevc_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_vp9_encode_preserves_hdr10_metadata", test_transcoder_vp9_encode_preserves_hdr10_metadata},
	};
	return RunSelectedTest(argc, argv, tests);
}
