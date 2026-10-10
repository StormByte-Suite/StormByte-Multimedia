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

#include "helpers.hxx"

#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
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
		const auto deadline = std::chrono::steady_clock::now() + 30s;
		while (!IsTerminal(job.Status()) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		if (!IsTerminal(job.Status())) {
			std::cerr << "[DETAIL] Transcoder timeout: "
				<< static_cast<StormByte::Safe::String>(*job.Telemetry()) << std::endl;
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

	int WaitForOptionalCodec(Transcoder& job) {
		const int result = WaitForTranscoder(job);
		if (result == 0)
			return 0;
		const auto error = job.Error();
		if (job.Status() == Status::Error && error) {
			const auto message = TestView(error.value());
			if (message.find("encoder implementation is unavailable") != std::string_view::npos
				|| message.find("decoder implementation is unavailable") != std::string_view::npos) {
				std::cerr << "[SKIP] " << error.value() << std::endl;
				return TEST_SKIP;
			}
		}
		return result;
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
		const auto deadline = std::chrono::steady_clock::now() + 30s;
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
		const auto deadline = std::chrono::steady_clock::now() + 30s;
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
