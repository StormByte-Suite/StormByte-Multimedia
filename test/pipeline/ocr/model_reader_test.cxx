#include "../../test_helpers.hxx"

#include <StormByte/multimedia/ocr/engine.hxx>
#include <StormByte/multimedia/ocr/tessdata.hxx>
#ifndef STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
	#include <tessdata.h>
#endif

#include <array>
#include <limits>
#include <vector>

#ifndef STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
namespace {
	bool requestedVertical = false;
	bool loadedVertical = false;
}

namespace StormByte::Multimedia::OCR {
	ExpectedTessData TessData(std::string_view language) noexcept {
		const auto bytes = tessdata::blob(language);
		if (language == "jpn_vert") {
			requestedVertical = true;
			loadedVertical = !bytes.empty();
		}
		if (bytes.empty())
			return Unexpected<TessDataNotFoundException>(std::string(language));
		return bytes;
	}
}

int test_ocr_embedded_secondary_model() {
	requestedVertical = false;
	loadedVertical = false;
	StormByte::Multimedia::OCR::Engine engine{StormByte::Safe::String{"jpn"}};
	const std::vector<std::uint8_t> blank(320 * 80, 255);
	const auto result = engine.Recognize(blank, 320, 80, 320);
	TEST_REQUIRE(result);
	TEST_REQUIRE(requestedVertical);
	TEST_REQUIRE(loadedVertical);
	return 0;
}
#endif

int test_ocr_missing_pinned_model() {
	StormByte::Multimedia::OCR::Engine engine{StormByte::Safe::String{"missing-test-model"}};
	const std::vector<std::uint8_t> blank(320 * 80, 255);
	const auto result = engine.Recognize(blank, 320, 80, 320);
	TEST_REQUIRE(!result);
	return 0;
}

int test_ocr_image_size_limits() {
	StormByte::Multimedia::OCR::Engine engine;
	const std::array<std::uint8_t, 8> pixels{};
	TEST_REQUIRE(!engine.Recognize(pixels, 1, std::numeric_limits<int>::max(), 1));
	TEST_REQUIRE(!engine.Recognize(pixels, std::numeric_limits<int>::max(), 1, std::numeric_limits<int>::max()));
	TEST_REQUIRE(!engine.Recognize(pixels, 4, 4, 4));
	TEST_REQUIRE(!engine.Recognize(pixels, 4, 1, 3));
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
#ifndef STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
		TestEntry{"test_ocr_embedded_secondary_model", test_ocr_embedded_secondary_model},
#endif
		TestEntry{"test_ocr_missing_pinned_model", test_ocr_missing_pinned_model},
		TestEntry{"test_ocr_image_size_limits", test_ocr_image_size_limits},
	};
	return RunSelectedTest(argc, argv, tests);
}
