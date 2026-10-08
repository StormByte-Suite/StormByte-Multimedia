#include "../helpers.hxx"

#include <StormByte/multimedia/pipeline/filters/analytics/vmaf.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <array>
#include <string>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
static int CheckVmafRemux(std::string_view source, std::string_view destination) {
	const auto output = OutputPath(destination);
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath(source), output, logger, 2000000000LL};
	job.Video(0).Remux();
	job.Filter<StormByte::Multimedia::Pipeline::Filter::Video::VMAF>(
		logger, StormByte::Safe::String{"vmaf_4k_v0.6.1"}, static_cast<unsigned short>(2));
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	auto reports = job.Reports();
	TEST_REQUIRE(reports.size() == 1);
	const auto report = reports[0].second;
	TEST_REQUIRE(report.Kind() == Filter::Report::Status::Ok);
	const auto& data = report.Data();
	TEST_REQUIRE(data.contains(StormByte::Safe::String{"vmaf_mean"}));
	TEST_REQUIRE(data.contains(StormByte::Safe::String{"vmaf_min"}));
	TEST_REQUIRE(data.contains(StormByte::Safe::String{"frames"}));
	const auto score = std::stod(std::string{TestView(data.at(StormByte::Safe::String{"vmaf_mean"}))});
	const auto minimum = std::stod(std::string{TestView(data.at(StormByte::Safe::String{"vmaf_min"}))});
	const auto frames = std::stoul(std::string{TestView(data.at(StormByte::Safe::String{"frames"}))});
	TEST_REQUIRE(score == 100.0);
	TEST_REQUIRE(minimum == 100.0);
	TEST_REQUIRE(frames == 48);
	return 0;
}
}

int test_transcoder_vmaf_remux_matches_source_at_100() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-remux.mkv");
}

int test_transcoder_vmaf_hevc_remux_matches_source_at_100() {
	return CheckVmafRemux("video/anime_like.mkv", "pipeline/vmaf-hevc-remux.mkv");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_vmaf_remux_matches_source_at_100", test_transcoder_vmaf_remux_matches_source_at_100},
		TestEntry{"test_transcoder_vmaf_hevc_remux_matches_source_at_100", test_transcoder_vmaf_hevc_remux_matches_source_at_100},
	};
	return RunSelectedTest(argc, argv, tests);
}
