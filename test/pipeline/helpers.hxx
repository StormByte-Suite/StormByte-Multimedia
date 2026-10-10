#pragma once

#include "../test_helpers.hxx"

#include <StormByte/safe/pointers.hxx>

#include <cstdint>

/**
 * @brief StormByte library facilities.
 */
namespace StormByte {
	/**
	 * @brief Logging facilities.
	 */
	namespace Logger {
		/**
		 * @brief Base logger shared by the pipeline tests.
		 */
		class Log;
	}

	/**
	 * @brief Multimedia inspection and processing facilities.
	 */
	namespace Multimedia {
		/**
		 * @brief Demux / decode / filter / encode / mux types.
		 */
		namespace Pipeline {
			/**
			 * @brief Asynchronous transcoding job used by the pipeline tests.
			 */
			class Transcoder;

			/**
			 * @brief Worker stage used by the manual pipeline tests.
			 */
			class Step;
		}
	}
}

/**
 * @brief Waits for a transcoder to complete successfully within the test deadline.
 * @param job Transcoder to observe and cancel on timeout.
 * @return Zero on success, one on failure or timeout.
 */
int WaitForTranscoder(StormByte::Multimedia::Pipeline::Transcoder& job);

/**
 * @brief Waits for a pinned codec job, skipping only unavailable implementations.
 * @param job Transcoder with explicit encoder or decoder selection.
 * @return Zero on success, TEST_SKIP for an unavailable implementation, one otherwise.
 */
int WaitForOptionalCodec(StormByte::Multimedia::Pipeline::Transcoder& job);

/**
 * @brief Reports a failed transcoder configuration.
 * @param job Transcoder whose configuration is checked.
 * @return Zero for a valid configuration, one on failure.
 */
int CheckTranscoderConfigured(StormByte::Multimedia::Pipeline::Transcoder& job);

/**
 * @brief Waits for an expected transcoder failure within the test deadline.
 * @param job Transcoder to observe and cancel on timeout.
 * @param expectedMessage Optional substring required in the error message.
 * @return Zero for the expected failure, one otherwise.
 */
int WaitForTranscoderFailure(StormByte::Multimedia::Pipeline::Transcoder& job,
	std::string_view expectedMessage = {});

/**
 * @brief Creates the logger used with the tests' scoped output suppression.
 * @return Shared threaded logger writing debug messages to standard output.
 */
StormByte::Safe::Shared<StormByte::Logger::Log> MakeSilentLogger();

/**
 * @brief Waits for the final manual pipeline stage to stop successfully.
 * @param last Final stage of the manual pipeline.
 * @return Zero on success, one on failure or timeout.
 */
int WaitForManualPipeline(StormByte::Multimedia::Pipeline::Step& last);

/**
 * @brief Checks the properties of an output containing one audio stream.
 * @param output Generated media file to inspect.
 * @param container Expected container name.
 * @param codec Expected codec name.
 * @param channels Expected channel count.
 * @param sampleRate Expected sample rate in hertz.
 * @param language Optional expected language tag.
 * @return Zero when every property matches, one otherwise.
 */
int CheckSingleAudioOutput(const std::filesystem::path& output, std::string_view container,
	std::string_view codec, std::uint8_t channels, std::uint32_t sampleRate,
	std::string_view language = {});