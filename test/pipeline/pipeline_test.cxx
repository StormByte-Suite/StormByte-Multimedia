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

#include "../test_helpers.hxx"

#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/config/attachment.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/vmaf.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <array>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
	bool IsTerminal(Status status) {
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

	int WaitForTranscoderFailure(Transcoder& job, std::string_view expectedMessage = {}) {
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

	auto MakeSilentLogger() {
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

	int CheckRemuxOutput(const std::filesystem::path& output, std::size_t expectedAttachments = 1) {
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& result = opened.value();
		std::size_t videos = 0;
		std::size_t audios = 0;
		std::size_t subtitles = 0;
		bool foundJapanese = false;
		for (const auto stream : result.Streams()) {
			switch (stream.Type()) {
				case StormByte::Multimedia::Type::Video: ++videos; break;
				case StormByte::Multimedia::Type::Audio: ++audios; break;
				case StormByte::Multimedia::Type::Subtitle: {
					++subtitles;
					TEST_REQUIRE(stream.Codec().Name() == "PGS"sv);
					const auto language = stream.Metadata().Language();
					if (!language)
						std::cerr << "[DETAIL] PGS stream " << stream.Index() << " has no language tag" << std::endl;
					TEST_REQUIRE(language);
					foundJapanese = foundJapanese || TestView(language.value()) == "jpn"sv;
					break;
				}
				default: break;
			}
		}
		TEST_REQUIRE(videos == 1);
		TEST_REQUIRE(audios == 1);
		TEST_REQUIRE(subtitles == 3);
		TEST_REQUIRE(foundJapanese);
		TEST_REQUIRE(result.Attachments().size() == expectedAttachments);
		return 0;
	}

	int CheckEncodedVideoOutput(const std::filesystem::path& output, std::string_view expectedCodec) {
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& result = opened.value();
		TEST_REQUIRE(result.Streams().size() == 1);
		const auto video = result.Streams()[0];
		TEST_REQUIRE(video.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(video.Codec().Name() == expectedCodec);
		const auto properties = video.Video();
		TEST_REQUIRE(properties);
		TEST_REQUIRE(properties.value().Resolution().Width() == 1920);
		TEST_REQUIRE(properties.value().Resolution().Height() == 1080);
		return 0;
	}

	int CheckSingleAudioOutput(const std::filesystem::path& output, std::string_view container,
		std::string_view codec, std::uint8_t channels, std::uint32_t sampleRate,
		std::string_view language = {}) {
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

	bool SamePoint(const StormByte::Multimedia::Property::Point& left,
		const StormByte::Multimedia::Property::Point& right) {
		return left.X() == right.X() && left.Y() == right.Y();
	}

	int CheckHdr10EncodeOutput(const std::filesystem::path& output, std::string_view expectedCodec) {
	const auto sourcePath = FixturePath("video/hdr10_metadata_source.mkv");
	auto source = File::Open(StormByte::Safe::String{sourcePath.string()});
	TEST_REQUIRE(source);
	TEST_REQUIRE(source.value().Streams().size() == 1);
	const auto sourceVideo = source.value().Streams()[0].Video();
	TEST_REQUIRE(sourceVideo && sourceVideo.value().HDR10());
	TEST_REQUIRE(sourceVideo.value().HDR10()->Origin() == StormByte::Multimedia::Property::HDR10::Source::Metadata);

	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded);
	TEST_REQUIRE(encoded.value().Streams().size() == 1);
	const auto stream = encoded.value().Streams()[0];
	TEST_REQUIRE(stream.Codec().Name() == expectedCodec);
	const auto encodedVideo = stream.Video();
	TEST_REQUIRE(encodedVideo && encodedVideo.value().HDR10());
	TEST_REQUIRE(encodedVideo.value().Resolution().Width() == sourceVideo.value().Resolution().Width());
	TEST_REQUIRE(encodedVideo.value().Resolution().Height() == sourceVideo.value().Resolution().Height());
	TEST_REQUIRE(encodedVideo.value().Color().PixelFormat() == sourceVideo.value().Color().PixelFormat());
	TEST_REQUIRE(encodedVideo.value().Color().Space() == sourceVideo.value().Color().Space());
	TEST_REQUIRE(encodedVideo.value().Color().Primaries() == sourceVideo.value().Color().Primaries());
	TEST_REQUIRE(encodedVideo.value().Color().Transfer() == sourceVideo.value().Color().Transfer());
	const auto& before = sourceVideo.value().HDR10().value();
	const auto& after = encodedVideo.value().HDR10().value();
	TEST_REQUIRE(after.Origin() == StormByte::Multimedia::Property::HDR10::Source::Metadata);
	TEST_REQUIRE(SamePoint(after.Red(), before.Red()));
	TEST_REQUIRE(SamePoint(after.Green(), before.Green()));
	TEST_REQUIRE(SamePoint(after.Blue(), before.Blue()));
	TEST_REQUIRE(SamePoint(after.White(), before.White()));
	TEST_REQUIRE(SamePoint(after.Luminance(), before.Luminance()));
	TEST_REQUIRE(after.LightLevel().has_value() == before.LightLevel().has_value());
	if (before.LightLevel())
		TEST_REQUIRE(SamePoint(after.LightLevel().value(), before.LightLevel().value()));
	return 0;
}

	std::string ReadFileBytes(const std::filesystem::path& path) {
		std::ifstream input{path, std::ios::binary};
		return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
	}

	std::string WithoutAsciiWhitespace(std::string_view text) {
		std::string result;
		for (const unsigned char character : text)
			if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
				result.push_back(static_cast<char>(character));
		return result;
	}
}

int test_transcoder_remux_preserves_pgs_and_attachments() {
	const auto output = OutputPath("pipeline/transcoder-remux.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/anime_like.mkv"), output, logger, 2000000000LL};
	job.Video(0).Remux();
	job.Audio(1).Remux();
	job.Subtitle(2).Remux();
	job.Subtitle(3).Remux();
	job.Subtitle(4).Remux();
	job.Attachments();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckRemuxOutput(output);
}

int test_transcoder_remux_omits_unselected_attachments() {
	const auto output = OutputPath("pipeline/transcoder-remux-no-attachments.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/anime_like.mkv"), output, logger, 2000000000LL};
	job.Video(0).Remux();
	job.Audio(1).Remux();
	job.Subtitle(2).Remux();
	job.Subtitle(3).Remux();
	job.Subtitle(4).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckRemuxOutput(output, 0);
}

int test_hand_built_pipe_remux_preserves_pgs_and_attachments() {
	const auto input = FixturePath("video/anime_like.mkv");
	const auto output = OutputPath("pipeline/manual-remux.mkv");
	Plan plan{input, output, 2000000000LL};
	plan.add(Track{0, StormByte::Multimedia::Type::Video});
	plan.add(Track{1, StormByte::Multimedia::Type::Audio});
	plan.add(Track{2, Config::Subtitle{}});
	plan.add(Track{3, Config::Subtitle{}});
	plan.add(Track{4, Config::Subtitle{}});
	Config::Attachment attachments{std::string_view{"*/*"}};
	plan.add(Track{0, std::move(attachments)});
	const auto planCheck = plan.Check();
	if (!planCheck) {
		std::cerr << "[DETAIL] Manual Plan check: "
			<< planCheck.error()->what() << std::endl;
		return 1;
	}

	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Demuxer demuxer{noLogger};
	Remuxer video{noLogger, 0};
	Remuxer audio{noLogger, 1};
	Remuxer spanish{noLogger, 2};
	Remuxer english{noLogger, 3};
	Remuxer japanese{noLogger, 4};
	Muxer muxer{noLogger};
	std::move(plan) >> demuxer;
	demuxer >> muxer;
	demuxer >> video >> muxer;
	demuxer >> audio >> muxer;
	demuxer >> spanish >> muxer;
	demuxer >> english >> muxer;
	demuxer >> japanese >> muxer;
	TEST_REQUIRE(WaitForManualPipeline(muxer) == 0);
	return CheckRemuxOutput(output);
}

static int CheckVmafRemux(std::string_view source, std::string_view destination) {
	const auto output = OutputPath(destination);
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath(source), output, logger, 2000000000LL};
	job.Video(0).Remux();
	job.Filter<StormByte::Multimedia::Pipeline::Filter::Video::VMAF>(
		logger, StormByte::Safe::String{"vmaf_4k_v0.6.1"}, static_cast<unsigned short>(2));
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
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
	return 0;
}

int test_transcoder_vmaf_remux_matches_source_at_100() {
	return CheckVmafRemux("video/bluray_like_vp9.mkv", "pipeline/vmaf-remux.mkv");
}

int test_transcoder_vmaf_hevc_remux_matches_source_at_100() {
	return CheckVmafRemux("video/anime_like.mkv", "pipeline/vmaf-hevc-remux.mkv");
}

int test_transcoder_extracts_single_ac3_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-1.ac3");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/bluray_like_hdr10.mkv"), output, logger, 2000000000LL};
	job.Audio(1).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "AC-3", "AC-3", 6, 48000);
}

int test_transcoder_extracts_single_aac_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-2.m4a");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/bluray_like_hdr10.mkv"), output, logger, 2000000000LL};
	job.Audio(2).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_remuxes_single_opus_file_to_mka() {
	const auto output = OutputPath("pipeline/opus-remux.mka");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_opus_source_51.opus"), output, logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "Matroska", "Opus", 6, 48000);
}

int test_transcoder_remuxes_wav_file_to_wav() {
	const auto output = OutputPath("pipeline/wav-remux.wav");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_stereo.wav"), output, logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "WAV", "PCM S16 LE", 2, 48000);
}

int test_transcoder_hevc_encode_output_file_properties() {
	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	TEST_REQUIRE(hevc);
	if (!hevc.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-hevc.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/hdr10_metadata_source.mkv"), output, logger, 2000000000LL};
	job.Video(0).Codec(hevc.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckEncodedVideoOutput(output, "H.265");
}

int test_transcoder_hevc_encode_preserves_hdr10_metadata() {
	auto& registry = Registry::Instance();
	auto hevc = registry.FindCodec("H.265");
	TEST_REQUIRE(hevc);
	if (!hevc.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-hevc-hdr10.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/hdr10_metadata_source.mkv"), output, logger, 2000000000LL};
	job.Video(0).Codec(hevc.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckHdr10EncodeOutput(output, "H.265");
}

int test_transcoder_vp9_encode_preserves_hdr10_metadata() {
	auto& registry = Registry::Instance();
	auto vp9 = registry.FindCodec("VP9");
	TEST_REQUIRE(vp9);
	if (!vp9.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-vp9-hdr10.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/hdr10_metadata_source.mkv"), output, logger, 2000000000LL};
	job.Video(0).Codec(vp9.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckHdr10EncodeOutput(output, "VP9");
}

int test_transcoder_ocr_japanese_pgs_to_subrip() {
	auto& registry = Registry::Instance();
	auto subrip = registry.FindCodec("SubRip");
	TEST_REQUIRE(subrip);
	if (!subrip.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/ocr-japanese.mkv");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("video/anime_like.mkv"), output, logger, 2000000000LL};
	job.Subtitle(4).Codec(subrip.value().get()).Language(StormByte::Safe::String{"jpn"});
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Streams().size() == 1);
	const auto subtitle = opened.value().Streams()[0];
	TEST_REQUIRE(subtitle.Type() == StormByte::Multimedia::Type::Subtitle);
	TEST_REQUIRE(subtitle.Codec().Name() == "SRT"sv);
	const auto language = subtitle.Metadata().Language();
	TEST_REQUIRE(language);
	TEST_REQUIRE(TestView(language.value()) == "jpn"sv);
	const auto encodedBytes = ReadFileBytes(output);
	const auto normalizedBytes = WithoutAsciiWhitespace(encodedBytes);
	const bool foundSubtitleToken = normalizedBytes.find("字幕") != std::string::npos;
	const bool foundDepartureToken = normalizedBytes.find("出発") != std::string::npos;
	if (!foundSubtitleToken || !foundDepartureToken)
		std::cerr << "[DETAIL] OCR did not recognize the expected Japanese tokens: 字幕 / 出発" << std::endl;
	TEST_REQUIRE(foundSubtitleToken);
	TEST_REQUIRE(foundDepartureToken);
	return 0;
}

int test_transcoder_opus_encode_output_file_properties() {
	auto& registry = Registry::Instance();
	auto opus = registry.FindCodec("Opus");
	TEST_REQUIRE(opus);
	if (!opus.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-opus.mka");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_opus_source_51.opus"), output, logger, 2000000000LL};
	job.Audio(0).Codec(opus.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Container().Name() == "Matroska");
	TEST_REQUIRE(opened.value().Streams().size() == 1);
	const auto audioStream = opened.value().Streams()[0];
	TEST_REQUIRE(audioStream.Type() == StormByte::Multimedia::Type::Audio);
	TEST_REQUIRE(audioStream.Codec().Name() == "Opus"sv);
	auto audio = audioStream.Audio();
	TEST_REQUIRE(audio);
	TEST_REQUIRE(audio.value().Channels() == 6);
	TEST_REQUIRE(audio.value().SampleRate() == 48000);
	return 0;
}

int test_transcoder_aac_encode_from_stereo_wav() {
	auto& registry = Registry::Instance();
	auto aac = registry.FindCodec("AAC");
	TEST_REQUIRE(aac);
	if (!aac.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-aac.m4a");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_51.opus"), output, logger, 2000000000LL};
	job.Audio(0).Codec(aac.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

static int CheckAutomaticSurroundConversion(std::string_view codecName, std::string_view destination) {
	const auto input = FixturePath("audio/noise_71.wav");
	auto source = File::Open(StormByte::Safe::String{input.string()});
	TEST_REQUIRE(source);
	TEST_REQUIRE(source.value().Streams().size() == 1);
	const auto sourceStream = source.value().Streams()[0];
	TEST_REQUIRE(sourceStream.Type() == StormByte::Multimedia::Type::Audio);
	TEST_REQUIRE(sourceStream.Codec().Name() == "PCM S16 LE"sv);
	const auto sourceAudio = sourceStream.Audio();
	TEST_REQUIRE(sourceAudio);
	TEST_REQUIRE(sourceAudio.value().Channels() == 8);
	TEST_REQUIRE(sourceAudio.value().Layout() == Property::ChannelLayout::SevenPointOne);
	TEST_REQUIRE(sourceAudio.value().SampleRate() == 48000);

	auto codec = Registry::Instance().FindCodec(codecName);
	TEST_REQUIRE(codec);
	if (!codec.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath(destination);
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{input, output, logger, 2000000000LL};
	// AC-3/E-AC3 may automatically convert 7.1 to 5.1 without an explicit downmix filter.
	job.Audio(0).Codec(codec.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	TEST_REQUIRE(CheckSingleAudioOutput(output, "Matroska", codecName, 6, 48000) == 0);
	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded);
	const auto outputAudio = encoded.value().Streams()[0].Audio();
	TEST_REQUIRE(outputAudio);
	TEST_REQUIRE(outputAudio.value().Layout() == Property::ChannelLayout::FivePointOne);
	return 0;
}

int test_transcoder_allows_71_to_ac3_51_without_downmix_filter() {
	return CheckAutomaticSurroundConversion("AC-3", "pipeline/encoded-71-to-ac3-51.mka");
}

int test_transcoder_allows_71_to_eac3_51_without_downmix_filter() {
	return CheckAutomaticSurroundConversion("E-AC3", "pipeline/encoded-71-to-eac3-51.mka");
}

int test_transcoder_rejects_51_audio_to_mp3_without_downmix() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_51.opus"),
		OutputPath("pipeline/rejected-51.mp3"), logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job, "unsupported channel layout");
}

template<typename Configure>
static int CheckRejectedJob(std::string_view source, const std::filesystem::path& destination,
	std::string_view expectedMessage, Configure configure, bool rejectDuringConfigure = false) {
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath(source), destination, logger, 2000000000LL};
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	configure(job);
	if (rejectDuringConfigure)
		TEST_REQUIRE(job.Failed());
	else
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	return WaitForTranscoderFailure(job, expectedMessage);
}

int test_transcoder_rejects_negative_stream_index() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-negative-index.mkv"),
		"origin index is negative", [](Transcoder& job) { job.Audio(-1).Remux(); }, true);
}

int test_transcoder_rejects_missing_stream_index() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-missing-index.mkv"),
		"source stream 99 does not exist", [](Transcoder& job) { job.Video(99).Remux(); }, true);
}

int test_transcoder_rejects_wrong_stream_type() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-stream-type.mkv"),
		"requires", [](Transcoder& job) { job.Audio(0).Remux(); }, true);
}

int test_transcoder_rejects_wrong_codec_type() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-codec-type.mkv"),
		"codec 'MP3'", [&](Transcoder& job) { job.Video(0).Codec(mp3.value().get()); }, true);
}

int test_transcoder_rejects_duplicate_mapping() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-duplicate.mkv"),
		"origin 0 is already mapped", [](Transcoder& job) {
			job.Video(0).Remux();
			job.Video(0).Remux();
		}, true);
}

int test_transcoder_rejects_invalid_attachment_pattern() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-mime.mkv"),
		"attachment MIME pattern", [](Transcoder& job) { job.Attachments("application/**"); }, true);
}

int test_transcoder_rejects_missing_logger() {
	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Transcoder job{FixturePath("video/anime_like.mkv"),
		OutputPath("pipeline/rejected-no-logger.mkv"), noLogger, 2000000000LL};
	TEST_REQUIRE(job.Failed());
	job.Run();
	return WaitForTranscoderFailure(job, "logger is required");
}

int test_transcoder_rejects_empty_mapping() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-empty-map.mkv"),
		"plan has no tracks", [](Transcoder&) {});
}

int test_transcoder_rejects_unknown_destination_container() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-container.unknown-container"),
		"destination container is unknown", [](Transcoder& job) { job.Video(0).Remux(); });
}

int test_transcoder_rejects_video_in_wav() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-video.wav"),
		"requires exactly one audio track", [](Transcoder& job) { job.Video(0).Remux(); });
}

int test_transcoder_rejects_multiple_audio_tracks_in_wav() {
	return CheckRejectedJob("video/bluray_like_hdr10.mkv", OutputPath("pipeline/rejected-multiple-audio.wav"),
		"requires exactly one audio track", [](Transcoder& job) {
			job.Audio(1).Remux();
			job.Audio(2).Remux();
		});
}

int test_transcoder_rejects_aac_remux_in_mp3() {
	return CheckRejectedJob("audio/noise_51.m4a", OutputPath("pipeline/rejected-aac-remux.mp3"),
		"does not support codec aac", [](Transcoder& job) { job.Audio(0).Remux(); });
}

int test_transcoder_rejects_pgs_remux_in_mp4() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-pgs.mp4"),
		"does not support codec hdmv_pgs_subtitle", [](Transcoder& job) { job.Subtitle(4).Remux(); });
}

int test_transcoder_rejects_attachments_in_mp4() {
	return CheckRejectedJob("video/anime_like.mkv", OutputPath("pipeline/rejected-attachments.mp4"),
		"does not support file attachments", [](Transcoder& job) {
			job.Video(0).Remux();
			job.Attachments();
		});
}

int test_transcoder_rejects_missing_destination_parent() {
	const auto destination = OutputPath("pipeline/parent-probe.mkv").parent_path()
		/ "nonexistent-destination-parent" / "output.mkv";
	TEST_REQUIRE(!std::filesystem::exists(destination.parent_path()));
	return CheckRejectedJob("audio/noise_stereo.wav", destination,
		"muxer writer open failed", [](Transcoder& job) { job.Audio(0).Remux(); });
}

int test_transcoder_encodes_stereo_audio_to_mp3() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-stereo.mp3");
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath("audio/noise_stereo.mp3"), output, logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP3", "MP3", 2, 48000);
}

static int CheckManualRejectsInvalidSource(std::string_view source, std::string_view destination) {
	Plan plan{FixturePath(source), OutputPath(destination), 2000000000LL};
	plan.add(Track{0, StormByte::Multimedia::Type::Video});
	const auto check = plan.Check();
	TEST_REQUIRE(!check);
	TEST_REQUIRE(check.error());
	TEST_REQUIRE(std::string_view{check.error()->what()}.find("source snapshot failed") != std::string_view::npos);

	StormByte::Safe::Shared<StormByte::Logger::Log> noLogger;
	Demuxer demuxer{noLogger};
	Remuxer video{noLogger, 0};
	Muxer muxer{noLogger};
	std::move(plan) >> demuxer;
	demuxer >> muxer;
	demuxer >> video >> muxer;
	const auto deadline = std::chrono::steady_clock::now() + 20s;
	while (demuxer.Status() != State::Failed && demuxer.Status() != State::Stopped &&
		std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(10ms);
	TEST_REQUIRE(demuxer.Status() == State::Failed);
	const auto error = demuxer.Error();
	TEST_REQUIRE(error && !error->empty());
	TEST_REQUIRE(TestView(error.value()).find("source snapshot failed") != std::string_view::npos);
	TEST_REQUIRE(muxer.Failed());
	return 0;
}

static int CheckTranscoderRejectsInvalidSource(std::string_view source, std::string_view destination) {
	QuietCout quietCout;
	auto logger = MakeSilentLogger();
	Transcoder job{FixturePath(source), OutputPath(destination), logger, 2000000000LL};
	TEST_REQUIRE(job.Failed());
	const auto originalError = job.Error();
	TEST_REQUIRE(originalError && !originalError->empty());
	job.Video(0).Remux();
	job.Run();
	TEST_REQUIRE(WaitForTranscoderFailure(job) == 0);
	TEST_REQUIRE(job.Error().value() == originalError.value());
	return 0;
}

int test_hand_built_pipe_rejects_random_invalid_source() {
	return CheckManualRejectsInvalidSource("invalid/random_garbage.mkv", "pipeline/rejected-manual-garbage.mkv");
}

int test_hand_built_pipe_rejects_truncated_source() {
	return CheckManualRejectsInvalidSource("invalid/truncated_hevc.mkv", "pipeline/rejected-manual-truncated.mkv");
}

int test_transcoder_rejects_random_invalid_source() {
	return CheckTranscoderRejectsInvalidSource("invalid/random_garbage.mkv", "pipeline/rejected-transcoder-garbage.mkv");
}

int test_transcoder_rejects_truncated_source() {
	return CheckTranscoderRejectsInvalidSource("invalid/truncated_hevc.mkv", "pipeline/rejected-transcoder-truncated.mkv");
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_remux_preserves_pgs_and_attachments", test_transcoder_remux_preserves_pgs_and_attachments},
		TestEntry{"test_transcoder_remux_omits_unselected_attachments", test_transcoder_remux_omits_unselected_attachments},
		TestEntry{"test_hand_built_pipe_remux_preserves_pgs_and_attachments", test_hand_built_pipe_remux_preserves_pgs_and_attachments},
		TestEntry{"test_transcoder_vmaf_remux_matches_source_at_100", test_transcoder_vmaf_remux_matches_source_at_100},
		TestEntry{"test_transcoder_vmaf_hevc_remux_matches_source_at_100", test_transcoder_vmaf_hevc_remux_matches_source_at_100},
		TestEntry{"test_transcoder_extracts_single_ac3_track_from_bluray_mkv", test_transcoder_extracts_single_ac3_track_from_bluray_mkv},
		TestEntry{"test_transcoder_extracts_single_aac_track_from_bluray_mkv", test_transcoder_extracts_single_aac_track_from_bluray_mkv},
		TestEntry{"test_transcoder_remuxes_single_opus_file_to_mka", test_transcoder_remuxes_single_opus_file_to_mka},
		TestEntry{"test_transcoder_remuxes_wav_file_to_wav", test_transcoder_remuxes_wav_file_to_wav},
		TestEntry{"test_transcoder_ocr_japanese_pgs_to_subrip", test_transcoder_ocr_japanese_pgs_to_subrip},
		TestEntry{"test_transcoder_hevc_encode_output_file_properties", test_transcoder_hevc_encode_output_file_properties},
		TestEntry{"test_transcoder_hevc_encode_preserves_hdr10_metadata", test_transcoder_hevc_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_vp9_encode_preserves_hdr10_metadata", test_transcoder_vp9_encode_preserves_hdr10_metadata},
		TestEntry{"test_transcoder_opus_encode_output_file_properties", test_transcoder_opus_encode_output_file_properties},
		TestEntry{"test_transcoder_aac_encode_from_stereo_wav", test_transcoder_aac_encode_from_stereo_wav},
		TestEntry{"test_transcoder_rejects_51_audio_to_mp3_without_downmix", test_transcoder_rejects_51_audio_to_mp3_without_downmix},
		TestEntry{"test_transcoder_allows_71_to_ac3_51_without_downmix_filter", test_transcoder_allows_71_to_ac3_51_without_downmix_filter},
		TestEntry{"test_transcoder_allows_71_to_eac3_51_without_downmix_filter", test_transcoder_allows_71_to_eac3_51_without_downmix_filter},
		TestEntry{"test_hand_built_pipe_rejects_random_invalid_source", test_hand_built_pipe_rejects_random_invalid_source},
		TestEntry{"test_hand_built_pipe_rejects_truncated_source", test_hand_built_pipe_rejects_truncated_source},
		TestEntry{"test_transcoder_rejects_random_invalid_source", test_transcoder_rejects_random_invalid_source},
		TestEntry{"test_transcoder_rejects_truncated_source", test_transcoder_rejects_truncated_source},
		TestEntry{"test_transcoder_rejects_negative_stream_index", test_transcoder_rejects_negative_stream_index},
		TestEntry{"test_transcoder_rejects_missing_stream_index", test_transcoder_rejects_missing_stream_index},
		TestEntry{"test_transcoder_rejects_wrong_stream_type", test_transcoder_rejects_wrong_stream_type},
		TestEntry{"test_transcoder_rejects_wrong_codec_type", test_transcoder_rejects_wrong_codec_type},
		TestEntry{"test_transcoder_rejects_duplicate_mapping", test_transcoder_rejects_duplicate_mapping},
		TestEntry{"test_transcoder_rejects_invalid_attachment_pattern", test_transcoder_rejects_invalid_attachment_pattern},
		TestEntry{"test_transcoder_rejects_missing_logger", test_transcoder_rejects_missing_logger},
		TestEntry{"test_transcoder_rejects_empty_mapping", test_transcoder_rejects_empty_mapping},
		TestEntry{"test_transcoder_rejects_unknown_destination_container", test_transcoder_rejects_unknown_destination_container},
		TestEntry{"test_transcoder_rejects_video_in_wav", test_transcoder_rejects_video_in_wav},
		TestEntry{"test_transcoder_rejects_multiple_audio_tracks_in_wav", test_transcoder_rejects_multiple_audio_tracks_in_wav},
		TestEntry{"test_transcoder_rejects_aac_remux_in_mp3", test_transcoder_rejects_aac_remux_in_mp3},
		TestEntry{"test_transcoder_rejects_pgs_remux_in_mp4", test_transcoder_rejects_pgs_remux_in_mp4},
		TestEntry{"test_transcoder_rejects_attachments_in_mp4", test_transcoder_rejects_attachments_in_mp4},
		TestEntry{"test_transcoder_rejects_missing_destination_parent", test_transcoder_rejects_missing_destination_parent},
		TestEntry{"test_transcoder_encodes_stereo_audio_to_mp3", test_transcoder_encodes_stereo_audio_to_mp3},
	};
	return RunSelectedTest(argc, argv, tests);
}
