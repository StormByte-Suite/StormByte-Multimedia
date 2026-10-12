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
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	int CheckSubtitleEncode(std::string_view source, std::string_view destination,
		std::string_view containerName, std::string_view codecName, std::string_view encoder) {
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(destination);
		auto logger = MakeLogger();
		TEST_PHASE("creating transcoder");
		Transcoder job{TestLocation(FixturePath(source)), TestLocation(output), logger};
		job.Subtitle(0).Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder});
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		const int result = WaitForOptionalCodec(job);
		if (result != 0)
			return result;

		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == containerName);
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Subtitle);
		TEST_REQUIRE(stream.Codec().Name() == codecName);
		return 0;
	}
}

int test_srt_encodes_srt() {
	return CheckSubtitleEncode("subtitles/anime_eng.srt", "pipeline/subtitle-codecs/srt.srt",
		"SRT", "SRT", "srt");
}

int test_ass_encodes_matroska() {
	return CheckSubtitleEncode("subtitles/anime_eng.srt", "pipeline/subtitle-codecs/ass.mkv",
		"Matroska", "ASS", "ass");
}

int test_webvtt_encodes_vtt() {
	return CheckSubtitleEncode("subtitles/anime_eng.srt", "pipeline/subtitle-codecs/webvtt.vtt",
		"WebVTT", "WebVTT", "webvtt");
}

int test_mov_text_encodes_mp4() {
	return CheckSubtitleEncode("subtitles/anime_eng.srt", "pipeline/subtitle-codecs/mov-text.mp4",
		"MP4", "3GPP Timed Text", "mov_text");
}

int test_ttml_encodes_mp4() {
	return CheckSubtitleEncode("subtitles/anime_eng.srt", "pipeline/subtitle-codecs/ttml.mp4",
		"MP4", "TTML", "ttml");
}

int test_ttml_rejects_matroska() {
	auto codec = Registry::Instance().FindCodec("TTML");
	TEST_REQUIRE(codec);
	if (!codec.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath("subtitles/anime_eng.srt")),
		TestLocation(OutputPath("pipeline/subtitle-codecs/ttml.mks")), logger};
	job.Subtitle(0).Codec(codec.value().get())
		.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"ttml"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job, "does not support codec ttml");
}

int test_pgssub_remuxes_sup() {
	const auto output = OutputPath("pipeline/subtitle-codecs/pgs.mks");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("subtitles/anime_eng.sup")), TestLocation(output), logger};
	job.Subtitle(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	TEST_PHASE("opening media file for inspection");
	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Container().Name() == "Matroska");
	TEST_REQUIRE(!opened.value().Streams().empty());
	TEST_REQUIRE(opened.value().Streams()[0].Type() == StormByte::Multimedia::Type::Subtitle);
	TEST_REQUIRE(opened.value().Streams()[0].Codec().Name() == "PGS");
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_srt_encodes_srt", test_srt_encodes_srt},
		TestEntry{"test_ass_encodes_matroska", test_ass_encodes_matroska},
		TestEntry{"test_webvtt_encodes_vtt", test_webvtt_encodes_vtt},
		TestEntry{"test_mov_text_encodes_mp4", test_mov_text_encodes_mp4},
		TestEntry{"test_ttml_encodes_mp4", test_ttml_encodes_mp4},
		TestEntry{"test_ttml_rejects_matroska", test_ttml_rejects_matroska},
		TestEntry{"test_pgssub_remuxes_sup", test_pgssub_remuxes_sup},
	};
	return RunSelectedTest(argc, argv, tests);
}
