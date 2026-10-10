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
			auto logger = MakeLogger();
			Transcoder job{FixturePath("audio/noise_stereo.wav"), output, logger, 2000000000LL};
			auto track = job.Audio(0);
			track.Codec(codec.value().get());
			if (codecName == "Vorbis")
				track.BitRate(192000);
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

int test_generic_muxer_writes_vorbis_in_ogg() {
	return CheckSimpleOutput("pipeline/mux-policy/audio.ogg", "Ogg", "Vorbis");
}

int test_generic_muxer_writes_vorbis_in_ogv() {
	return CheckSimpleOutput("pipeline/mux-policy/audio.ogv", "Ogg", "Vorbis");
}

int test_generic_muxer_writes_alac_in_caf() {
	return CheckSimpleOutput("pipeline/mux-policy/apple.caf", "CAF", "ALAC");
}

int test_generic_muxer_writes_eac3() {
	return CheckSimpleOutput("pipeline/mux-policy/dolby.eac3", "E-AC3", "E-AC3");
}

int test_generic_flac_rejects_aac_remux() {
	auto logger = MakeLogger();
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
	auto logger = MakeLogger();
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
		TestEntry{"test_generic_muxer_writes_vorbis_in_ogg", test_generic_muxer_writes_vorbis_in_ogg},
		TestEntry{"test_generic_muxer_writes_vorbis_in_ogv", test_generic_muxer_writes_vorbis_in_ogv},
		TestEntry{"test_generic_muxer_writes_alac_in_caf", test_generic_muxer_writes_alac_in_caf},
		TestEntry{"test_generic_muxer_writes_eac3", test_generic_muxer_writes_eac3},
		TestEntry{"test_generic_flac_rejects_aac_remux", test_generic_flac_rejects_aac_remux},
		TestEntry{"test_webm_policy_rejects_attachments", test_webm_policy_rejects_attachments},
	};
	return RunSelectedTest(argc, argv, tests);
}
