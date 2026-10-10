#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	int CheckVideoOutput(const std::filesystem::path& output, std::string_view containerName,
		std::string_view codecName) {
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
		std::string_view decoder = {}) {
		auto source = File::Open(StormByte::Safe::String{input.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(!source.value().Streams().empty());
		TEST_REQUIRE(source.value().Streams()[0].Type() == StormByte::Multimedia::Type::Video);
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		QuietCout quietCout;
		auto logger = MakeSilentLogger();
		Transcoder job{input, output, logger, 2000000000LL};
		auto track = job.Video(0);
		track.Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder});
		if (!decoder.empty())
			track.Implementation(ImplementationSide::Decoder, StormByte::Safe::String{decoder});
		track.Filter<Filter::Video::Scale>(logger, 320u, 192u);
		if (encoder == "libsvtav1")
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
		job.Run();
		const int result = WaitForOptionalCodec(job);
		if (result != 0)
			return result;
		return CheckVideoOutput(output, containerName, codecName);
	}

	int CheckVideoEncode(std::string_view destination, std::string_view containerName,
		std::string_view codecName, std::string_view encoder) {
		return CheckVideoCodec(FixturePath("video/bluray_like_hdr10.mp4"), OutputPath(destination),
			containerName, codecName, encoder);
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

int main(int argc, char** argv) {
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
	};
	return RunSelectedTest(argc, argv, tests);
}
