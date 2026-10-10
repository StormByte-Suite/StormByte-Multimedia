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
#include <StormByte/multimedia/pipeline/filters/video/scale.hxx>
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <cstddef>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	constexpr unsigned char LogoBytes[] = {
#embed "../../files/logo/watermark-noise-logo.png"
	};

	int CheckWatermarkOutput(bool binaryInput, const std::filesystem::path& output) {
		auto codec = Registry::Instance().FindCodec("VP9");
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		{
			auto logger = MakeLogger();
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(FixturePath("video/bluray_like_hdr10.mp4")), TestLocation(output), logger, 2000000000LL};
			auto track = job.Video(0);
			track.Codec(codec.value().get())
				.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"})
				.BitRate(1500000);
			StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
			options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
			options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
			track.FineTune(std::move(options));
			track.Filter<Filter::Video::Scale>(logger, 640u, 360u);
			if (binaryInput) {
				StormByte::Safe::Binary logo;
				const auto* bytes = reinterpret_cast<const std::byte*>(LogoBytes);
				logo.assign(bytes, bytes + sizeof(LogoBytes));
				track.Filter<Filter::Video::Watermark>(logger, logo, Filter::Video::Anchor::TopRight, 30u, 16);
			}
			else {
				const auto logo = FixturePath("logo/watermark-noise-logo.png");
				track.Filter<Filter::Video::Watermark>(logger, StormByte::Safe::String{logo.string()},
					Filter::Video::Anchor::TopRight, 30u, 16);
			}
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("starting transcoder run");
			job.Run();
			const int result = WaitForOptionalCodec(job);
			if (result != 0)
				return result;
		}
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == "WebM");
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(stream.Codec().Name() == "VP9");
		const auto video = stream.Video();
		TEST_REQUIRE(video);
		TEST_REQUIRE(video.value().Resolution().Width() == 640);
		TEST_REQUIRE(video.value().Resolution().Height() == 360);
		const auto& duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		std::cout << "[WATERMARK " << (binaryInput ? "binary" : "path") << "] " << output.string() << std::endl;
		return 0;
	}
}

int test_watermark_path_and_embedded_binary() {
	const int pathResult = CheckWatermarkOutput(false, OutputPath("pipeline/watermark/watermark-logo-path.webm"));
	if (pathResult != 0)
		return pathResult;
	return CheckWatermarkOutput(true, OutputPath("pipeline/watermark/watermark-logo-binary.webm"));
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_watermark_path_and_embedded_binary", test_watermark_path_and_embedded_binary},
	};
	return RunSelectedTest(argc, argv, tests);
}
