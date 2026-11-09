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
#include <StormByte/multimedia/metadata/tag.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <iostream>
#include <string>
#include <string_view>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using Metadata::FileTag;
using Metadata::StreamTag;

namespace {
	constexpr std::int64_t Duration = 2000000000LL;
	constexpr std::string_view TrackTitle = "Edited track title";

	struct TagValue {
		FileTag tag;
		std::string_view value;
	};

	constexpr std::array EditedTags{
		TagValue{FileTag::Title, "Edited Title"},
		TagValue{FileTag::Artist, "Edited Artist"},
		TagValue{FileTag::Album, "Edited Album"},
		TagValue{FileTag::AlbumArtist, "Edited Album Artist"},
		TagValue{FileTag::Composer, "Edited Composer"},
		TagValue{FileTag::Genre, "Edited Genre"},
		TagValue{FileTag::Comment, "Edited Comment"},
		TagValue{FileTag::Copyright, "Edited Copyright"},
		TagValue{FileTag::Date, "2024"},
		TagValue{FileTag::Track, "4"},
		TagValue{FileTag::Disc, "5"},
	};

	bool Equals(const StormByte::Safe::Optional<StormByte::Safe::String>& value, std::string_view expected) {
		return value && std::string_view{*value} == expected;
	}

	const Container* FindContainer(std::string_view name) {
		const auto found = Registry::Instance().FindContainer(name);
		return found ? &found.value().get() : nullptr;
	}

	/**
	 * @brief Runs one audio-only job.
	 * @param input Source path.
	 * @param output Destination path.
	 * @param codecName Encoder codec, or empty to remux.
	 * @param configure Applies the edits under test.
	 * @return Zero, TEST_SKIP when the encoder is missing, or one.
	 */
	template<typename Configure>
	int WriteAudio(const std::filesystem::path& input, const std::filesystem::path& output,
		std::string_view codecName, Configure&& configure) {
		Transcoder job{TestLocation(input), TestLocation(output), MakeLogger(), Duration};
		auto track = job.Audio(0);
		if (codecName.empty())
			track.Remux();
		else {
			const auto codec = Registry::Instance().FindCodec(codecName);
			TEST_REQUIRE(codec);
			if (!codec->get().HasAccess(Access{Operation::Write}))
				return TEST_SKIP;
			track.Codec(codec->get());
		}
		configure(job, track);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
		return 0;
	}

	/**
	 * @brief Writes every file and stream tag and checks that exactly the
	 * tags reported by Container::Stores read back.
	 */
	int CheckCapability(std::string_view containerName, std::string_view extension, std::string_view codecName,
		std::string_view fixture = "audio/noise_tagged.mp3") {
		const auto* container = FindContainer(containerName);
		TEST_REQUIRE(container);
		TEST_REQUIRE(container->HasAccess(Access{Operation::Write}));
		const auto output = OutputPath(std::string{"pipeline/metadata-edit/capability."} + std::string{extension});
		const int written = WriteAudio(FixturePath(fixture), output, codecName,
			[](Transcoder& job, Transcoder::Track& track) {
				for (const auto& edit : EditedTags)
					job.Tag(edit.tag, StormByte::Safe::String{edit.value});
				track.Title(StormByte::Safe::String{TrackTitle}).Language("spa");
			});
		if (written != 0)
			return written;
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened->Streams().size() == 1);
		int failures = 0;
		for (const auto& edit : EditedTags) {
			const auto value = opened->Metadata().Tag(edit.tag);
			if (Equals(value, edit.value) != container->Stores(edit.tag)) {
				std::cerr << "[ASSERT] " << containerName << " file tag " << static_cast<int>(edit.tag)
					<< " read back as '" << (value ? std::string{*value} : std::string{"<none>"})
					<< "', Stores=" << container->Stores(edit.tag) << std::endl;
				++failures;
			}
		}
		const auto& stream = opened->Streams()[0].Metadata();
		// Ogg-based outputs replace the track title with the edited file title.
		const std::string_view expectedTitle = container->SharedTitle() ? EditedTags[0].value : TrackTitle;
		if (Equals(stream.Title(), expectedTitle) != container->Stores(StreamTag::Title)) {
			std::cerr << "[ASSERT] " << containerName << " stream title read back as '"
				<< (stream.Title() ? std::string{*stream.Title()} : std::string{"<none>"})
				<< "', Stores=" << container->Stores(StreamTag::Title) << std::endl;
			++failures;
		}
		if (Equals(stream.Language(), "spa") != container->Stores(StreamTag::Language)) {
			std::cerr << "[ASSERT] " << containerName << " stream language read back as '"
				<< (stream.Language() ? std::string{*stream.Language()} : std::string{"<none>"})
				<< "', Stores=" << container->Stores(StreamTag::Language) << std::endl;
			++failures;
		}
		TEST_REQUIRE(failures == 0);
		return 0;
	}

	/**
	 * @brief Sets, keeps and removes file tags and checks the result.
	 */
	int CheckFileTagEdits(std::string_view containerName, std::string_view extension, std::string_view codecName) {
		const auto* container = FindContainer(containerName);
		TEST_REQUIRE(container);
		const auto input = FixturePath("audio/noise_tagged.mp3");
		auto source = File::Open(StormByte::Safe::String{input.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(source->Metadata().Artist() && source->Metadata().Comment() && source->Metadata().Composer());
		const auto output = OutputPath(std::string{"pipeline/metadata-edit/file-tags."} + std::string{extension});
		const int written = WriteAudio(input, output, codecName, [](Transcoder& job, Transcoder::Track&) {
			job.Tag(FileTag::Title, "Nuevo título ñ")
				.Tag(FileTag::Copyright, "CC0")
				.Tag(FileTag::Track, "5/12")
				.Tag(FileTag::Album, "")
				.RemoveTag(FileTag::Comment)
				.RemoveTag(FileTag::Composer);
		});
		if (written != 0)
			return written;
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& before = source->Metadata();
		const auto& after = opened->Metadata();
		TEST_REQUIRE(Equals(after.Title(), "Nuevo título ñ"));
		TEST_REQUIRE(Equals(after.Copyright(), "CC0"));
		TEST_REQUIRE(after.Track() && *after.Track() == 5);
		TEST_REQUIRE(after.Artist() == before.Artist());
		// An empty value drops the edit and keeps the source value.
		TEST_REQUIRE(after.Album() == before.Album());
		TEST_REQUIRE(!after.Comment());
		TEST_REQUIRE(!after.Composer());
		TEST_REQUIRE(container->Stores(FileTag::Comment) && container->Stores(FileTag::Composer));
		return 0;
	}

	bool LanguageRemoved(const Metadata::Stream& stream) {
		return !stream.Language() || std::string_view{*stream.Language()} == "und";
	}
}

int test_capability_aiff() { return CheckCapability("AIFF", "aiff", "", "audio/noise_stereo.wav"); }
int test_capability_avi() { return CheckCapability("AVI", "avi", ""); }
int test_capability_caf() { return CheckCapability("CAF", "caf", "ALAC"); }
int test_capability_flac() { return CheckCapability("FLAC", "flac", "FLAC"); }
int test_capability_matroska() { return CheckCapability("Matroska", "mka", ""); }
int test_capability_mov() { return CheckCapability("MOV", "mov", "ALAC"); }
int test_capability_mp3() { return CheckCapability("MP3", "mp3", ""); }
int test_capability_mp4() { return CheckCapability("MP4", "m4a", "ALAC"); }
int test_capability_ogg() { return CheckCapability("Ogg", "ogg", "FLAC"); }
int test_capability_opus() { return CheckCapability("Opus", "opus", "Opus"); }
int test_capability_wav() { return CheckCapability("WAV", "wav", ""); }
int test_capability_webm() { return CheckCapability("WebM", "webm", "Opus"); }

int test_container_capability_queries() {
	const auto* mp3 = FindContainer("MP3");
	const auto* matroska = FindContainer("Matroska");
	const auto* wav = FindContainer("WAV");
	const auto* opus = FindContainer("Opus");
	const auto* aac = FindContainer("AAC");
	TEST_REQUIRE(mp3 && matroska && wav && opus && aac);
	for (const auto& edit : EditedTags) {
		TEST_REQUIRE(mp3->Stores(edit.tag));
		TEST_REQUIRE(matroska->Stores(edit.tag));
		TEST_REQUIRE(!aac->Stores(edit.tag));
	}
	TEST_REQUIRE(!mp3->Stores(StreamTag::Title) && !mp3->Stores(StreamTag::Language));
	TEST_REQUIRE(matroska->Stores(StreamTag::Title) && matroska->Stores(StreamTag::Language));
	TEST_REQUIRE(wav->Stores(FileTag::Title) && !wav->Stores(FileTag::AlbumArtist));
	TEST_REQUIRE(opus->SharedTitle() && !matroska->SharedTitle() && !mp3->SharedTitle());
	TEST_REQUIRE(!aac->Stores(StreamTag::Title) && !aac->Stores(StreamTag::Language));
	return 0;
}

int test_file_tag_accessor() {
	auto source = File::Open(StormByte::Safe::String{FixturePath("audio/noise_tagged.mp3").string()});
	TEST_REQUIRE(source);
	const auto& meta = source->Metadata();
	TEST_REQUIRE(meta.Tag(FileTag::Title) == meta.Title());
	TEST_REQUIRE(meta.Tag(FileTag::Artist) == meta.Artist());
	TEST_REQUIRE(meta.Tag(FileTag::Album) == meta.Album());
	TEST_REQUIRE(meta.Tag(FileTag::AlbumArtist) == meta.AlbumArtist());
	TEST_REQUIRE(meta.Tag(FileTag::Composer) == meta.Composer());
	TEST_REQUIRE(meta.Tag(FileTag::Genre) == meta.Genre());
	TEST_REQUIRE(meta.Tag(FileTag::Comment) == meta.Comment());
	TEST_REQUIRE(meta.Tag(FileTag::Copyright) == meta.Copyright());
	TEST_REQUIRE(meta.Tag(FileTag::Date) == meta.Date());
	TEST_REQUIRE(Equals(meta.Tag(FileTag::Track), "3"));
	TEST_REQUIRE(Equals(meta.Tag(FileTag::Disc), "2"));
	return 0;
}

int test_file_tags_flac() { return CheckFileTagEdits("FLAC", "flac", "FLAC"); }
int test_file_tags_matroska() { return CheckFileTagEdits("Matroska", "mka", ""); }
int test_file_tags_mp3() { return CheckFileTagEdits("MP3", "mp3", ""); }
int test_file_tags_mp4() { return CheckFileTagEdits("MP4", "m4a", "ALAC"); }
int test_file_tags_opus() { return CheckFileTagEdits("Opus", "opus", "Opus"); }

int test_ogg_file_title_removed() {
	const auto output = OutputPath("pipeline/metadata-edit/title-removed.opus");
	const int written = WriteAudio(FixturePath("audio/noise_tagged.opus"), output, "",
		[](Transcoder& job, Transcoder::Track&) { job.RemoveTag(FileTag::Title); });
	if (written != 0)
		return written;
	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(!opened->Metadata().Title());
	TEST_REQUIRE(!opened->Streams()[0].Metadata().Title());
	TEST_REQUIRE(opened->Metadata().Artist());
	return 0;
}

int test_ogg_file_title_wins() {
	auto source = File::Open(StormByte::Safe::String{FixturePath("audio/noise_tagged.opus").string()});
	TEST_REQUIRE(source);
	TEST_REQUIRE(source->Metadata().Title());
	const auto output = OutputPath("pipeline/metadata-edit/title-wins.opus");
	const int written = WriteAudio(FixturePath("audio/noise_tagged.opus"), output, "",
		[](Transcoder& job, Transcoder::Track& track) {
			job.Tag(FileTag::Title, "Edited Title");
			track.Title("Losing track title");
		});
	if (written != 0)
		return written;
	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(Equals(opened->Metadata().Title(), "Edited Title"));
	TEST_REQUIRE(Equals(opened->Streams()[0].Metadata().Title(), "Edited Title"));
	return 0;
}

int test_track_tags_encode_matroska() {
	const auto first = OutputPath("pipeline/metadata-edit/encoded-set.mka");
	const int written = WriteAudio(FixturePath("audio/noise_tagged.mp3"), first, "FLAC",
		[](Transcoder&, Transcoder::Track& track) { track.Title("Pista editada").Language("spa"); });
	if (written != 0)
		return written;
	auto edited = File::Open(StormByte::Safe::String{first.string()});
	TEST_REQUIRE(edited);
	TEST_REQUIRE(Equals(edited->Streams()[0].Metadata().Title(), "Pista editada"));
	TEST_REQUIRE(Equals(edited->Streams()[0].Metadata().Language(), "spa"));

	const auto second = OutputPath("pipeline/metadata-edit/encoded-removed.mka");
	TEST_REQUIRE(WriteAudio(first, second, "FLAC",
		[](Transcoder&, Transcoder::Track& track) { track.RemoveTitle().RemoveLanguage(); }) == 0);
	auto removed = File::Open(StormByte::Safe::String{second.string()});
	TEST_REQUIRE(removed);
	TEST_REQUIRE(!removed->Streams()[0].Metadata().Title());
	TEST_REQUIRE(LanguageRemoved(removed->Streams()[0].Metadata()));

	const auto third = OutputPath("pipeline/metadata-edit/encoded-kept.mka");
	TEST_REQUIRE(WriteAudio(first, third, "FLAC",
		[](Transcoder&, Transcoder::Track& track) { track.Title("").Language(""); }) == 0);
	auto kept = File::Open(StormByte::Safe::String{third.string()});
	TEST_REQUIRE(kept);
	TEST_REQUIRE(Equals(kept->Streams()[0].Metadata().Title(), "Pista editada"));
	TEST_REQUIRE(Equals(kept->Streams()[0].Metadata().Language(), "spa"));
	return 0;
}

int test_track_tags_mp4() {
	const auto first = OutputPath("pipeline/metadata-edit/track-set.m4a");
	TEST_REQUIRE(WriteAudio(FixturePath("audio/noise_tagged.mp3"), first, "",
		[](Transcoder&, Transcoder::Track& track) { track.Title("Pista editada").Language("spa"); }) == 0);
	auto edited = File::Open(StormByte::Safe::String{first.string()});
	TEST_REQUIRE(edited);
	TEST_REQUIRE(Equals(edited->Streams()[0].Metadata().Title(), "Pista editada"));
	TEST_REQUIRE(Equals(edited->Streams()[0].Metadata().Language(), "spa"));

	const auto second = OutputPath("pipeline/metadata-edit/track-removed.m4a");
	TEST_REQUIRE(WriteAudio(first, second, "",
		[](Transcoder&, Transcoder::Track& track) { track.RemoveTitle().RemoveLanguage(); }) == 0);
	auto removed = File::Open(StormByte::Safe::String{second.string()});
	TEST_REQUIRE(removed);
	TEST_REQUIRE(!removed->Streams()[0].Metadata().Title());
	TEST_REQUIRE(LanguageRemoved(removed->Streams()[0].Metadata()));
	return 0;
}

int test_track_tags_remux_matroska() {
	const auto input = FixturePath("video/anime_like.mkv");
	auto source = File::Open(StormByte::Safe::String{input.string()});
	TEST_REQUIRE(source);
	const auto output = OutputPath("pipeline/metadata-edit/remuxed.mkv");
	{
		Transcoder job{TestLocation(input), TestLocation(output), MakeLogger(), Duration};
		job.Video(0).Remux();
		job.Audio(1).Remux().Title("Audio editado").Language("spa");
		job.Subtitle(2).Remux().RemoveTitle().RemoveLanguage();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
	}
	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened->Streams().size() == 3);
	const auto& video = opened->Streams()[0].Metadata();
	const auto& audio = opened->Streams()[1].Metadata();
	const auto& subtitle = opened->Streams()[2].Metadata();
	TEST_REQUIRE(video.Title() == source->Streams()[0].Metadata().Title());
	TEST_REQUIRE(Equals(audio.Title(), "Audio editado"));
	TEST_REQUIRE(Equals(audio.Language(), "spa"));
	TEST_REQUIRE(!subtitle.Title());
	TEST_REQUIRE(LanguageRemoved(subtitle));
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_capability_aiff", test_capability_aiff},
		TestEntry{"test_capability_avi", test_capability_avi},
		TestEntry{"test_capability_caf", test_capability_caf},
		TestEntry{"test_capability_flac", test_capability_flac},
		TestEntry{"test_capability_matroska", test_capability_matroska},
		TestEntry{"test_capability_mov", test_capability_mov},
		TestEntry{"test_capability_mp3", test_capability_mp3},
		TestEntry{"test_capability_mp4", test_capability_mp4},
		TestEntry{"test_capability_ogg", test_capability_ogg},
		TestEntry{"test_capability_opus", test_capability_opus},
		TestEntry{"test_capability_wav", test_capability_wav},
		TestEntry{"test_capability_webm", test_capability_webm},
		TestEntry{"test_container_capability_queries", test_container_capability_queries},
		TestEntry{"test_file_tag_accessor", test_file_tag_accessor},
		TestEntry{"test_file_tags_flac", test_file_tags_flac},
		TestEntry{"test_file_tags_matroska", test_file_tags_matroska},
		TestEntry{"test_file_tags_mp3", test_file_tags_mp3},
		TestEntry{"test_file_tags_mp4", test_file_tags_mp4},
		TestEntry{"test_file_tags_opus", test_file_tags_opus},
		TestEntry{"test_ogg_file_title_removed", test_ogg_file_title_removed},
		TestEntry{"test_ogg_file_title_wins", test_ogg_file_title_wins},
		TestEntry{"test_track_tags_encode_matroska", test_track_tags_encode_matroska},
		TestEntry{"test_track_tags_mp4", test_track_tags_mp4},
		TestEntry{"test_track_tags_remux_matroska", test_track_tags_remux_matroska},
	};
	return RunSelectedTest(argc, argv, tests);
}
