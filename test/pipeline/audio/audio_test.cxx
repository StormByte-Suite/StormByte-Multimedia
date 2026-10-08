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
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{input, output, logger, 2000000000LL};
	// AC-3/E-AC3 may automatically convert 7.1 to 5.1 without an explicit downmix filter.
	job.Audio(0).Codec(codec.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	TEST_REQUIRE(CheckSingleAudioOutput(output, "Matroska", codecName, 6, 48000) == 0);
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
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/bluray_like_hdr10.mkv"), output, logger, 2000000000LL};
	job.Audio(1).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "AC-3", "AC-3", 6, 48000);
}

int test_transcoder_extracts_single_aac_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-2.m4a");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/bluray_like_hdr10.mkv"), output, logger, 2000000000LL};
	job.Audio(2).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_remuxes_single_opus_file_to_mka() {
	const auto output = OutputPath("pipeline/opus-remux.mka");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_opus_source_51.opus"), output, logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "Matroska", "Opus", 6, 48000);
}

int test_transcoder_remuxes_wav_file_to_wav() {
	const auto output = OutputPath("pipeline/wav-remux.wav");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_stereo.wav"), output, logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
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
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_opus_source_51.opus"), output, logger, 2000000000LL};
	job.Audio(0).Codec(opus.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

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
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_51.opus"), output, logger, 2000000000LL};
	job.Audio(0).Codec(aac.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_rejects_51_audio_to_mp3_without_downmix() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_51.opus"),
		OutputPath("pipeline/rejected-51.mp3"), logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
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
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_stereo.mp3"), output, logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
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
