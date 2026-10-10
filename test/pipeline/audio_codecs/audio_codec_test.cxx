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
	int CheckAudioCodec(const std::filesystem::path& input, const std::filesystem::path& output,
		std::string_view containerName, std::string_view codecName, std::string_view encoder,
		std::uint8_t channels, std::string_view decoder = {}) {
		TEST_PHASE("opening media file for inspection");
		auto source = File::Open(StormByte::Safe::String{input.string()});
		TEST_REQUIRE(source);
		TEST_REQUIRE(source.value().Streams().size() == 1);
		const auto sourceAudio = source.value().Streams()[0].Audio();
		TEST_REQUIRE(sourceAudio);
		TEST_REQUIRE(sourceAudio.value().Channels() == channels);
		TEST_REQUIRE(sourceAudio.value().SampleRate() == 48000);

		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write})) {
#ifdef STORMBYTE_TEST_REQUIRE_FDK_AAC
			TEST_REQUIRE(encoder != "libfdk_aac");
#endif
			return TEST_SKIP;
		}
		auto logger = MakeLogger();
			TEST_PHASE("creating transcoder");
			Transcoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
		auto track = job.Audio(0);
		track.Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder});
		if (!decoder.empty())
			track.Implementation(ImplementationSide::Decoder, StormByte::Safe::String{decoder});
		if (codecName == "AAC" || codecName == "Vorbis" || codecName == "Opus"
			|| codecName == "MP3" || codecName == "AC-3" || codecName == "E-AC3")
			track.BitRate(channels == 2 ? 192000 : 384000);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		const int result = WaitForOptionalCodec(job);
#ifdef STORMBYTE_TEST_REQUIRE_FDK_AAC
		if (encoder == "libfdk_aac" || decoder == "libfdk_aac")
			TEST_REQUIRE(result != TEST_SKIP);
#endif
		if (result != 0)
			return result;
		TEST_REQUIRE(CheckSingleAudioOutput(output, containerName, codecName, channels, 48000) == 0);
		TEST_PHASE("opening media file for inspection");
		auto encoded = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(encoded);
		const auto& duration = encoded.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}

	int CheckAudioEncode(std::string_view source, std::string_view destination,
		std::string_view containerName, std::string_view codecName, std::string_view encoder,
		std::uint8_t channels) {
		return CheckAudioCodec(FixturePath(source), OutputPath(destination), containerName,
			codecName, encoder, channels);
	}

	int CheckAudioRoundtrip(std::string_view codecName, std::string_view encoder,
		std::string_view decoder, std::string_view encodedName, std::string_view decodedName,
		std::string_view containerName) {
		const auto encoded = OutputPath(encodedName);
		const int result = CheckAudioCodec(FixturePath("audio/noise_stereo.wav"), encoded,
			containerName, codecName, encoder, 2);
		if (result != 0)
			return result;
		return CheckAudioCodec(encoded, OutputPath(decodedName), "Matroska", "FLAC",
			"flac", 2, decoder);
	}
}

int test_fdk_aac_encodes_stereo_mp4() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/fdk-stereo.m4a",
		"MP4", "AAC", "libfdk_aac", 2);
}

int test_fdk_aac_encodes_surround_mp4() {
	return CheckAudioEncode("audio/noise_51.opus", "pipeline/audio-codecs/fdk-surround.m4a",
		"MP4", "AAC", "libfdk_aac", 6);
}

int test_fdk_aac_decodes_surround_to_flac() {
	return CheckAudioCodec(FixturePath("audio/noise_51.m4a"),
		OutputPath("pipeline/audio-codecs/fdk-decoded.mka"), "Matroska", "FLAC", "flac", 6, "libfdk_aac");
}

int test_libvorbis_encodes_stereo_ogg() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/vorbis-stereo.ogg",
		"Ogg", "Vorbis", "libvorbis", 2);
}

int test_libvorbis_encodes_surround_matroska() {
	return CheckAudioEncode("audio/noise_51.opus", "pipeline/audio-codecs/vorbis-surround.mka",
		"Matroska", "Vorbis", "libvorbis", 6);
}

int test_libvorbis_decode_roundtrip() {
	return CheckAudioRoundtrip("Vorbis", "libvorbis", "libvorbis",
		"pipeline/audio-codecs/vorbis-roundtrip.ogg", "pipeline/audio-codecs/vorbis-decoded.mka", "Ogg");
}

int test_libopus_encodes_stereo_opus() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/opus-stereo.opus",
		"Opus", "Opus", "libopus", 2);
}

int test_libopus_decodes_surround_to_flac() {
	return CheckAudioCodec(FixturePath("audio/noise_51.opus"),
		OutputPath("pipeline/audio-codecs/opus-decoded.mka"), "Matroska", "FLAC", "flac", 6, "libopus");
}

int test_lame_encodes_stereo_mp3() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/lame-stereo.mp3",
		"MP3", "MP3", "libmp3lame", 2);
}

int test_flac_encodes_stereo_matroska() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/flac-stereo.mka",
		"Matroska", "FLAC", "flac", 2);
}

int test_flac_encodes_surround_matroska() {
	return CheckAudioEncode("audio/noise_51.opus", "pipeline/audio-codecs/flac-surround.mka",
		"Matroska", "FLAC", "flac", 6);
}

int test_flac_decode_roundtrip() {
	return CheckAudioRoundtrip("FLAC", "flac", "flac",
		"pipeline/audio-codecs/flac-roundtrip.mka", "pipeline/audio-codecs/flac-decoded.mka", "Matroska");
}

int test_alac_encodes_stereo_mp4() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/alac-stereo.m4a",
		"MP4", "ALAC", "alac", 2);
}

int test_alac_encodes_surround_mp4() {
	return CheckAudioEncode("audio/noise_51.opus", "pipeline/audio-codecs/alac-surround.m4a",
		"MP4", "ALAC", "alac", 6);
}

int test_alac_decode_roundtrip() {
	return CheckAudioRoundtrip("ALAC", "alac", "alac",
		"pipeline/audio-codecs/alac-roundtrip.m4a", "pipeline/audio-codecs/alac-decoded.mka", "MP4");
}

int test_ac3_fixed_encodes_stereo_matroska() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/ac3-fixed.mka",
		"Matroska", "AC-3", "ac3_fixed", 2);
}

int test_eac3_encodes_stereo_matroska() {
	return CheckAudioEncode("audio/noise_stereo.wav", "pipeline/audio-codecs/eac3-stereo.mka",
		"Matroska", "E-AC3", "eac3", 2);
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_fdk_aac_encodes_stereo_mp4", test_fdk_aac_encodes_stereo_mp4},
		TestEntry{"test_fdk_aac_encodes_surround_mp4", test_fdk_aac_encodes_surround_mp4},
		TestEntry{"test_fdk_aac_decodes_surround_to_flac", test_fdk_aac_decodes_surround_to_flac},
		TestEntry{"test_libvorbis_encodes_stereo_ogg", test_libvorbis_encodes_stereo_ogg},
		TestEntry{"test_libvorbis_encodes_surround_matroska", test_libvorbis_encodes_surround_matroska},
		TestEntry{"test_libvorbis_decode_roundtrip", test_libvorbis_decode_roundtrip},
		TestEntry{"test_libopus_encodes_stereo_opus", test_libopus_encodes_stereo_opus},
		TestEntry{"test_libopus_decodes_surround_to_flac", test_libopus_decodes_surround_to_flac},
		TestEntry{"test_lame_encodes_stereo_mp3", test_lame_encodes_stereo_mp3},
		TestEntry{"test_flac_encodes_stereo_matroska", test_flac_encodes_stereo_matroska},
		TestEntry{"test_flac_encodes_surround_matroska", test_flac_encodes_surround_matroska},
		TestEntry{"test_flac_decode_roundtrip", test_flac_decode_roundtrip},
		TestEntry{"test_alac_encodes_stereo_mp4", test_alac_encodes_stereo_mp4},
		TestEntry{"test_alac_encodes_surround_mp4", test_alac_encodes_surround_mp4},
		TestEntry{"test_alac_decode_roundtrip", test_alac_decode_roundtrip},
		TestEntry{"test_ac3_fixed_encodes_stereo_matroska", test_ac3_fixed_encodes_stereo_matroska},
		TestEntry{"test_eac3_encodes_stereo_matroska", test_eac3_encodes_stereo_matroska},
	};
	return RunSelectedTest(argc, argv, tests);
}
