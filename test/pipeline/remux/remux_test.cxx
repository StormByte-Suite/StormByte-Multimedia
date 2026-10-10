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
#include <StormByte/multimedia/pipeline/config/attachment.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <array>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
	int CheckRemuxOutput(const std::filesystem::path& output, std::size_t expectedAttachments = 1) {
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& result = opened.value();
		std::size_t videos = 0;
		std::size_t audios = 0;
		std::size_t subtitles = 0;
		bool foundJapanese = false;
		for (const auto& stream : result.Streams()) {
			switch (stream.Type()) {
				case StormByte::Multimedia::Type::Video: ++videos; break;
				case StormByte::Multimedia::Type::Audio: ++audios; break;
				case StormByte::Multimedia::Type::Subtitle: {
					++subtitles;
					TEST_REQUIRE(stream.Codec().Name() == "PGS"sv);
					const auto language = stream.Metadata().Language();
					if (!language)
						std::cerr << "[DETAIL] PGS stream " << stream.Index() << " has no language tag" << std::endl;
					TEST_REQUIRE(language);
					foundJapanese = foundJapanese || TestView(language.value()) == "jpn"sv;
					break;
				}
				default: break;
			}
		}
		TEST_REQUIRE(videos == 1);
		TEST_REQUIRE(audios == 1);
		TEST_REQUIRE(subtitles == 3);
		TEST_REQUIRE(foundJapanese);
		TEST_REQUIRE(result.Attachments().size() == expectedAttachments);
		return 0;
	}
}

int test_transcoder_remux_preserves_pgs_and_attachments() {
	const auto output = OutputPath("pipeline/transcoder-remux.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Video(0).Remux();
	job.Audio(1).Remux();
	job.Subtitle(2).Remux();
	job.Subtitle(3).Remux();
	job.Subtitle(4).Remux();
	job.Attachments();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckRemuxOutput(output);
}

int test_transcoder_remux_omits_unselected_attachments() {
	const auto output = OutputPath("pipeline/transcoder-remux-no-attachments.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Video(0).Remux();
	job.Audio(1).Remux();
	job.Subtitle(2).Remux();
	job.Subtitle(3).Remux();
	job.Subtitle(4).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckRemuxOutput(output, 0);
}

int test_hand_built_pipe_remux_preserves_pgs_and_attachments() {
	const auto input = FixturePath("video/anime_like.mkv");
	const auto output = OutputPath("pipeline/manual-remux.mkv");
	Plan plan{StormByte::Safe::String{input.string()}, StormByte::Safe::String{output.string()}, 2000000000LL};
	plan.add(Track{0, StormByte::Multimedia::Type::Video});
	plan.add(Track{1, StormByte::Multimedia::Type::Audio});
	plan.add(Track{2, Config::Subtitle{}});
	plan.add(Track{3, Config::Subtitle{}});
	plan.add(Track{4, Config::Subtitle{}});
	Config::Attachment attachments{std::string_view{"*/*"}};
	plan.add(Track{0, std::move(attachments)});
	const auto planCheck = plan.Check();
	if (!planCheck) {
		std::cerr << "[DETAIL] Manual Plan check: "
			<< planCheck.error()->what() << std::endl;
		return 1;
	}

	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Demuxer demuxer{noLogger};
	Remuxer video{noLogger, 0};
	Remuxer audio{noLogger, 1};
	Remuxer spanish{noLogger, 2};
	Remuxer english{noLogger, 3};
	Remuxer japanese{noLogger, 4};
	Muxer muxer{noLogger};
	std::move(plan) >> demuxer;
	demuxer >> muxer;
	demuxer >> video >> muxer;
	demuxer >> audio >> muxer;
	demuxer >> spanish >> muxer;
	demuxer >> english >> muxer;
	demuxer >> japanese >> muxer;
	TEST_REQUIRE(WaitForManualPipeline(muxer) == 0);
	return CheckRemuxOutput(output);
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_remux_preserves_pgs_and_attachments", test_transcoder_remux_preserves_pgs_and_attachments},
		TestEntry{"test_transcoder_remux_omits_unselected_attachments", test_transcoder_remux_omits_unselected_attachments},
		TestEntry{"test_hand_built_pipe_remux_preserves_pgs_and_attachments", test_hand_built_pipe_remux_preserves_pgs_and_attachments},
	};
	return RunSelectedTest(argc, argv, tests);
}
