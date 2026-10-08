#include "../helpers.hxx"

#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
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
static int CheckTranscoderDecoderPin(std::string_view pin, std::string_view destination,
	std::string_view expectedError = {}) {
	auto codec = Registry::Instance().FindCodec("AC-3");
	TEST_REQUIRE(codec && codec.value().get().HasAccess(Access{Operation::Write}));
	const auto output = OutputPath(destination);
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_stereo.wav"), output, logger, 2000000000LL};
	job.Audio(0).Codec(codec.value().get())
		.Implementation(ImplementationSide::Decoder, StormByte::Safe::String{pin})
		.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"ac3"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	if (!expectedError.empty())
		return WaitForTranscoderFailure(job, expectedError);
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "Matroska", "AC-3", 2, 48000);
}

static int CheckManualDecoderPin(std::string_view pin, std::string_view destination,
	std::string_view expectedError = {}) {
	auto codec = Registry::Instance().FindCodec("AC-3");
	TEST_REQUIRE(codec && codec.value().get().HasAccess(Access{Operation::Write}));
	const auto output = OutputPath(destination);
	Plan plan{FixturePath("audio/noise_stereo.wav"), output, 2000000000LL};
	Config::Audio audio;
	audio.Codec(codec.value().get());
	Config::Implementation implementation;
	implementation.Decoder = StormByte::Safe::String{pin};
	audio.Implementation(std::move(implementation));
	plan.add(Track{0, std::move(audio)});
	TEST_REQUIRE(plan.Check());
	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Demuxer demuxer{noLogger};
	Decoder decoder{noLogger, 0};
	Encoder encoder{noLogger, 0, codec.value().get()};
	Muxer muxer{noLogger};
	std::move(plan) >> demuxer;
	demuxer >> muxer;
	demuxer >> decoder;
	decoder >> encoder;
	encoder >> muxer;
	TEST_REQUIRE(decoder.Implementation());
	TEST_REQUIRE(TestView(decoder.Implementation().value()) == pin);
	if (!expectedError.empty()) {
		const auto deadline = std::chrono::steady_clock::now() + 20s;
		while (!decoder.Failed() && decoder.Status() != State::Stopped
			&& std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		TEST_REQUIRE(decoder.Failed());
		TEST_REQUIRE(decoder.Error());
		TEST_REQUIRE(TestView(decoder.Error().value()).find(expectedError) != std::string_view::npos);
		return 0;
	}
	const auto deadline = std::chrono::steady_clock::now() + 20s;
	while (muxer.Status() != State::Stopped && !muxer.Failed()
		&& !demuxer.Failed() && !decoder.Failed() && !encoder.Failed()
		&& std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(10ms);
	for (const Step* stage : std::array<const Step*, 4>{&demuxer, &decoder, &encoder, &muxer}) {
		if (muxer.Status() != State::Stopped)
			std::cerr << "[DETAIL] Manual timeout state=" << static_cast<int>(stage->Status()) << " "
				<< static_cast<StormByte::Safe::String>(*stage->Telemetry()) << std::endl;
		if (stage->Failed())
			std::cerr << "[DETAIL] Manual decoder-pin pipeline: "
				<< stage->Error().value_or(StormByte::Safe::String{"no error message"}) << std::endl;
	}
	TEST_REQUIRE(muxer.Status() == State::Stopped);
	TEST_REQUIRE(!demuxer.Failed() && !decoder.Failed() && !encoder.Failed());
	return CheckSingleAudioOutput(output, "Matroska", "AC-3", 2, 48000);
}
}

int test_transcoder_selects_decoder_implementation() {
	return CheckTranscoderDecoderPin("pcm_s16le", "pipeline/pinned-decoder.mka");
}

int test_transcoder_rejects_missing_decoder_implementation() {
	return CheckTranscoderDecoderPin("stormbyte_nonexistent_decoder", "pipeline/missing-decoder.mka",
		"decoder implementation is unavailable");
}

int test_transcoder_rejects_mismatched_decoder_implementation() {
	return CheckTranscoderDecoderPin("hevc", "pipeline/mismatched-decoder.mka",
		"decoder implementation does not match source codec");
}

int test_manual_selects_plan_decoder_implementation() {
	return CheckManualDecoderPin("pcm_s16le", "pipeline/manual-pinned-decoder.mka");
}

int test_manual_rejects_missing_decoder_implementation() {
	return CheckManualDecoderPin("stormbyte_nonexistent_decoder", "pipeline/manual-missing-decoder.mka",
		"decoder implementation is unavailable");
}

int test_manual_rejects_mismatched_decoder_implementation() {
	return CheckManualDecoderPin("hevc", "pipeline/manual-mismatched-decoder.mka",
		"decoder implementation does not match source codec");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_selects_decoder_implementation", test_transcoder_selects_decoder_implementation},
		TestEntry{"test_transcoder_rejects_missing_decoder_implementation", test_transcoder_rejects_missing_decoder_implementation},
		TestEntry{"test_transcoder_rejects_mismatched_decoder_implementation", test_transcoder_rejects_mismatched_decoder_implementation},
		TestEntry{"test_manual_selects_plan_decoder_implementation", test_manual_selects_plan_decoder_implementation},
		TestEntry{"test_manual_rejects_missing_decoder_implementation", test_manual_rejects_missing_decoder_implementation},
		TestEntry{"test_manual_rejects_mismatched_decoder_implementation", test_manual_rejects_mismatched_decoder_implementation},
	};
	return RunSelectedTest(argc, argv, tests);
}
