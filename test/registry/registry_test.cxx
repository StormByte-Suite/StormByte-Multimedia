#include "../test_helpers.hxx"

#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <string_view>

using namespace StormByte::Multimedia;
using namespace std::string_view_literals;

namespace {
	int CheckContainerAliases(std::string_view canonical, std::span<const std::string_view> aliases) {
		auto& registry = Registry::Instance();
		auto expected = registry.FindContainer(canonical);
		TEST_REQUIRE(expected);
		for (const auto alias : aliases) {
			auto actual = registry.FindContainer(alias);
			TEST_REQUIRE(actual);
			TEST_REQUIRE(&actual.value().get() == &expected.value().get());
		}
		return 0;
	}
}

int test_matroska_extensions_resolve_to_same_container() {
	constexpr std::array aliases{"mkv"sv, "mka"sv, "mks"sv, "mk3d"sv};
	return CheckContainerAliases("Matroska", aliases);
}

int test_mov_extensions_resolve_to_same_container() {
	constexpr std::array aliases{"mov"sv, "qt"sv};
	return CheckContainerAliases("MOV", aliases);
}

int test_mp4_extensions_resolve_to_same_container() {
	constexpr std::array aliases{"mp4"sv, "m4a"sv, "m4b"sv, "m4r"sv};
	return CheckContainerAliases("MP4", aliases);
}

int test_mp4_registry_exposes_read_and_write_access() {
	auto container = Registry::Instance().FindContainer("MP4");
	TEST_REQUIRE(container);
	TEST_REQUIRE(container.value().get().HasAccess(Access{Operation::Read}));
	TEST_REQUIRE(container.value().get().HasAccess(Access{Operation::Write}));
	return 0;
}

int test_codec_names_resolve_by_public_identity() {
	auto& registry = Registry::Instance();
	auto canonical = registry.FindCodec("H.265");
	auto ffmpegName = registry.FindCodec("hevc");
	TEST_REQUIRE(canonical);
	TEST_REQUIRE(ffmpegName);
	TEST_REQUIRE(&canonical.value().get() == &ffmpegName.value().get());
	TEST_REQUIRE(canonical.value().get().Type() == StormByte::Multimedia::Type::Video);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_matroska_extensions_resolve_to_same_container", test_matroska_extensions_resolve_to_same_container},
		TestEntry{"test_mov_extensions_resolve_to_same_container", test_mov_extensions_resolve_to_same_container},
		TestEntry{"test_mp4_extensions_resolve_to_same_container", test_mp4_extensions_resolve_to_same_container},
		TestEntry{"test_mp4_registry_exposes_read_and_write_access", test_mp4_registry_exposes_read_and_write_access},
		TestEntry{"test_codec_names_resolve_by_public_identity", test_codec_names_resolve_by_public_identity},
	};
	return RunSelectedTest(argc, argv, tests);
}
