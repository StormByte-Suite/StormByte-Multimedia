#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <cstddef>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	constexpr unsigned char LogoBytes[] = {
#embed "../../files/logo/watermark-noise-logo.png"
	};

	int CheckWatermarkOutput(bool binaryInput, const std::filesystem::path& output) {
		auto codec = Registry::Instance().FindCodec("VP9");
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		{
			QuietCout quietCout;
			auto logger = MakeSilentLogger();
			Transcoder job{FixturePath("video/bluray_like_hdr10.mp4"), output, logger, 2000000000LL};
			auto track = job.Video(0);
			track.Codec(codec.value().get())
				.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"})
				.BitRate(1500000);
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
			options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
			options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
			track.FineTune(std::move(options));
			track.Filter<Filter::Video::Scale>(logger, 640u, 360u);
			if (binaryInput) {
				StormByte::Safe::Binary logo;
				const auto* bytes = reinterpret_cast<const std::byte*>(LogoBytes);
				logo.assign(bytes, bytes + sizeof(LogoBytes));
				track.Filter<Filter::Video::Watermark>(logger, logo, Filter::Video::Anchor::TopRight, 30u, 16);
			}
			else {
				const auto logo = FixturePath("logo/watermark-noise-logo.png");
				track.Filter<Filter::Video::Watermark>(logger, StormByte::Safe::String{logo.string()},
					Filter::Video::Anchor::TopRight, 30u, 16);
			}
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			const int result = WaitForOptionalCodec(job);
			if (result != 0)
				return result;
		}
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == "WebM");
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(stream.Codec().Name() == "VP9");
		const auto video = stream.Video();
		TEST_REQUIRE(video);
		TEST_REQUIRE(video.value().Resolution().Width() == 640);
		TEST_REQUIRE(video.value().Resolution().Height() == 360);
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		std::cout << "[WATERMARK " << (binaryInput ? "binary" : "path") << "] " << output.string() << std::endl;
		return 0;
	}
}

int test_watermark_path_and_embedded_binary() {
	const int pathResult = CheckWatermarkOutput(false, OutputPath("pipeline/watermark/watermark-logo-path.webm"));
	if (pathResult != 0)
		return pathResult;
	return CheckWatermarkOutput(true, OutputPath("pipeline/watermark/watermark-logo-binary.webm"));
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_watermark_path_and_embedded_binary", test_watermark_path_and_embedded_binary},
	};
	return RunSelectedTest(argc, argv, tests);
}
