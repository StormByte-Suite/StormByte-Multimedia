#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	int CheckSimpleOutput(std::string_view destination, std::string_view containerName,
		std::string_view codecName) {
		auto codec = Registry::Instance().FindCodec(codecName);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath(destination);
		{
			QuietCout quietCout;
			auto logger = MakeSilentLogger();
			Transcoder job{FixturePath("audio/noise_stereo.wav"), output, logger, 2000000000LL};
			job.Audio(0).Codec(codec.value().get());
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		TEST_REQUIRE(CheckSingleAudioOutput(output, containerName, codecName, 2, 48000) == 0);
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}
}

int test_generic_muxer_writes_flac() {
	return CheckSimpleOutput("pipeline/mux-policy/native.flac", "FLAC", "FLAC");
}

int test_generic_muxer_writes_flac_in_oga() {
	return CheckSimpleOutput("pipeline/mux-policy/audio.oga", "Ogg", "FLAC");
}

int test_generic_muxer_writes_alac_in_caf() {
	return CheckSimpleOutput("pipeline/mux-policy/apple.caf", "CAF", "ALAC");
}

int test_generic_muxer_writes_eac3() {
	return CheckSimpleOutput("pipeline/mux-policy/dolby.eac3", "E-AC3", "E-AC3");
}

int test_generic_flac_rejects_aac_remux() {
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_51.m4a"),
		OutputPath("pipeline/mux-policy/rejected-aac.flac"), logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job);
}

int test_webm_policy_rejects_attachments() {
	auto codec = Registry::Instance().FindCodec("VP9");
	TEST_REQUIRE(codec);
	if (!codec.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/anime_like.mkv"),
		OutputPath("pipeline/mux-policy/rejected-attachment.webm"), logger, 2000000000LL};
	job.Video(0).Codec(codec.value().get());
	job.Attachments();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job, "WebM output does not support file attachments");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_generic_muxer_writes_flac", test_generic_muxer_writes_flac},
		TestEntry{"test_generic_muxer_writes_flac_in_oga", test_generic_muxer_writes_flac_in_oga},
		TestEntry{"test_generic_muxer_writes_alac_in_caf", test_generic_muxer_writes_alac_in_caf},
		TestEntry{"test_generic_muxer_writes_eac3", test_generic_muxer_writes_eac3},
		TestEntry{"test_generic_flac_rejects_aac_remux", test_generic_flac_rejects_aac_remux},
		TestEntry{"test_webm_policy_rejects_attachments", test_webm_policy_rejects_attachments},
	};
	return RunSelectedTest(argc, argv, tests);
}
