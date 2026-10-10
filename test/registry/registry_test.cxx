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
