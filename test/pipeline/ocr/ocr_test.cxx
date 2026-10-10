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
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Subtitle(4).Codec(subrip.value().get()).Language(StormByte::Safe::String{"jpn"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	TEST_PHASE("opening media file for inspection");
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
