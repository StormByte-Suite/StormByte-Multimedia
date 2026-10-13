#include "../../test_helpers.hxx"

#include <StormByte/multimedia/ocr/engine.hxx>
#include <StormByte/multimedia/ocr/tessdata.hxx>
#include <tessdata.h>

#include <array>
#include <vector>

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

int test_ocr_missing_pinned_model() {
	StormByte::Multimedia::OCR::Engine engine{StormByte::Safe::String{"missing-test-model"}};
	const std::vector<std::uint8_t> blank(320 * 80, 255);
	const auto result = engine.Recognize(blank, 320, 80, 320);
	TEST_REQUIRE(!result);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_ocr_embedded_secondary_model", test_ocr_embedded_secondary_model},
		TestEntry{"test_ocr_missing_pinned_model", test_ocr_missing_pinned_model},
	};
	return RunSelectedTest(argc, argv, tests);
}
