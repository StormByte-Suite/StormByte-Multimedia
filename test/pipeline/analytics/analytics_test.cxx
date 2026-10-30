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

#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/vmaf.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <algorithm>
#include <cmath>
#include <string>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
static int CheckVmafRemux(std::string_view source, std::string_view destination,
	ResourceUsage resources = ResourceUsage::Full, unsigned short workers = 2) {
	const auto output = OutputPath(destination);
	const auto availableBefore = StormByte::Multimedia::Backend::Pipeline::Ceiling::AvailableRam();
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath(source)), TestLocation(output), logger, 2000000000LL};
	job.Video(0).Remux();
	job.Resources(resources);
	if (workers == 0)
		job.Filter<StormByte::Multimedia::Pipeline::Filter::Video::VMAF>(
			logger, StormByte::Safe::String{"vmaf_4k_v0.6.1"});
	else
		job.Filter<StormByte::Multimedia::Pipeline::Filter::Video::VMAF>(
			logger, StormByte::Safe::String{"vmaf_4k_v0.6.1"}, workers);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	auto reports = job.Reports();
	TEST_REQUIRE(reports.size() == 1);
	const auto report = reports[0].second;
	TEST_REQUIRE(report.Kind() == Filter::Report::Status::Ok);
	const auto& data = report.Data();
	TEST_REQUIRE(data.contains(StormByte::Safe::String{"vmaf_mean"}));
	TEST_REQUIRE(data.contains(StormByte::Safe::String{"vmaf_min"}));
	TEST_REQUIRE(data.contains(StormByte::Safe::String{"frames"}));
	const auto score = std::stod(std::string{TestView(data.at(StormByte::Safe::String{"vmaf_mean"}))});
	const auto minimum = std::stod(std::string{TestView(data.at(StormByte::Safe::String{"vmaf_min"}))});
	const auto frames = std::stoul(std::string{TestView(data.at(StormByte::Safe::String{"frames"}))});
	TEST_REQUIRE(score == 100.0);
	TEST_REQUIRE(minimum == 100.0);
	TEST_REQUIRE(frames == 48);
	const auto threads = std::stoul(std::string{TestView(data.at(StormByte::Safe::String{"threads"}))});
	const auto ceiling = std::stoul(std::string{TestView(data.at(StormByte::Safe::String{"input_ceiling"}))});
	TEST_REQUIRE(ceiling >= 2 && ceiling <= (resources == ResourceUsage::Low ? 2 : resources == ResourceUsage::Medium ? 8 : 32));
	if (resources == ResourceUsage::Low)
		TEST_REQUIRE(threads == 1);
	else if (resources == ResourceUsage::Medium) {
		using StormByte::Multimedia::Backend::Pipeline::Ceiling;
		const auto budget = Ceiling::ResourceThreads(resources, Ceiling::MaxThreads());
		TEST_REQUIRE(threads >= 1 && threads <= budget && (workers == 0 || threads <= workers));
		if (std::min(availableBefore, Ceiling::AvailableRam()) >= 12ull * 1024 * 1024 * 1024)
			TEST_REQUIRE(ceiling == 8);
	}
	else {
		TEST_REQUIRE(ceiling == 32);
#if defined(__LLVM_INSTR_PROFILE_GENERATE)
		TEST_REQUIRE(threads == 1);
#else
		TEST_REQUIRE(threads == workers);
#endif
	}
	return 0;
}
}

int test_transcoder_vmaf_remux_matches_source_at_100() {
	const int result = CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-remux.mkv");
	if (result != 0)
		return result;
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-remux-repeat.mkv");
}

int test_transcoder_vmaf_hevc_remux_matches_source_at_100() {
	return CheckVmafRemux("video/anime_like.mkv", "pipeline/vmaf-hevc-remux.mkv");
}

int test_transcoder_vmaf_reduced_resources_clamp_explicit_workers() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-low.mkv", ResourceUsage::Low, 32);
}

int test_transcoder_vmaf_low_resources_clamp_automatic_workers() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-low-automatic.mkv", ResourceUsage::Low, 0);
}

int test_transcoder_vmaf_medium_resources_clamp_explicit_workers() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-medium.mkv", ResourceUsage::Medium, 32);
}

int test_transcoder_vmaf_medium_resources_clamp_automatic_workers() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-medium-automatic.mkv", ResourceUsage::Medium, 0);
}

int test_transcoder_vmaf_full_resources_honor_explicit_workers() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-full-workers.mkv", ResourceUsage::Full, 17);
}

int test_transcoder_vmaf_kvazaar_padding() {
	const auto codec = Registry::Instance().FindCodec("H.265");
	TEST_REQUIRE(codec);
	bool available = false;
	for (const auto& encoder : codec->get().Encoders())
		available |= encoder.Name() == "libkvazaar";
	if (!available)
		return TEST_SKIP;
	const auto output = OutputPath("pipeline/vmaf-kvazaar-padding.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath("video/kvazaar_unaligned_704x396.avi")),
		TestLocation(output), logger, 2000000000LL};
	job.Resources(ResourceUsage::Low);
	job.Video(0).Codec(codec->get())
		.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libkvazaar"});
	job.Filter<Filter::Video::VMAF>(logger, StormByte::Safe::String{"vmaf_v0.6.1"}, 1u);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	const auto reports = job.Reports();
	TEST_REQUIRE(reports.size() == 1);
	const auto& report = reports[0].second;
	TEST_REQUIRE(report.Kind() == Filter::Report::Status::Ok);
	TEST_REQUIRE(std::stoul(std::string{TestView(report.Data().at(StormByte::Safe::String{"frames"}))}) == 24);
	const double score = std::stod(std::string{TestView(report.Data().at(StormByte::Safe::String{"vmaf_mean"}))});
	TEST_REQUIRE(std::isfinite(score) && score > 80.0 && score <= 100.0);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_vmaf_kvazaar_padding", test_transcoder_vmaf_kvazaar_padding},
		TestEntry{"test_transcoder_vmaf_remux_matches_source_at_100", test_transcoder_vmaf_remux_matches_source_at_100},
		TestEntry{"test_transcoder_vmaf_hevc_remux_matches_source_at_100", test_transcoder_vmaf_hevc_remux_matches_source_at_100},
		TestEntry{"test_transcoder_vmaf_reduced_resources_clamp_explicit_workers", test_transcoder_vmaf_reduced_resources_clamp_explicit_workers},
		TestEntry{"test_transcoder_vmaf_low_resources_clamp_automatic_workers", test_transcoder_vmaf_low_resources_clamp_automatic_workers},
		TestEntry{"test_transcoder_vmaf_medium_resources_clamp_explicit_workers", test_transcoder_vmaf_medium_resources_clamp_explicit_workers},
		TestEntry{"test_transcoder_vmaf_medium_resources_clamp_automatic_workers", test_transcoder_vmaf_medium_resources_clamp_automatic_workers},
		TestEntry{"test_transcoder_vmaf_full_resources_honor_explicit_workers", test_transcoder_vmaf_full_resources_honor_explicit_workers},
	};
	return RunSelectedTest(argc, argv, tests);
}
