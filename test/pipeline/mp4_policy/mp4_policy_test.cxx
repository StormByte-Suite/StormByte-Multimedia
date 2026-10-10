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
#include <algorithm>
#include <span>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	struct TrackExpectation {
		int Input;
		StormByte::Multimedia::Type Kind;
		std::string_view Codec;
		std::string_view Language;
		bool Encode = false;
	};

	constexpr TrackExpectation Video{0, StormByte::Multimedia::Type::Video, "H.264", {}};
	constexpr TrackExpectation EnglishAudio{1, StormByte::Multimedia::Type::Audio, "AAC", "eng"};
	constexpr TrackExpectation SpanishAudio{2, StormByte::Multimedia::Type::Audio, "AAC", "spa"};
	constexpr TrackExpectation EnglishSubtitle{3, StormByte::Multimedia::Type::Subtitle, "3GPP Timed Text", "eng"};
	constexpr TrackExpectation SpanishSubtitle{4, StormByte::Multimedia::Type::Subtitle, "3GPP Timed Text", "spa"};

	int CheckMp4Tracks(std::string_view destination, std::span<const TrackExpectation> expected,
		std::string_view source = "video/bluray_like_hdr10.mp4") {
		const auto output = OutputPath(destination);
		{
			auto logger = MakeLogger();
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(FixturePath(source)), TestLocation(output), logger, 2000000000LL};
			for (const auto& item : expected) {
				auto track = item.Kind == StormByte::Multimedia::Type::Video ? job.Video(item.Input)
					: item.Kind == StormByte::Multimedia::Type::Audio ? job.Audio(item.Input)
					: job.Subtitle(item.Input);
				if (item.Encode) {
					auto codec = Registry::Instance().FindCodec(item.Codec);
					TEST_REQUIRE(codec);
					if (!codec.value().get().HasAccess(Access{Operation::Write}))
						return TEST_SKIP;
					track.Codec(codec.value().get());
					if (item.Kind == StormByte::Multimedia::Type::Audio)
						track.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"aac"}).BitRate(384000);
					else if (item.Kind == StormByte::Multimedia::Type::Subtitle)
						track.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"mov_text"});
				}
				else
					track.Remux();
				if (!item.Language.empty())
					track.Language(StormByte::Safe::String{item.Language});
			}
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("starting transcoder run");
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}

		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		if (!opened)
			std::cerr << "[DETAIL] MP4 output " << output.string() << ": " << opened.error()->what() << std::endl;
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == "MP4");
		TEST_REQUIRE(opened.value().Attachments().empty());
		const auto streams = opened.value().Streams();
		TEST_REQUIRE(streams.size() == expected.size());
		for (std::size_t index = 0; index < expected.size(); ++index) {
			const auto stream = streams[index];
			const auto& item = expected[index];
			TEST_REQUIRE(stream.Index() == static_cast<int>(index));
			TEST_REQUIRE(stream.Type() == item.Kind);
			TEST_REQUIRE(stream.Codec().Name() == item.Codec);
			if (!item.Language.empty()) {
				const auto language = stream.Metadata().Language();
				TEST_REQUIRE(language && TestView(language.value()) == item.Language);
			}
			if (item.Kind == StormByte::Multimedia::Type::Video) {
				const auto video = stream.Video();
				TEST_REQUIRE(video);
				TEST_REQUIRE(video.value().Resolution().Width() == 1920);
				TEST_REQUIRE(video.value().Resolution().Height() == 1080);
			}
			if (item.Kind == StormByte::Multimedia::Type::Audio) {
				const auto audio = stream.Audio();
				TEST_REQUIRE(audio);
				TEST_REQUIRE(audio.value().Channels() == 6);
				TEST_REQUIRE(audio.value().SampleRate() == 48000);
			}
		}
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}

	int CheckRejectedAttachment(std::string_view source, std::string_view destination,
		std::string_view pattern, std::string_view expectedMime) {
		TEST_PHASE("opening media file for inspection");
		auto input = File::Open(StormByte::Safe::String{FixturePath(source).string()});
		TEST_REQUIRE(input);
		bool found = false;
		for (const auto& attachment : input.value().Attachments()) {
			const auto mime = attachment.MimeType();
			found = found || (mime && TestView(mime.value()) == expectedMime);
		}
		TEST_REQUIRE(found);
		auto logger = MakeLogger();
			TEST_PHASE("creating transcoder");
			Transcoder job{TestLocation(FixturePath(source)), TestLocation(OutputPath(destination)), logger, 2000000000LL};
		job.Video(0).Remux();
		job.Attachments(pattern);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		return WaitForTranscoderFailure(job, "MP4 attachment cannot be represented as cover art");
	}

	int CheckCoverRoundtrip(std::string_view destination, std::string_view containerName, bool audioOnly = false) {
		const auto sourcePath = FixturePath("video/bluray_like_hdr10.mkv");
		TEST_PHASE("opening media file for inspection");
		auto source = File::Open(StormByte::Safe::String{sourcePath.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(source.value().Attachments().size() == 1);
		const auto original = source.value().Attachments()[0];
		const auto output = OutputPath(destination);
		{
			auto logger = MakeLogger();
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(sourcePath), TestLocation(output), logger, 2000000000LL};
			if (audioOnly)
				job.Audio(2).Remux();
			else
				job.Video(0).Remux();
			job.Attachments("image/*");
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("starting transcoder run");
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == containerName);
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == (audioOnly ? StormByte::Multimedia::Type::Audio : StormByte::Multimedia::Type::Video));
		TEST_REQUIRE(stream.Codec().Name() == (audioOnly ? "AAC" : "H.265"));
		TEST_REQUIRE(opened.value().Attachments().size() == 1);
		const auto cover = opened.value().Attachments()[0];
		const auto mime = cover.MimeType();
		TEST_REQUIRE(mime && TestView(mime.value()) == "image/png");
		const auto& before = original.Payload().Data();
		const auto& after = cover.Payload().Data();
		TEST_REQUIRE(original.Payload().Available() > 0);
		TEST_REQUIRE(cover.Payload().Available() == original.Payload().Available());
		TEST_REQUIRE(std::equal(before.begin(), before.end(), after.begin(), after.end()));
		if (containerName == "Matroska") {
			const auto name = cover.FileName();
			TEST_REQUIRE(name && TestView(name.value()) == "synthetic-cover.png");
		}
		return 0;
	}
}

int test_mp4_video_only() {
	constexpr std::array tracks{Video};
	return CheckMp4Tracks("pipeline/mp4-policy/video.mp4", tracks);
}

int test_mp4_audio_only_m4a() {
	constexpr std::array tracks{EnglishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/audio.m4a", tracks);
}

int test_mp4_audio_only_m4b() {
	constexpr std::array tracks{EnglishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/audio.m4b", tracks);
}

int test_mp4_two_audio_tracks() {
	constexpr std::array tracks{EnglishAudio, SpanishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/two-audio.mp4", tracks);
}

int test_mp4_reordered_audio_tracks() {
	constexpr std::array tracks{SpanishAudio, EnglishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/reordered-audio.mp4", tracks);
}

int test_mp4_video_and_audio() {
	constexpr std::array tracks{Video, EnglishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/video-audio.mp4", tracks);
}

int test_mp4_video_and_subtitle() {
	constexpr std::array tracks{Video, EnglishSubtitle};
	return CheckMp4Tracks("pipeline/mp4-policy/video-subtitle.mp4", tracks);
}

int test_mp4_video_and_two_audio_tracks() {
	constexpr std::array tracks{Video, EnglishAudio, SpanishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/video-two-audio.mp4", tracks);
}

int test_mp4_video_and_two_subtitle_tracks() {
	constexpr std::array tracks{Video, EnglishSubtitle, SpanishSubtitle};
	return CheckMp4Tracks("pipeline/mp4-policy/video-two-subtitles.mp4", tracks);
}

int test_mp4_all_five_tracks() {
	constexpr std::array tracks{Video, EnglishAudio, SpanishAudio, EnglishSubtitle, SpanishSubtitle};
	return CheckMp4Tracks("pipeline/mp4-policy/all-tracks.mp4", tracks);
}

int test_mp4_subtitles_only() {
	constexpr std::array tracks{EnglishSubtitle, SpanishSubtitle};
	return CheckMp4Tracks("pipeline/mp4-policy/subtitles.mp4", tracks);
}

int test_mp4_remux_video_and_encode_audio() {
	constexpr std::array tracks{Video,
		TrackExpectation{1, StormByte::Multimedia::Type::Audio, "AAC", "eng", true}, EnglishSubtitle};
	return CheckMp4Tracks("pipeline/mp4-policy/remux-and-encode.mp4", tracks);
}

int test_mp4_audio_and_encode_timed_text() {
	constexpr std::array tracks{EnglishAudio,
		TrackExpectation{3, StormByte::Multimedia::Type::Subtitle, "3GPP Timed Text", "eng", true}};
	return CheckMp4Tracks("pipeline/mp4-policy/encoded-timed-text.mp4", tracks);
}

int test_mp4_hevc_aac_without_attachments() {
	constexpr std::array tracks{
		TrackExpectation{0, StormByte::Multimedia::Type::Video, "H.265", {}}, SpanishAudio};
	return CheckMp4Tracks("pipeline/mp4-policy/hevc-aac.mp4", tracks, "video/bluray_like_hdr10.mkv");
}

int test_mp4_preserves_image_as_cover() {
	return CheckCoverRoundtrip("pipeline/mp4-policy/video-cover.mp4", "MP4");
}

int test_m4a_preserves_image_as_cover() {
	return CheckCoverRoundtrip("pipeline/mp4-policy/audio-cover.m4a", "MP4", true);
}

int test_matroska_preserves_image_as_attachment() {
	return CheckCoverRoundtrip("pipeline/mp4-policy/matroska-cover.mkv", "Matroska");
}

int test_mp4_rejects_font_attachment() {
	return CheckRejectedAttachment("video/anime_like.mkv", "pipeline/mp4-policy/rejected-font.mp4",
		"application/*", "application/x-truetype-font");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_mp4_video_only", test_mp4_video_only},
		TestEntry{"test_mp4_audio_only_m4a", test_mp4_audio_only_m4a},
		TestEntry{"test_mp4_audio_only_m4b", test_mp4_audio_only_m4b},
		TestEntry{"test_mp4_two_audio_tracks", test_mp4_two_audio_tracks},
		TestEntry{"test_mp4_reordered_audio_tracks", test_mp4_reordered_audio_tracks},
		TestEntry{"test_mp4_video_and_audio", test_mp4_video_and_audio},
		TestEntry{"test_mp4_video_and_subtitle", test_mp4_video_and_subtitle},
		TestEntry{"test_mp4_video_and_two_audio_tracks", test_mp4_video_and_two_audio_tracks},
		TestEntry{"test_mp4_video_and_two_subtitle_tracks", test_mp4_video_and_two_subtitle_tracks},
		TestEntry{"test_mp4_all_five_tracks", test_mp4_all_five_tracks},
		TestEntry{"test_mp4_subtitles_only", test_mp4_subtitles_only},
		TestEntry{"test_mp4_remux_video_and_encode_audio", test_mp4_remux_video_and_encode_audio},
		TestEntry{"test_mp4_audio_and_encode_timed_text", test_mp4_audio_and_encode_timed_text},
		TestEntry{"test_mp4_hevc_aac_without_attachments", test_mp4_hevc_aac_without_attachments},
		TestEntry{"test_mp4_preserves_image_as_cover", test_mp4_preserves_image_as_cover},
		TestEntry{"test_m4a_preserves_image_as_cover", test_m4a_preserves_image_as_cover},
		TestEntry{"test_matroska_preserves_image_as_attachment", test_matroska_preserves_image_as_attachment},
		TestEntry{"test_mp4_rejects_font_attachment", test_mp4_rejects_font_attachment},
	};
	return RunSelectedTest(argc, argv, tests);
}
