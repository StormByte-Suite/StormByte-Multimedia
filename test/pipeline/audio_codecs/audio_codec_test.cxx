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
		QuietCout quietCout;
		auto logger = MakeSilentLogger();
		Transcoder job{input, output, logger, 2000000000LL};
		auto track = job.Audio(0);
		track.Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{encoder});
		if (!decoder.empty())
			track.Implementation(ImplementationSide::Decoder, StormByte::Safe::String{decoder});
		if (codecName == "AAC" || codecName == "Vorbis" || codecName == "Opus"
			|| codecName == "MP3" || codecName == "AC-3" || codecName == "E-AC3")
			track.BitRate(channels == 2 ? 192000 : 384000);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		const int result = WaitForOptionalCodec(job);
#ifdef STORMBYTE_TEST_REQUIRE_FDK_AAC
		if (encoder == "libfdk_aac" || decoder == "libfdk_aac")
			TEST_REQUIRE(result != TEST_SKIP);
#endif
		if (result != 0)
			return result;
		TEST_REQUIRE(CheckSingleAudioOutput(output, containerName, codecName, channels, 48000) == 0);
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
