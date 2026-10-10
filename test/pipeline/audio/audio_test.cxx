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

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
static int CheckAutomaticSurroundConversion(std::string_view codecName, std::string_view destination) {
	const auto input = FixturePath("audio/noise_71.wav");
	TEST_PHASE("opening media file for inspection");
	auto source = File::Open(StormByte::Safe::String{input.string()});
	TEST_REQUIRE(source);
	TEST_REQUIRE(source.value().Streams().size() == 1);
	const auto sourceStream = source.value().Streams()[0];
	TEST_REQUIRE(sourceStream.Type() == StormByte::Multimedia::Type::Audio);
	TEST_REQUIRE(sourceStream.Codec().Name() == "PCM S16 LE"sv);
	const auto sourceAudio = sourceStream.Audio();
	TEST_REQUIRE(sourceAudio);
	TEST_REQUIRE(sourceAudio.value().Channels() == 8);
	TEST_REQUIRE(sourceAudio.value().Layout() == Property::ChannelLayout::SevenPointOne);
	TEST_REQUIRE(sourceAudio.value().SampleRate() == 48000);

	auto codec = Registry::Instance().FindCodec(codecName);
	TEST_REQUIRE(codec);
	if (!codec.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath(destination);
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
	// AC-3/E-AC3 may automatically convert 7.1 to 5.1 without an explicit downmix filter.
	job.Audio(0).Codec(codec.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	TEST_REQUIRE(CheckSingleAudioOutput(output, "Matroska", codecName, 6, 48000) == 0);
	TEST_PHASE("opening media file for inspection");
	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded);
	const auto outputAudio = encoded.value().Streams()[0].Audio();
	TEST_REQUIRE(outputAudio);
	TEST_REQUIRE(outputAudio.value().Layout() == Property::ChannelLayout::FivePointOne);
	return 0;
}
}

int test_transcoder_extracts_single_ac3_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-1.ac3");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/bluray_like_hdr10.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Audio(1).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "AC-3", "AC-3", 6, 48000);
}

int test_transcoder_extracts_single_aac_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-2.m4a");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/bluray_like_hdr10.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Audio(2).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_remuxes_single_opus_file_to_mka() {
	const auto output = OutputPath("pipeline/opus-remux.mka");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_opus_source_51.opus")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "Matroska", "Opus", 6, 48000);
}

int test_transcoder_remuxes_wav_file_to_wav() {
	const auto output = OutputPath("pipeline/wav-remux.wav");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_stereo.wav")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "WAV", "PCM S16 LE", 2, 48000);
}

int test_transcoder_opus_encode_output_file_properties() {
	auto& registry = Registry::Instance();
	auto opus = registry.FindCodec("Opus");
	TEST_REQUIRE(opus);
	if (!opus.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-opus.mka");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_opus_source_51.opus")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Codec(opus.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	TEST_PHASE("opening media file for inspection");
	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Container().Name() == "Matroska");
	TEST_REQUIRE(opened.value().Streams().size() == 1);
	const auto audioStream = opened.value().Streams()[0];
	TEST_REQUIRE(audioStream.Type() == StormByte::Multimedia::Type::Audio);
	TEST_REQUIRE(audioStream.Codec().Name() == "Opus"sv);
	auto audio = audioStream.Audio();
	TEST_REQUIRE(audio);
	TEST_REQUIRE(audio.value().Channels() == 6);
	TEST_REQUIRE(audio.value().SampleRate() == 48000);
	return 0;
}

int test_transcoder_aac_encode_from_stereo_wav() {
	auto& registry = Registry::Instance();
	auto aac = registry.FindCodec("AAC");
	TEST_REQUIRE(aac);
	if (!aac.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-aac.m4a");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_51.opus")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Codec(aac.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_rejects_51_audio_to_mp3_without_downmix() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_51.opus")),
			TestLocation(OutputPath("pipeline/rejected-51.mp3")), logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	return WaitForTranscoderFailure(job, "unsupported channel layout");
}

int test_transcoder_allows_71_to_ac3_51_without_downmix_filter() {
	return CheckAutomaticSurroundConversion("AC-3", "pipeline/encoded-71-to-ac3-51.mka");
}

int test_transcoder_allows_71_to_eac3_51_without_downmix_filter() {
	return CheckAutomaticSurroundConversion("E-AC3", "pipeline/encoded-71-to-eac3-51.mka");
}

int test_transcoder_encodes_stereo_audio_to_mp3() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-stereo.mp3");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_stereo.mp3")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP3", "MP3", 2, 48000);
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_extracts_single_ac3_track_from_bluray_mkv", test_transcoder_extracts_single_ac3_track_from_bluray_mkv},
		TestEntry{"test_transcoder_extracts_single_aac_track_from_bluray_mkv", test_transcoder_extracts_single_aac_track_from_bluray_mkv},
		TestEntry{"test_transcoder_remuxes_single_opus_file_to_mka", test_transcoder_remuxes_single_opus_file_to_mka},
		TestEntry{"test_transcoder_remuxes_wav_file_to_wav", test_transcoder_remuxes_wav_file_to_wav},
		TestEntry{"test_transcoder_opus_encode_output_file_properties", test_transcoder_opus_encode_output_file_properties},
		TestEntry{"test_transcoder_aac_encode_from_stereo_wav", test_transcoder_aac_encode_from_stereo_wav},
		TestEntry{"test_transcoder_rejects_51_audio_to_mp3_without_downmix", test_transcoder_rejects_51_audio_to_mp3_without_downmix},
		TestEntry{"test_transcoder_allows_71_to_ac3_51_without_downmix_filter", test_transcoder_allows_71_to_ac3_51_without_downmix_filter},
		TestEntry{"test_transcoder_allows_71_to_eac3_51_without_downmix_filter", test_transcoder_allows_71_to_eac3_51_without_downmix_filter},
		TestEntry{"test_transcoder_encodes_stereo_audio_to_mp3", test_transcoder_encodes_stereo_audio_to_mp3},
	};
	return RunSelectedTest(argc, argv, tests);
}
