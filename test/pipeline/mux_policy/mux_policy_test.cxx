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
#include <StormByte/multimedia/pipeline/filters/audio/resample.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <algorithm>
#include <array>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	int CheckCoverMetadata(const File& source, const File& output) {
		TEST_REQUIRE(output.Streams().size() == 1);
		TEST_REQUIRE(output.Streams()[0].Type() == StormByte::Multimedia::Type::Audio);
		const auto& before = source.Metadata();
		const auto& after = output.Metadata();
		TEST_REQUIRE(before.Title() && after.Title() && *before.Title() == *after.Title());
		TEST_REQUIRE(before.Artist() && after.Artist() && *before.Artist() == *after.Artist());
		TEST_REQUIRE(before.Album() && after.Album() && *before.Album() == *after.Album());
		TEST_REQUIRE(before.AlbumArtist() == after.AlbumArtist());
		TEST_REQUIRE(before.Composer() == after.Composer());
		TEST_REQUIRE(before.Genre() == after.Genre());
		TEST_REQUIRE(before.Comment() == after.Comment());
		TEST_REQUIRE(before.Date() == after.Date());
		TEST_REQUIRE(before.Track() == after.Track());
		TEST_REQUIRE(before.Disc() == after.Disc());
		TEST_REQUIRE(source.Attachments().size() == 2);
		TEST_REQUIRE(output.Attachments().size() == source.Attachments().size());
		for (const auto& cover : source.Attachments()) {
			bool found = false;
			for (const auto& copy : output.Attachments()) {
				const auto& original = cover.Payload().Data();
				const auto& bytes = copy.Payload().Data();
				found = found || (cover.MimeType() == copy.MimeType() && original.size() == bytes.size()
					&& std::equal(original.begin(), original.end(), bytes.begin(), bytes.end()));
			}
			TEST_REQUIRE(found);
		}
		return 0;
	}

	int CheckAudioCovers(std::string_view suffix, std::string_view codecName, std::string_view implementation) {
		const bool remuxOnly = implementation.empty();
		const auto input = FixturePath(remuxOnly ? "audio/noise_tagged_covers.aiff" : "audio/noise_tagged_covers.mp3");
		auto source = File::Open(StormByte::Safe::String{input.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(source->Attachments().size() == 2);
		TEST_REQUIRE(!source->Attachments()[0].FileName() || source->Attachments()[0].FileName()->empty());
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!remuxOnly && !codec->get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(std::string{"pipeline/mux-policy/covered."} + std::string{suffix});
		{
			Transcoder job{TestLocation(input), TestLocation(output), MakeLogger(), 2000000000LL};
			auto track = job.Audio(0);
			if (remuxOnly)
				track.Remux();
			else
				track.Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{implementation});
			if (codecName == "Opus") {
				track.Filter<Filter::Audio::Resample>(MakeLogger(), 48000);
				track.BitRate(192000);
			}
			if (codecName == "Vorbis")
				track.BitRate(192000);
			// No Attachments() call: artwork travels with the ordinary file tags.
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(CheckCoverMetadata(*source, *opened) == 0);
		const auto remuxed = OutputPath(std::string{"pipeline/mux-policy/remuxed."} + std::string{suffix});
		{
			Transcoder job{TestLocation(output), TestLocation(remuxed), MakeLogger(), 2000000000LL};
			job.Audio(0).Remux();
			job.Attachments("image/*");
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		auto copy = File::Open(StormByte::Safe::String{remuxed.string()});
		TEST_REQUIRE(copy);
		TEST_REQUIRE(CheckCoverMetadata(*source, *copy) == 0);
		const auto repeated = OutputPath(std::string{"pipeline/mux-policy/repeated."} + std::string{suffix});
		{
			Transcoder job{TestLocation(remuxed), TestLocation(repeated), MakeLogger(), 2000000000LL};
			job.Audio(0).Remux();
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		auto final = File::Open(StormByte::Safe::String{repeated.string()});
		TEST_REQUIRE(final);
		return CheckCoverMetadata(*source, *final);
	}

	int CheckSimpleOutput(std::string_view destination, std::string_view containerName,
		std::string_view codecName) {
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(destination);
		{
			auto logger = MakeLogger();
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(FixturePath("audio/noise_stereo.wav")), TestLocation(output), logger, 2000000000LL};
			auto track = job.Audio(0);
			track.Codec(codec.value().get());
			if (codecName == "Vorbis")
				track.BitRate(192000);
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("starting transcoder run");
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		TEST_REQUIRE(CheckSingleAudioOutput(output, containerName, codecName, 2, 48000) == 0);
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened->Attachments().empty());
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}
}

int test_generic_muxer_writes_flac() {
	return CheckSimpleOutput("pipeline/mux-policy/native.flac", "FLAC", "FLAC");
}

int test_audio_cover_opus() { return CheckAudioCovers("opus", "Opus", "libopus"); }
int test_audio_cover_vorbis() { return CheckAudioCovers("ogg", "Vorbis", "libvorbis"); }
int test_audio_cover_ogg_flac() { return CheckAudioCovers("oga", "FLAC", "flac"); }
int test_audio_cover_flac() { return CheckAudioCovers("flac", "FLAC", "flac"); }
int test_audio_cover_mp3() { return CheckAudioCovers("mp3", "MP3", "libmp3lame"); }
int test_audio_cover_aiff() { return CheckAudioCovers("aiff", "PCM S16 BE", ""); }
int test_audio_cover_aac_m4a() { return CheckAudioCovers("m4a", "AAC", "aac"); }
int test_audio_cover_alac_m4a() { return CheckAudioCovers("m4a", "ALAC", "alac"); }
int test_audio_cover_matroska() { return CheckAudioCovers("mka", "FLAC", "flac"); }

int test_generic_muxer_writes_flac_in_oga() {
	return CheckSimpleOutput("pipeline/mux-policy/audio.oga", "Ogg", "FLAC");
}

int test_generic_muxer_writes_vorbis_in_ogg() {
	return CheckSimpleOutput("pipeline/mux-policy/audio.ogg", "Ogg", "Vorbis");
}

int test_generic_muxer_writes_vorbis_in_ogv() {
	return CheckSimpleOutput("pipeline/mux-policy/audio.ogv", "Ogg", "Vorbis");
}

int test_generic_muxer_writes_alac_in_caf() {
	return CheckSimpleOutput("pipeline/mux-policy/apple.caf", "CAF", "ALAC");
}

int test_generic_muxer_writes_eac3() {
	return CheckSimpleOutput("pipeline/mux-policy/dolby.eac3", "E-AC3", "E-AC3");
}

int test_generic_flac_rejects_aac_remux() {
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_51.m4a")),
			TestLocation(OutputPath("pipeline/mux-policy/rejected-aac.flac")), logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	return WaitForTranscoderFailure(job);
}

int test_audio_cover_unsupported_caf() {
	auto codec = Registry::Instance().FindCodec("ALAC");
	TEST_REQUIRE(codec);
	const auto output = OutputPath("pipeline/mux-policy/no-picture-support.caf");
	Transcoder job{TestLocation(FixturePath("audio/noise_tagged_covers.mp3")), TestLocation(output), MakeLogger(), 2000000000LL};
	job.Audio(0).Codec(codec->get());
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	auto file = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(file);
	TEST_REQUIRE(file->Streams().size() == 1);
	TEST_REQUIRE(file->Streams()[0].Codec().Name() == "ALAC");
	TEST_REQUIRE(file->Attachments().empty());
	return 0;
}

int test_ogg_policy_rejects_non_image_attachments() {
	Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
		TestLocation(OutputPath("pipeline/mux-policy/rejected-font.opus")), MakeLogger(), 2000000000LL};
	job.Audio(1).Remux();
	job.Attachments();
	job.Run();
	return WaitForTranscoderFailure(job, "Ogg cover must be");
}

int test_webm_policy_rejects_attachments() {
	auto codec = Registry::Instance().FindCodec("VP9");
	TEST_REQUIRE(codec);
	if (!codec.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/mux-policy/rejected-attachment.webm")), logger, 2000000000LL};
	job.Video(0).Codec(codec.value().get());
	job.Attachments();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	return WaitForTranscoderFailure(job, "WebM output does not support file attachments");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_audio_cover_unsupported_caf", test_audio_cover_unsupported_caf},
		TestEntry{"test_ogg_policy_rejects_non_image_attachments", test_ogg_policy_rejects_non_image_attachments},
		TestEntry{"test_audio_cover_opus", test_audio_cover_opus},
		TestEntry{"test_audio_cover_vorbis", test_audio_cover_vorbis},
		TestEntry{"test_audio_cover_ogg_flac", test_audio_cover_ogg_flac},
		TestEntry{"test_audio_cover_flac", test_audio_cover_flac},
		TestEntry{"test_audio_cover_mp3", test_audio_cover_mp3},
		TestEntry{"test_audio_cover_aiff", test_audio_cover_aiff},
		TestEntry{"test_audio_cover_aac_m4a", test_audio_cover_aac_m4a},
		TestEntry{"test_audio_cover_alac_m4a", test_audio_cover_alac_m4a},
		TestEntry{"test_audio_cover_matroska", test_audio_cover_matroska},
		TestEntry{"test_generic_muxer_writes_flac", test_generic_muxer_writes_flac},
		TestEntry{"test_generic_muxer_writes_flac_in_oga", test_generic_muxer_writes_flac_in_oga},
		TestEntry{"test_generic_muxer_writes_vorbis_in_ogg", test_generic_muxer_writes_vorbis_in_ogg},
		TestEntry{"test_generic_muxer_writes_vorbis_in_ogv", test_generic_muxer_writes_vorbis_in_ogv},
		TestEntry{"test_generic_muxer_writes_alac_in_caf", test_generic_muxer_writes_alac_in_caf},
		TestEntry{"test_generic_muxer_writes_eac3", test_generic_muxer_writes_eac3},
		TestEntry{"test_generic_flac_rejects_aac_remux", test_generic_flac_rejects_aac_remux},
		TestEntry{"test_webm_policy_rejects_attachments", test_webm_policy_rejects_attachments},
	};
	return RunSelectedTest(argc, argv, tests);
}
