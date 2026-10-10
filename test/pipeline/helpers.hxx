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
 * @brief Creates the logger used by pipeline tests.
 * @return Shared threaded logger writing LowLevel messages to standard output.
 */
StormByte::Safe::Shared<StormByte::Logger::Log> MakeLogger();

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
