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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

extern "C" {
	#include <libavutil/pixfmt.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	using VideoFrame = StormByte::Multimedia::FFmpeg::AVFrame;
	constexpr float HdrWhite = 203.f / 10000.f;
	constexpr int PictureSize = 128;
	constexpr int Origin = 16;

	constexpr unsigned char LogoBytes[] = {
#embed "../../files/logo/watermark-noise-logo.png"
	};
	constexpr unsigned char AlphaLogoBytes[] = {
#embed "../../files/logo/watermark-alpha.png"
	};
	constexpr unsigned char SvgLogoBytes[] = {
#embed "../../files/logo/watermark-vector.svg"
	};

	struct CompositeResult {
		VideoFrame Before;
		VideoFrame After;
		std::vector<float> Linear;
		bool Captured = false;
		bool RepeatedMatches = true;
		unsigned Frames = 0;
	};

	class SeedPicture final: public Filter::Process {
		public:
			SeedPicture(int transfer, int format, CompositeResult& result):
				Filter::Process({}, "watermark-seed"), m_transfer(transfer), m_format(format), m_result(result) {}
			StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
			void Clean() noexcept override { m_seeded = false; }
			void Setup() noexcept override {}
			void Process(const Frame&) noexcept override {
				VideoFrame linear;
				if (!linear.AllocVideo(PictureSize, PictureSize, AV_PIX_FMT_GBRPF32LE)) {
					Fail("failed to allocate watermark test picture");
					return;
				}
				const bool hdr = m_transfer == AVCOL_TRC_SMPTE2084 || m_transfer == AVCOL_TRC_ARIB_STD_B67;
				for (int plane = 0; plane != 3; ++plane)
					for (int row = 0; row != PictureSize; ++row) {
						auto* values = reinterpret_cast<float*>(linear.Data(plane) + row * linear.Linesize(plane));
						for (int col = 0; col != PictureSize; ++col)
							values[col] = (0.01f + 0.6f * static_cast<float>(col) / PictureSize) * (hdr ? HdrWhite : 1.f);
					}
				linear.ColorTransfer(AVCOL_TRC_LINEAR);
				linear.ColorPrimaries(AVCOL_PRI_BT709);
				linear.ColorSpace(AVCOL_SPC_RGB);
				linear.ColorRange(AVCOL_RANGE_JPEG);
				VideoFrame encoded;
				encoded.Format(m_format);
				VideoFrame::ColorTarget color;
				color.transfer = m_transfer;
				color.primaries = hdr ? AVCOL_PRI_BT2020 : AVCOL_PRI_BT709;
				color.matrix = m_format == AV_PIX_FMT_GBRPF32LE ? AVCOL_SPC_RGB : AVCOL_SPC_BT709;
				color.range = m_format == AV_PIX_FMT_GBRPF32LE ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
				color.peak = hdr ? 10000 : 100;
				if (!linear.ScaleTo(ScalingContext(), encoded, PictureSize, PictureSize, VideoFrame::Resample::Point,
					VideoFrame::Scaler::Zimg, color) || !encoded.CopyProps(AVFrame())) {
					Fail("failed to encode watermark test picture");
					return;
				}
				encoded.ColorTransfer(color.transfer);
				encoded.ColorPrimaries(color.primaries);
				encoded.ColorSpace(color.matrix);
				encoded.ColorRange(color.range);
				if (!m_seeded) {
					if (!m_result.Before.AllocVideo(PictureSize, PictureSize, m_format) || !m_result.Before.Copy(encoded)) {
						Fail("failed to retain watermark test reference");
						return;
					}
					m_seeded = true;
				}
				Save(std::move(encoded));
			}
		private:
			int m_transfer;
			int m_format;
			CompositeResult& m_result;
			bool m_seeded = false;
	};

	class CapturePicture final: public Filter::Process {
		public:
			explicit CapturePicture(CompositeResult& result):
				Filter::Process({}, "watermark-capture"), m_result(result) {}
			StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
			void Clean() noexcept override {}
			void Setup() noexcept override {}
			void Process(const Frame&) noexcept override {
				++m_result.Frames;
				if (m_result.Captured) {
					if (AVFrame().Format() == AV_PIX_FMT_GBRPF32LE)
						for (int plane = 0; plane != 3; ++plane)
							for (int row = 0; row != PictureSize; ++row)
								m_result.RepeatedMatches &= std::memcmp(
									AVFrame().Data(plane) + row * AVFrame().Linesize(plane),
									m_result.After.Data(plane) + row * m_result.After.Linesize(plane),
									PictureSize * sizeof(float)) == 0;
					EncodePicture();
					return;
				}
				const auto& source = AVFrame();
				if (!m_result.After.AllocVideo(PictureSize, PictureSize, source.Format()) || !m_result.After.Copy(source)
					|| !m_result.After.CopyProps(source)) {
					Fail("failed to retain watermark test output");
					return;
				}
				const bool hdr = source.ColorTransfer() == AVCOL_TRC_SMPTE2084
					|| source.ColorTransfer() == AVCOL_TRC_ARIB_STD_B67;
				VideoFrame linear;
				linear.Format(AV_PIX_FMT_GBRPF32LE);
				VideoFrame::ColorTarget color;
				color.transfer = AVCOL_TRC_LINEAR;
				color.primaries = AVCOL_PRI_BT709;
				color.matrix = AVCOL_SPC_RGB;
				color.range = AVCOL_RANGE_JPEG;
				color.peak = hdr ? 10000 : 100;
				if (!source.ScaleTo(ScalingContext(), linear, PictureSize, PictureSize, VideoFrame::Resample::Point,
					VideoFrame::Scaler::Zimg, color)) {
					Fail("failed to linearize watermark test output");
					return;
				}
				for (int plane = 0; plane != 3; ++plane)
					for (int row = 0; row != PictureSize; ++row) {
						const auto* values = reinterpret_cast<const float*>(linear.Data(plane) + row * linear.Linesize(plane));
						for (int col = 0; col != PictureSize; ++col)
							m_result.Linear.push_back(values[col] / (hdr ? HdrWhite : 1.f));
					}
				m_result.Captured = true;
				EncodePicture();
			}
		private:
			void EncodePicture() noexcept {
				const auto& source = AVFrame();
				VideoFrame encoded;
				encoded.Format(AV_PIX_FMT_YUV420P10LE);
				VideoFrame::ColorTarget color;
				color.transfer = source.ColorTransfer();
				color.primaries = source.ColorPrimaries();
				color.matrix = AVCOL_SPC_BT709;
				color.range = AVCOL_RANGE_MPEG;
				if (!source.ScaleTo(ScalingContext(), encoded, PictureSize, PictureSize, VideoFrame::Resample::Point,
					VideoFrame::Scaler::Zimg, color) || !encoded.CopyProps(source)) {
					Fail("failed to prepare captured picture for encoder");
					return;
				}
				encoded.ColorSpace(color.matrix);
				encoded.ColorRange(color.range);
				Save(std::move(encoded));
			}
			CompositeResult& m_result;
	};

	int RenderComposite(int transfer, int format, unsigned opacity, CompositeResult& result,
		const StormByte::Safe::Binary* svg = nullptr,
		StormByte::Safe::Optional<Property::Resolution> designed = {}, bool fileInput = false) {
		auto codec = Registry::Instance().FindCodec("VP9");
		TEST_REQUIRE(codec && codec->get().HasAccess(Access{Operation::Write}));
		auto logger = MakeLogger();
		Transcoder job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")),
			TestLocation(OutputPath("pipeline/watermark/opacity.webm")), logger, 2000000000LL};
		auto track = job.Video(0);
		track.Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"});
		StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
		options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
		options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
		track.FineTune(std::move(options));
		track.Filter<SeedPicture>(transfer, format, result);
		if (svg && !fileInput)
			track.Filter<Filter::Video::Watermark>(logger, *svg, Property::Point{Origin, Origin}, opacity, designed);
		else
			track.Filter<Filter::Video::Watermark>(logger,
				StormByte::Safe::String{FixturePath(svg ? "logo/watermark-vector.svg" : "logo/watermark-alpha.png").string()},
				Property::Point{Origin, Origin}, opacity, designed);
		track.Filter<CapturePicture>(result);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
		TEST_REQUIRE(result.Captured);
		TEST_REQUIRE(result.After.ColorTransfer() == transfer);
		TEST_REQUIRE(result.After.Format() == format);
		return 0;
	}

	StormByte::Safe::Binary SvgBytes(std::string_view text) {
		StormByte::Safe::Binary bytes;
		const auto* begin = reinterpret_cast<const std::byte*>(text.data());
		bytes.assign(begin, begin + text.size());
		return bytes;
	}

	int CheckSamePicture(const CompositeResult& a, const CompositeResult& b) {
		TEST_REQUIRE(a.Linear == b.Linear);
		TEST_REQUIRE(a.RepeatedMatches && b.RepeatedMatches);
		TEST_REQUIRE(a.Frames > 1 && b.Frames > 1);
		return 0;
	}

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

int test_watermark_sdr_hdr_opacity_matches() {
	for (const unsigned opacity : {0u, 45u, 50u, 70u, 100u}) {
		CompositeResult sdr;
		TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, opacity, sdr) == 0);
		for (const int transfer : {AVCOL_TRC_SMPTE2084, AVCOL_TRC_ARIB_STD_B67}) {
			CompositeResult hdr;
			TEST_REQUIRE(RenderComposite(transfer, AV_PIX_FMT_GBRPF32LE, opacity, hdr) == 0);
			TEST_REQUIRE(sdr.Linear.size() == hdr.Linear.size());
			float worst = 0;
			bool finite = true;
			for (std::size_t index = 0; index != sdr.Linear.size(); ++index) {
				finite &= std::isfinite(sdr.Linear[index]) && std::isfinite(hdr.Linear[index]);
				worst = std::max(worst, std::abs(sdr.Linear[index] - hdr.Linear[index]));
			}
			std::cout << "[OPACITY] " << opacity << " transfer=" << transfer << " max error=" << worst << '\n';
			TEST_REQUIRE(worst < 0.002f);
			TEST_REQUIRE(finite);
		}
	}
	return 0;
}

int test_watermark_preserves_uncovered_samples() {
	auto decoded = VideoFrame::DecodeImage(AlphaLogoBytes, sizeof(AlphaLogoBytes), ".png");
	VideoFrame::ScaleContext scaling;
	VideoFrame rgba;
	rgba.Format(AV_PIX_FMT_RGBA);
	TEST_REQUIRE(decoded.ScaleTo(scaling, rgba, 96, 96, VideoFrame::Resample::Point, VideoFrame::Scaler::Sws));
	for (const int transfer : {AVCOL_TRC_BT709, AVCOL_TRC_SMPTE2084, AVCOL_TRC_ARIB_STD_B67}) {
		for (const int format : {AV_PIX_FMT_GBRPF32LE, AV_PIX_FMT_YUV420P10LE}) {
			CompositeResult result;
			TEST_REQUIRE(RenderComposite(transfer, format, 45u, result) == 0);
			std::size_t untouched = 0;
			std::size_t transparent = 0;
			std::size_t changed = 0;
			bool identical = true;
			for (int plane = 0; plane != 3; ++plane) {
				const int shift = format == AV_PIX_FMT_YUV420P10LE && plane != 0 ? 1 : 0;
				const int bytes = format == AV_PIX_FMT_GBRPF32LE ? 4 : 2;
				for (int row = 0; row != (PictureSize >> shift); ++row)
					for (int col = 0; col != (PictureSize >> shift); ++col) {
						bool covered = false;
						for (int dy = 0; dy != (1 << shift); ++dy)
							for (int dx = 0; dx != (1 << shift); ++dx) {
								const int x = (col << shift) + dx - Origin;
								const int y = (row << shift) + dy - Origin;
								if (x >= 0 && x < 96 && y >= 0 && y < 96)
									covered |= rgba.Data(0)[y * rgba.Linesize(0) + x * 4 + 3] != 0;
							}
						const auto* before = result.Before.Data(plane) + row * result.Before.Linesize(plane) + col * bytes;
						const auto* after = result.After.Data(plane) + row * result.After.Linesize(plane) + col * bytes;
						if (covered) {
							changed += std::memcmp(before, after, bytes) != 0;
							continue;
						}
						identical &= std::memcmp(before, after, bytes) == 0;
						++untouched;
						const int x = col << shift;
						const int y = row << shift;
						if (x >= Origin && x < Origin + 96 && y >= Origin && y < Origin + 96)
							++transparent;
					}
			}
			TEST_REQUIRE(untouched > 0);
			TEST_REQUIRE(transparent > 0);
			TEST_REQUIRE(changed > 0);
			TEST_REQUIRE(identical);
		}
	}
	return 0;
}

int test_watermark_svg_path_buffer_and_scaling() {
	const std::string text(reinterpret_cast<const char*>(SvgLogoBytes), sizeof(SvgLogoBytes));
	const auto svg = SvgBytes(text);
	for (const auto design : {Property::Resolution{16, 16}, Property::Resolution{16, 0},
		Property::Resolution{0, 16}, Property::Resolution{16, 32}}) {
		const int edge = design.Height() == 32 ? 32 : 64;
		auto native = text;
		native.replace(native.find("width=\"8\" height=\"8\""), 20,
			"width=\"" + std::to_string(edge) + "\" height=\"" + std::to_string(edge) + "\"");
		const auto expected = SvgBytes(native);
		CompositeResult buffer, path, direct;
		TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 70u, buffer, &svg, design) == 0);
		TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 70u, path, &svg, design, true) == 0);
		TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 70u, direct, &expected) == 0);
		TEST_REQUIRE(CheckSamePicture(buffer, path) == 0);
		TEST_REQUIRE(CheckSamePicture(buffer, direct) == 0);
		bool changed = false;
		bool untouched = true;
		for (int plane = 0; plane != 3; ++plane)
			for (int y = 0; y != PictureSize; ++y)
				for (int x = 0; x != PictureSize; ++x) {
					const auto* before = buffer.Before.Data(plane) + y * buffer.Before.Linesize(plane) + x * 4;
					const auto* after = buffer.After.Data(plane) + y * buffer.After.Linesize(plane) + x * 4;
					if (x < Origin || y < Origin || x >= Origin + edge || y >= Origin + edge)
						untouched &= std::memcmp(before, after, 4) == 0;
					else
						changed |= std::memcmp(before, after, 4) != 0;
				}
		TEST_REQUIRE(changed);
		TEST_REQUIRE(untouched);
	}
	auto viewBoxOnly = text;
	viewBoxOnly.erase(viewBoxOnly.find("width=\"8\" height=\"8\""), 21);
	const auto viewBox = SvgBytes("\xef\xbb\xbf" + viewBoxOnly);
	CompositeResult native, inferred;
	TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 70u, native, &svg) == 0);
	TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 70u, inferred, &viewBox) == 0);
	TEST_REQUIRE(CheckSamePicture(native, inferred) == 0);
	return 0;
}

int test_watermark_svg_alpha_and_hdr() {
	const auto svg = SvgBytes("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\">"
		"<rect width=\"8\" height=\"8\" fill=\"#00ff00\" fill-opacity=\".5\"/></svg>");
	CompositeResult sdr;
	TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 100u, sdr, &svg) == 0);
	const int x = Origin + 4, y = Origin + 4;
	const auto* before = reinterpret_cast<const float*>(sdr.Before.Data(0) + y * sdr.Before.Linesize(0));
	const auto* after = reinterpret_cast<const float*>(sdr.After.Data(0) + y * sdr.After.Linesize(0));
	TEST_REQUIRE(after[x] > before[x] + 0.1f);
	TEST_REQUIRE(after[x] < 0.99f);
	const auto solid = SvgBytes("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\">"
		"<rect width=\"8\" height=\"8\" fill=\"#00ff00\"/></svg>");
	CompositeResult weighted;
	TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 50u, weighted, &solid) == 0);
	float alphaError = 0;
	for (std::size_t i = 0; i != sdr.Linear.size(); ++i)
		alphaError = std::max(alphaError, std::abs(sdr.Linear[i] - weighted.Linear[i]));
	TEST_REQUIRE(alphaError < 0.004f);
	for (const int transfer : {AVCOL_TRC_SMPTE2084, AVCOL_TRC_ARIB_STD_B67}) {
		CompositeResult hdr;
		TEST_REQUIRE(RenderComposite(transfer, AV_PIX_FMT_GBRPF32LE, 100u, hdr, &svg) == 0);
		float worst = 0;
		for (std::size_t i = 0; i != sdr.Linear.size(); ++i)
			worst = std::max(worst, std::abs(sdr.Linear[i] - hdr.Linear[i]));
		TEST_REQUIRE(worst < 0.002f);
	}
	return 0;
}

int test_watermark_svg_invalid_passthrough() {
	for (const auto* text : {
		"<svg",
		"<svg xmlns=\"http://www.w3.org/2000/svg\"><rect width=\"8\" height=\"8\"/></svg>",
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"0\" height=\"8\" viewBox=\"0 0 8 8\"/>",
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"-8\" height=\"8\"/>",
		"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 0 8\"/>",
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"999999\" height=\"999999\"/>",
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\"><image width=\"8\" height=\"8\" href=\"test/files/logo/watermark-alpha.png\"/></svg>"}) {
		const auto svg = SvgBytes(text);
		CompositeResult result;
		TEST_REQUIRE(RenderComposite(AVCOL_TRC_BT709, AV_PIX_FMT_GBRPF32LE, 100u, result, &svg) == 0);
		bool identical = true;
		for (int plane = 0; plane != 3; ++plane)
			for (int row = 0; row != PictureSize; ++row)
				identical &= std::memcmp(result.Before.Data(plane) + row * result.Before.Linesize(plane),
					result.After.Data(plane) + row * result.After.Linesize(plane), PictureSize * sizeof(float)) == 0;
		TEST_REQUIRE(identical);
	}
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_watermark_svg_path_buffer_and_scaling", test_watermark_svg_path_buffer_and_scaling},
		TestEntry{"test_watermark_svg_alpha_and_hdr", test_watermark_svg_alpha_and_hdr},
		TestEntry{"test_watermark_svg_invalid_passthrough", test_watermark_svg_invalid_passthrough},
		TestEntry{"test_watermark_sdr_hdr_opacity_matches", test_watermark_sdr_hdr_opacity_matches},
		TestEntry{"test_watermark_preserves_uncovered_samples", test_watermark_preserves_uncovered_samples},
		TestEntry{"test_watermark_path_and_embedded_binary", test_watermark_path_and_embedded_binary},
	};
	return RunSelectedTest(argc, argv, tests);
}
