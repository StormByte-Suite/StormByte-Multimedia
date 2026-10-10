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

#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <chrono>
#include <thread>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
template<typename Configure>
static int CheckRejectedJob(std::string_view source, const std::filesystem::path& destination,
	std::string_view expectedMessage, Configure configure, bool rejectDuringConfigure = false) {
	auto logger = MakeLogger();
	Transcoder job{FixturePath(source), destination, logger, 2000000000LL};
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	configure(job);
	if (rejectDuringConfigure)
		TEST_REQUIRE(job.Failed());
	else
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job, expectedMessage);
}

static int CheckManualRejectsInvalidSource(std::string_view source, std::string_view destination) {
	Plan plan{FixturePath(source), OutputPath(destination), 2000000000LL};
	plan.add(Track{0, StormByte::Multimedia::Type::Video});
	const auto check = plan.Check();
	TEST_REQUIRE(!check);
	TEST_REQUIRE(check.error());
	TEST_REQUIRE(std::string_view{check.error()->what()}.find("source snapshot failed") != std::string_view::npos);

	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Demuxer demuxer{noLogger};
	Remuxer video{noLogger, 0};
	Muxer muxer{noLogger};
	std::move(plan) >> demuxer;
	demuxer >> muxer;
	demuxer >> video >> muxer;
	const auto deadline = std::chrono::steady_clock::now() + 30s;
	while (demuxer.Status() != State::Failed && demuxer.Status() != State::Stopped &&
		std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(10ms);
	TEST_REQUIRE(demuxer.Status() == State::Failed);
	const auto error = demuxer.Error();
	TEST_REQUIRE(error && !error->empty());
	TEST_REQUIRE(TestView(error.value()).find("source snapshot failed") != std::string_view::npos);
	TEST_REQUIRE(muxer.Failed());
	return 0;
}

static int CheckTranscoderRejectsInvalidSource(std::string_view source, std::string_view destination) {
	auto logger = MakeLogger();
	Transcoder job{FixturePath(source), OutputPath(destination), logger, 2000000000LL};
	TEST_REQUIRE(job.Failed());
	const auto originalError = job.Error();
	TEST_REQUIRE(originalError && !originalError->empty());
	job.Video(0).Remux();
	job.Run();
	TEST_REQUIRE(WaitForTranscoderFailure(job) == 0);
	TEST_REQUIRE(job.Error().value() == originalError.value());
	return 0;
}
}

int test_hand_built_pipe_rejects_random_invalid_source() {
	return CheckManualRejectsInvalidSource("invalid/random_garbage.mkv", "pipeline/rejected-manual-garbage.mkv");
}

int test_hand_built_pipe_rejects_truncated_source() {
	return CheckManualRejectsInvalidSource("invalid/truncated_hevc.mkv", "pipeline/rejected-manual-truncated.mkv");
}

int test_transcoder_rejects_random_invalid_source() {
	return CheckTranscoderRejectsInvalidSource("invalid/random_garbage.mkv", "pipeline/rejected-transcoder-garbage.mkv");
}

int test_transcoder_rejects_truncated_source() {
	return CheckTranscoderRejectsInvalidSource("invalid/truncated_hevc.mkv", "pipeline/rejected-transcoder-truncated.mkv");
}

int test_transcoder_rejects_negative_stream_index() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-negative-index.mkv"),
		"origin index is negative", [](Transcoder& job) { job.Audio(-1).Remux(); }, true);
}

int test_transcoder_rejects_missing_stream_index() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-missing-index.mkv"),
		"source stream 99 does not exist", [](Transcoder& job) { job.Video(99).Remux(); }, true);
}

int test_transcoder_rejects_wrong_stream_type() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-stream-type.mkv"),
		"requires", [](Transcoder& job) { job.Audio(0).Remux(); }, true);
}

int test_transcoder_rejects_wrong_codec_type() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-codec-type.mkv"),
		"codec 'MP3'", [&](Transcoder& job) { job.Video(0).Codec(mp3.value().get()); }, true);
}

int test_transcoder_rejects_duplicate_mapping() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-duplicate.mkv"),
		"origin 0 is already mapped", [](Transcoder& job) {
			job.Video(0).Remux();
			job.Video(0).Remux();
		}, true);
}

int test_transcoder_rejects_invalid_attachment_pattern() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-mime.mkv"),
		"attachment MIME pattern", [](Transcoder& job) { job.Attachments("application/**"); }, true);
}

int test_transcoder_rejects_missing_logger() {
	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Transcoder job{FixturePath("video/anime_like.mkv"),
		OutputPath("pipeline/rejected-no-logger.mkv"), noLogger, 2000000000LL};
	TEST_REQUIRE(job.Failed());
	job.Run();
	return WaitForTranscoderFailure(job, "logger is required");
}

int test_transcoder_rejects_empty_mapping() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-empty-map.mkv"),
		"plan has no tracks", [](Transcoder&) {});
}

int test_transcoder_rejects_unknown_destination_container() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-container.unknown-container"),
		"destination container is unknown", [](Transcoder& job) { job.Video(0).Remux(); });
}

int test_transcoder_rejects_video_in_wav() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-video.wav"),
		{}, [](Transcoder& job) { job.Video(0).Remux(); });
}

int test_transcoder_rejects_multiple_audio_tracks_in_wav() {
	return CheckRejectedJob("video/bluray_like_hdr10.mkv", OutputPath("pipeline/rejected-multiple-audio.wav"),
		{}, [](Transcoder& job) {
			job.Audio(1).Remux();
			job.Audio(2).Remux();
		});
}

int test_transcoder_rejects_aac_remux_in_mp3() {
	return CheckRejectedJob("audio/noise_51.m4a", OutputPath("pipeline/rejected-aac-remux.mp3"),
		"does not support codec aac", [](Transcoder& job) { job.Audio(0).Remux(); });
}

int test_transcoder_rejects_pgs_remux_in_mp4() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-pgs.mp4"),
		"does not support codec hdmv_pgs_subtitle", [](Transcoder& job) { job.Subtitle(4).Remux(); });
}

int test_transcoder_rejects_attachments_in_mp4() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-attachments.mp4"),
		{}, [](Transcoder& job) {
			job.Video(0).Remux();
			job.Attachments();
		});
}

int test_transcoder_rejects_missing_destination_parent() {
	const auto destination = OutputPath("pipeline/parent-probe.mkv").parent_path()
		/ "nonexistent-destination-parent" / "output.mkv";
	TEST_REQUIRE(!std::filesystem::exists(destination.parent_path()));
	return CheckRejectedJob("audio/noise_stereo.wav", destination,
		"muxer writer open failed", [](Transcoder& job) { job.Audio(0).Remux(); });
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_hand_built_pipe_rejects_random_invalid_source", test_hand_built_pipe_rejects_random_invalid_source},
		TestEntry{"test_hand_built_pipe_rejects_truncated_source", test_hand_built_pipe_rejects_truncated_source},
		TestEntry{"test_transcoder_rejects_random_invalid_source", test_transcoder_rejects_random_invalid_source},
		TestEntry{"test_transcoder_rejects_truncated_source", test_transcoder_rejects_truncated_source},
		TestEntry{"test_transcoder_rejects_negative_stream_index", test_transcoder_rejects_negative_stream_index},
		TestEntry{"test_transcoder_rejects_missing_stream_index", test_transcoder_rejects_missing_stream_index},
		TestEntry{"test_transcoder_rejects_wrong_stream_type", test_transcoder_rejects_wrong_stream_type},
		TestEntry{"test_transcoder_rejects_wrong_codec_type", test_transcoder_rejects_wrong_codec_type},
		TestEntry{"test_transcoder_rejects_duplicate_mapping", test_transcoder_rejects_duplicate_mapping},
		TestEntry{"test_transcoder_rejects_invalid_attachment_pattern", test_transcoder_rejects_invalid_attachment_pattern},
		TestEntry{"test_transcoder_rejects_missing_logger", test_transcoder_rejects_missing_logger},
		TestEntry{"test_transcoder_rejects_empty_mapping", test_transcoder_rejects_empty_mapping},
		TestEntry{"test_transcoder_rejects_unknown_destination_container", test_transcoder_rejects_unknown_destination_container},
		TestEntry{"test_transcoder_rejects_video_in_wav", test_transcoder_rejects_video_in_wav},
		TestEntry{"test_transcoder_rejects_multiple_audio_tracks_in_wav", test_transcoder_rejects_multiple_audio_tracks_in_wav},
		TestEntry{"test_transcoder_rejects_aac_remux_in_mp3", test_transcoder_rejects_aac_remux_in_mp3},
		TestEntry{"test_transcoder_rejects_pgs_remux_in_mp4", test_transcoder_rejects_pgs_remux_in_mp4},
		TestEntry{"test_transcoder_rejects_attachments_in_mp4", test_transcoder_rejects_attachments_in_mp4},
		TestEntry{"test_transcoder_rejects_missing_destination_parent", test_transcoder_rejects_missing_destination_parent},
	};
	return RunSelectedTest(argc, argv, tests);
}
