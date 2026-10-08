#include "helpers.hxx"

#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <chrono>
#include <thread>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

	static bool IsTerminal(Status status) {
		return status == Status::Done || status == Status::Error || status == Status::Aborted;
	}

	int WaitForTranscoder(Transcoder& job) {
		const auto deadline = std::chrono::steady_clock::now() + 20s;
		while (!IsTerminal(job.Status()) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		if (!IsTerminal(job.Status())) {
			job.Cancel();
			return 1;
		}
		if (job.Status() != Status::Done) {
			const auto error = job.Error();
			std::cerr << "[DETAIL] Transcoder ended with status " << static_cast<int>(job.Status()) << ": "
				<< error.value_or(StormByte::Safe::String{"no error message"}) << std::endl;
			return 1;
		}
		return 0;
	}

	int CheckTranscoderConfigured(Transcoder& job) {
		if (!job.Failed())
			return 0;
		const auto error = job.Error();
		std::cerr << "[DETAIL] Transcoder configuration: "
			<< error.value_or(StormByte::Safe::String{"no error message"}) << std::endl;
		return 1;
	}

	int WaitForTranscoderFailure(Transcoder& job, std::string_view expectedMessage) {
		const auto deadline = std::chrono::steady_clock::now() + 20s;
		while (!IsTerminal(job.Status()) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		if (!IsTerminal(job.Status())) {
			job.Cancel();
			std::cerr << "[DETAIL] Expected failure timed out" << std::endl;
			return 1;
		}
		TEST_REQUIRE(job.Status() == Status::Error);
		TEST_REQUIRE(job.Failed());
		const auto error = job.Error();
		TEST_REQUIRE(error && !error->empty());
		if (!expectedMessage.empty()) {
			if (TestView(error.value()).find(expectedMessage) == std::string_view::npos)
				std::cerr << "[DETAIL] Unexpected failure: " << error.value() << std::endl;
			TEST_REQUIRE(TestView(error.value()).find(expectedMessage) != std::string_view::npos);
		}
		return 0;
	}

	StormByte::Safe::Shared<StormByte::Logger::Log> MakeSilentLogger() {
		return StormByte::Safe::MakeShared<StormByte::Logger::ThreadedLog>(
			std::cout, StormByte::Logger::Level::Debug, "[%L]");
	}

	int WaitForManualPipeline(Step& last) {
		const auto deadline = std::chrono::steady_clock::now() + 20s;
		while (last.Status() != State::Stopped && last.Status() != State::Failed &&
			std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		if (last.Status() != State::Stopped)
			return 1;
		return 0;
	}

	int CheckSingleAudioOutput(const std::filesystem::path& output, std::string_view container,
		std::string_view codec, std::uint8_t channels, std::uint32_t sampleRate,
		std::string_view language) {
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == container);
		const auto streams = opened.value().Streams();
		TEST_REQUIRE(streams.size() == 1);
		const auto audioStream = streams[0];
		TEST_REQUIRE(audioStream.Index() == 0);
		TEST_REQUIRE(audioStream.Type() == StormByte::Multimedia::Type::Audio);
		TEST_REQUIRE(audioStream.Codec().Name() == codec);
		if (!language.empty()) {
			const auto actualLanguage = audioStream.Metadata().Language();
			TEST_REQUIRE(actualLanguage);
			TEST_REQUIRE(TestView(actualLanguage.value()) == language);
		}
		const auto audio = audioStream.Audio();
		TEST_REQUIRE(audio);
		TEST_REQUIRE(audio.value().Channels() == channels);
		TEST_REQUIRE(audio.value().SampleRate() == sampleRate);
		return 0;
	}
