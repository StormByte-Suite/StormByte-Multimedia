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
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	class EmptyCues final: public Filter::Process {
		public:
			EmptyCues(): Filter::Process({}, "empty-test-cues") {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Subtitle;
			}

		protected:
			void Clean() noexcept override {}

			void Setup() noexcept override {}

			void Process(const Frame& frame) noexcept override {
				auto& payload = const_cast<Frame&>(frame).Payload();
				static_cast<void>(payload.Drop(payload.Available()));
			}
	};

	int CheckHeaderWithoutCues(std::string_view source, int audioIndex, int subtitleIndex,
		std::string_view destination, std::string_view containerName, std::string_view subtitleCodec,
		bool retainEmptyTrack = true) {
		auto codec = Registry::Instance().FindCodec(subtitleCodec);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(destination);
		{
			Plan plan{FixturePath(source), output, 2000000000LL};
			plan.add(Track{audioIndex, StormByte::Multimedia::Type::Audio});
			Config::Subtitle config;
			config.Codec(codec.value().get());
			plan.add(Track{subtitleIndex, std::move(config)});
			TEST_REQUIRE(plan.Check());
			StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
			Demuxer demuxer{noLogger};
			Remuxer audio{noLogger, audioIndex};
			auto cueDrain = StormByte::Safe::MakeShared<Decoder>(noLogger, subtitleIndex);
			auto subtitle = StormByte::Safe::MakeShared<Encoder>(noLogger, 1, codec.value().get());
			Muxer muxer{noLogger};
			Filters graph;
			std::move(plan) >> demuxer;
			demuxer >> muxer;
			demuxer >> audio >> muxer;
			demuxer >> *cueDrain;
			*subtitle >> muxer;
			TEST_REQUIRE(subtitle->Opened());
			graph.Between(cueDrain, subtitle).Add(StormByte::Safe::MakeShared<EmptyCues>());
			graph.Close();
			TEST_REQUIRE(WaitForManualPipeline(muxer) == 0);
			TEST_REQUIRE(WaitForManualPipeline(*cueDrain) == 0);
			TEST_REQUIRE(!cueDrain->Failed());
			TEST_REQUIRE(!demuxer.Failed() && !audio.Failed() && !subtitle->Failed() && !muxer.Failed());
		}
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == containerName);
		const auto count = opened.value().Streams().size();
		TEST_REQUIRE(count >= 1 && count <= 2);
		if (retainEmptyTrack)
			TEST_REQUIRE(count == 2);
		const auto audio = opened.value().Streams()[0];
		TEST_REQUIRE(audio.Type() == StormByte::Multimedia::Type::Audio);
		if (count == 2) {
			const auto subtitle = opened.value().Streams()[1];
			TEST_REQUIRE(subtitle.Type() == StormByte::Multimedia::Type::Subtitle);
			TEST_REQUIRE(subtitle.Codec().Name() == subtitleCodec);
		}
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}
}

int test_muxer_declares_ass_without_cues() {
	return CheckHeaderWithoutCues("video/anime_like.mkv", 1, 4,
		"pipeline/subtitle-header/empty-ass.mkv", "Matroska", "ASS");
}

int test_muxer_declares_mov_text_without_cues() {
	return CheckHeaderWithoutCues("video/bluray_like_hdr10.mp4", 1, 3,
		"pipeline/subtitle-header/empty-mov-text.mp4", "MP4", "3GPP Timed Text", false);
}

int test_subtitle_first_cue_after_one_hour() {
	auto codec = Registry::Instance().FindCodec("ASS");
	TEST_REQUIRE(codec);
	if (!codec.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	const auto output = OutputPath("pipeline/subtitle-header/late-first-cue.mkv");
	{
		auto logger = MakeLogger();
		Transcoder job{FixturePath("subtitles/late_first_cue.srt"), output, logger, 3602000000000LL};
		job.Subtitle(0).Codec(codec.value().get());
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
	}
	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Streams().size() == 1);
	const auto subtitle = opened.value().Streams()[0];
	TEST_REQUIRE(subtitle.Type() == StormByte::Multimedia::Type::Subtitle);
	TEST_REQUIRE(subtitle.Codec().Name() == "ASS");
	const auto& duration = opened.value().Duration();
	TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_muxer_declares_ass_without_cues", test_muxer_declares_ass_without_cues},
		TestEntry{"test_muxer_declares_mov_text_without_cues", test_muxer_declares_mov_text_without_cues},
		TestEntry{"test_subtitle_first_cue_after_one_hour", test_subtitle_first_cue_after_one_hour},
	};
	return RunSelectedTest(argc, argv, tests);
}
