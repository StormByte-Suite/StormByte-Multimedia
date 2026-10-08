#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <fstream>
#include <iterator>
#include <string>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
	std::string ReadFileBytes(const std::filesystem::path& path) {
		std::ifstream input{path, std::ios::binary};
		return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
	}

	std::string WithoutAsciiWhitespace(std::string_view text) {
		std::string result;
		for (const unsigned char character : text)
			if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
				result.push_back(static_cast<char>(character));
		return result;
	}
}

int test_transcoder_ocr_japanese_pgs_to_subrip() {
	auto& registry = Registry::Instance();
	auto subrip = registry.FindCodec("SubRip");
	TEST_REQUIRE(subrip);
	if (!subrip.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/ocr-japanese.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/anime_like.mkv"), output, logger, 2000000000LL};
	job.Subtitle(4).Codec(subrip.value().get()).Language(StormByte::Safe::String{"jpn"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Streams().size() == 1);
	const auto subtitle = opened.value().Streams()[0];
	TEST_REQUIRE(subtitle.Type() == StormByte::Multimedia::Type::Subtitle);
	TEST_REQUIRE(subtitle.Codec().Name() == "SRT"sv);
	const auto language = subtitle.Metadata().Language();
	TEST_REQUIRE(language);
	TEST_REQUIRE(TestView(language.value()) == "jpn"sv);
	const auto encodedBytes = ReadFileBytes(output);
	const auto normalizedBytes = WithoutAsciiWhitespace(encodedBytes);
	const bool foundSubtitleToken = normalizedBytes.find("字幕") != std::string::npos;
	const bool foundDepartureToken = normalizedBytes.find("出発") != std::string::npos;
	if (!foundSubtitleToken || !foundDepartureToken)
		std::cerr << "[DETAIL] OCR did not recognize the expected Japanese tokens: 字幕 / 出発" << std::endl;
	TEST_REQUIRE(foundSubtitleToken);
	TEST_REQUIRE(foundDepartureToken);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_ocr_japanese_pgs_to_subrip", test_transcoder_ocr_japanese_pgs_to_subrip},
	};
	return RunSelectedTest(argc, argv, tests);
}
