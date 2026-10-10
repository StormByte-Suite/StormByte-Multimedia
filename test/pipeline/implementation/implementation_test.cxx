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
	auto logger = MakeLogger();
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
		const auto deadline = std::chrono::steady_clock::now() + 30s;
		while (!decoder.Failed() && decoder.Status() != State::Stopped
			&& std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		TEST_REQUIRE(decoder.Failed());
		TEST_REQUIRE(decoder.Error());
		TEST_REQUIRE(TestView(decoder.Error().value()).find(expectedError) != std::string_view::npos);
		return 0;
	}
	const auto deadline = std::chrono::steady_clock::now() + 30s;
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

static int CheckImmediatePlanBinding() {
	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	const auto output = OutputPath("pipeline/immediate-plan-binding.mka");
	for (int attempt = 0; attempt < 16; ++attempt) {
		Plan plan{FixturePath("audio/noise_stereo.wav"), output, 2000000000LL};
		Config::Audio audio;
		plan.add(Track{0, std::move(audio)});
		TEST_REQUIRE(plan.Check());

		Demuxer demuxer{noLogger};
		std::move(plan) >> demuxer;
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (demuxer.Status() == State::Created && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(1ms);
		TEST_REQUIRE(demuxer.Status() != State::Created);
		TEST_REQUIRE(!demuxer.Failed());
		demuxer.Stop();
	}
	return 0;
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

int test_manual_demuxer_immediate_plan_binding() {
	return CheckImmediatePlanBinding();
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
		TestEntry{"test_manual_demuxer_immediate_plan_binding", test_manual_demuxer_immediate_plan_binding},
		TestEntry{"test_manual_rejects_missing_decoder_implementation", test_manual_rejects_missing_decoder_implementation},
		TestEntry{"test_manual_rejects_mismatched_decoder_implementation", test_manual_rejects_mismatched_decoder_implementation},
	};
	return RunSelectedTest(argc, argv, tests);
}
