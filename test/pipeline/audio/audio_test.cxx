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
#include <StormByte/multimedia/pipeline/filters/audio/downmix.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/loudnorm.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <vector>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
int CheckRejectedAviPacket(std::string_view chunk, unsigned rejectedIndex, bool loudnorm) {
	const auto opus = Registry::Instance().FindCodec("Opus");
	const auto h264 = Registry::Instance().FindCodec("H.264");
	TEST_REQUIRE(opus && h264);
	if (!opus->get().HasAccess(Access{Operation::Write})
		|| !h264->get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	const auto input = OutputPath("pipeline/rejected-packet.avi");
	const auto output = OutputPath("pipeline/recovered-packet.mkv");
	{
		std::ifstream source(FixturePath("video/xvid_mp3_stereo.avi"), std::ios::binary);
		TEST_REQUIRE(source);
		std::vector<char> bytes{std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{}};
		const auto marker = std::string_view{bytes.data(), bytes.size()}.find("movi");
		TEST_REQUIRE(marker != std::string_view::npos);
		bool replaced = false;
		unsigned index = 0;
		for (std::size_t offset = marker + 4; offset + 8 <= bytes.size();) {
			std::uint32_t size = 0;
			for (unsigned byte = 0; byte < 4; ++byte)
				size |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 4 + byte])) << (8 * byte);
			TEST_REQUIRE(size <= bytes.size() - offset - 8);
			if (std::string_view{bytes.data() + offset, 4} == chunk && index++ == rejectedIndex) {
				TEST_REQUIRE(size > 19);
				const auto first = bytes.begin() + offset + 8;
				if (chunk == "01wb") {
					// Keep an MP3 sync word after a partial-frame prefix, as in cut legacy AVIs.
					std::move_backward(first, first + size - 19, first + size);
					std::fill_n(first, 19, '\x11');
				}
				else
					std::fill_n(first, size, '\0');
				replaced = true;
				break;
			}
			offset += 8 + size + (size & 1);
		}
		TEST_REQUIRE(replaced);
		std::ofstream destination(input, std::ios::binary);
		destination.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		TEST_REQUIRE(destination.good());
	}
	auto logger = MakeLogger();
	Transcoder job{TestLocation(input), TestLocation(output), logger};
	job.Video(0).Codec(h264->get());
	auto audio = job.Audio(1);
	audio.Codec(opus->get());
	if (loudnorm)
		audio.Filter<Filter::Audio::Loudnorm>(logger);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	const auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened && opened->Streams().size() == 2);
	TEST_REQUIRE(opened->Streams()[0].Codec().Name() == "H.264"sv);
	const auto stream = opened->Streams()[1];
	TEST_REQUIRE(stream.Codec().Name() == "Opus"sv);
	TEST_REQUIRE(stream.Audio() && stream.Audio()->Channels() == 2);
	TEST_REQUIRE(opened->Duration() && opened->Duration()->Nanoseconds().count() >= 1900000000LL);
	unsigned decodedTracks = 0;
	for (const auto& stage : job.Telemetry()->Stages()) {
		TEST_REQUIRE(!stage.Metrics->Error());
		if (TestView(stage.Name) == "Decoder(t=1)"sv) {
			TEST_REQUIRE(stage.Metrics->OutputFrames() >= 70);
			++decodedTracks;
		}
		if (TestView(stage.Name) == "Decoder(t=0)"sv) {
			TEST_REQUIRE(stage.Metrics->OutputFrames() >= 24);
			++decodedTracks;
		}
	}
	TEST_REQUIRE(decodedTracks == 2);
	return 0;
}

class AudioPacketCounter final: public Filter::Packet {
	public:
		AudioPacketCounter(StormByte::Safe::Shared<StormByte::Logger::Log> log,
			std::atomic<std::uint64_t>* packets) noexcept
		: Filter::Packet(std::move(log), "audio-packet-counter"), m_packets(packets) {}

		enum StormByte::Multimedia::Type Media() const noexcept override {
			return StormByte::Multimedia::Type::Audio;
		}
		void Clean() noexcept override {}
		void Setup() noexcept override {}
		void Process(const Pipeline::Packet&) noexcept override {
			m_packets->fetch_add(1);
		}

	private:
		std::atomic<std::uint64_t>* m_packets;
};

class SettledTranscoder final: public Transcoder {
	public:
		using Transcoder::Transcoder;
		std::vector<TrackSettled> Settled;

	protected:
		void OnSettled(const TrackSettled& row) noexcept override {
			Settled.push_back(row);
		}
};

int CheckRepeatedAudioMapping(bool encodeFirst, bool multipleEncodes, bool twoPass = false,
	bool embedded = false, bool measureBoth = false) {
	const auto eac3 = Registry::Instance().FindCodec("E-AC3");
	const auto ac3 = Registry::Instance().FindCodec("AC-3");
	TEST_REQUIRE(eac3 && ac3);
	if (!eac3->get().HasAccess(Access{Operation::Write})
		|| (multipleEncodes && !ac3->get().HasAccess(Access{Operation::Write})))
		return TEST_SKIP;
	const int origin = embedded ? 1 : 0;
	const auto input = FixturePath(embedded ? "video/bluray_like_hdr10.mkv" : "audio/noise_51.opus");
	const auto output = OutputPath(embedded ? "pipeline/duplicate-audio-embedded.mka"
		: twoPass ? "pipeline/duplicate-audio-two-pass.mka"
		: encodeFirst ? "pipeline/duplicate-audio-encode-first.mka"
		: multipleEncodes ? "pipeline/duplicate-audio-multi-encode.mka"
		: "pipeline/duplicate-audio-remux-first.mka");
	auto logger = MakeLogger();
	std::atomic<std::uint64_t> packets{0};
	SettledTranscoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
	const auto remux = [&] {
		job.Audio(origin).Remux().Title(StormByte::Safe::String{"original"})
			.Language(StormByte::Safe::String{"eng"})
			.Filter<AudioPacketCounter>(logger, &packets);
	};
	if (!encodeFirst)
		remux();
	auto compatibility = job.Audio(origin);
	compatibility.Codec(eac3->get()).BitRate(384000).Title(StormByte::Safe::String{"compatibility"})
		.Language(StormByte::Safe::String{"spa"});
	if (twoPass)
		compatibility.Filter<Filter::Audio::Loudnorm>(logger);
	if (encodeFirst)
		remux();
	if (multipleEncodes) {
		auto second = job.Audio(origin);
		second.Codec(ac3->get()).BitRate(448000).Title(StormByte::Safe::String{"second encode"})
			.Language(StormByte::Safe::String{"fra"})
			.Filter<Filter::Audio::Downmix>(logger, Property::ChannelLayout::Stereo);
		if (measureBoth)
			second.Filter<Filter::Audio::Loudnorm>(logger);
	}
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	TEST_REQUIRE(embedded ? packets.load() > 0 : packets.load() == 101);
	const std::size_t encodedCount = multipleEncodes ? 2 : 1;
	TEST_REQUIRE(job.Settled.size() == encodedCount);
	for (const auto& row : job.Settled) {
		TEST_REQUIRE(row.In == origin);
		TEST_REQUIRE(row.Destination);
		TEST_REQUIRE(row.Out == (row.Destination->Name() == "E-AC3" ? (encodeFirst ? 0 : 1) : 2));
		TEST_REQUIRE(row.BitRate && row.BitRate.value() == (row.Out == 2 ? 448000 : 384000));
	}
	const auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened->Streams().size() == encodedCount + 1);
	for (std::size_t index = 0; index < opened->Streams().size(); ++index) {
		const auto stream = opened->Streams()[index];
		const bool remux = index == (encodeFirst ? 1u : 0u);
		TEST_REQUIRE(stream.Codec().Name() == (remux ? (embedded ? "AC-3"sv : "Opus"sv)
			: index == 2 ? "AC-3"sv : "E-AC3"sv));
		const auto metadata = stream.Metadata();
		TEST_REQUIRE(metadata.Title());
		TEST_REQUIRE(TestView(metadata.Title().value()) == (remux ? "original"sv
			: index == 2 ? "second encode"sv : "compatibility"sv));
		TEST_REQUIRE(metadata.Language());
		TEST_REQUIRE(TestView(metadata.Language().value()) == (remux ? "eng"sv : index == 2 ? "fra"sv : "spa"sv));
		const auto audio = stream.Audio();
		TEST_REQUIRE(audio && audio->Channels() == (index == 2 ? 2 : 6) && audio->SampleRate() == 48000);

		const auto extracted = OutputPath("pipeline/duplicate-audio-decoded-" + std::to_string(encodeFirst)
			+ "-" + std::to_string(multipleEncodes) + "-" + std::to_string(index) + ".mka");
		const auto flac = Registry::Instance().FindCodec("FLAC");
		TEST_REQUIRE(flac && flac->get().HasAccess(Access{Operation::Write}));
		Transcoder decode{TestLocation(output), TestLocation(extracted), logger};
		decode.Audio(static_cast<int>(index)).Codec(flac->get());
		TEST_REQUIRE(CheckTranscoderConfigured(decode) == 0);
		decode.Run();
		TEST_REQUIRE(WaitForTranscoder(decode) == 0);
		const auto decoded = File::Open(StormByte::Safe::String{extracted.string()});
		TEST_REQUIRE(decoded && decoded->Duration());
		const auto ns = decoded->Duration()->Nanoseconds().count();
		TEST_REQUIRE(ns >= 1900000000LL && ns <= 2100000000LL);
	}
	return 0;
}

static int CheckAutomaticSurroundConversion(std::string_view codecName, std::string_view destination) {
	const auto input = FixturePath("audio/noise_71.wav");
	TEST_PHASE("opening media file for inspection");
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
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
	// AC-3/E-AC3 may automatically convert 7.1 to 5.1 without an explicit downmix filter.
	job.Audio(0).Codec(codec.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	TEST_REQUIRE(CheckSingleAudioOutput(output, "Matroska", codecName, 6, 48000) == 0);
	TEST_PHASE("opening media file for inspection");
	auto encoded = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(encoded);
	const auto outputAudio = encoded.value().Streams()[0].Audio();
	TEST_REQUIRE(outputAudio);
	TEST_REQUIRE(outputAudio.value().Layout() == Property::ChannelLayout::FivePointOne);
	return 0;
}
}

int test_transcoder_extracts_single_ac3_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-1.ac3");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/bluray_like_hdr10.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Audio(1).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "AC-3", "AC-3", 6, 48000);
}

int test_transcoder_extracts_single_aac_track_from_bluray_mkv() {
	const auto output = OutputPath("pipeline/bluray-track-2.m4a");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("video/bluray_like_hdr10.mkv")), TestLocation(output), logger, 2000000000LL};
	job.Audio(2).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_remuxes_single_opus_file_to_mka() {
	const auto output = OutputPath("pipeline/opus-remux.mka");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_opus_source_51.opus")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "Matroska", "Opus", 6, 48000);
}

int test_transcoder_remuxes_wav_file_to_wav() {
	const auto output = OutputPath("pipeline/wav-remux.wav");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_stereo.wav")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "WAV", "PCM S16 LE", 2, 48000);
}

int test_transcoder_opus_encode_output_file_properties() {
	auto& registry = Registry::Instance();
	auto opus = registry.FindCodec("Opus");
	TEST_REQUIRE(opus);
	if (!opus.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-opus.mka");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_opus_source_51.opus")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Codec(opus.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);

	TEST_PHASE("opening media file for inspection");
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
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_51.opus")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Codec(aac.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP4", "AAC", 6, 48000);
}

int test_transcoder_rejects_51_audio_to_mp3_without_downmix() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_51.opus")),
			TestLocation(OutputPath("pipeline/rejected-51.mp3")), logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	return WaitForTranscoderFailure(job, "unsupported channel layout");
}

int test_transcoder_allows_71_to_ac3_51_without_downmix_filter() {
	return CheckAutomaticSurroundConversion("AC-3", "pipeline/encoded-71-to-ac3-51.mka");
}

int test_transcoder_allows_71_to_eac3_51_without_downmix_filter() {
	return CheckAutomaticSurroundConversion("E-AC3", "pipeline/encoded-71-to-eac3-51.mka");
}

int test_transcoder_encodes_stereo_audio_to_mp3() {
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	if (!mp3.value().get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;

	const auto output = OutputPath("pipeline/encoded-stereo.mp3");
	auto logger = MakeLogger();
	TEST_PHASE("creating transcoder");
	Transcoder job{TestLocation(FixturePath("audio/noise_stereo.mp3")), TestLocation(output), logger, 2000000000LL};
	job.Audio(0).Codec(mp3.value().get());
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	TEST_PHASE("starting transcoder run");
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return CheckSingleAudioOutput(output, "MP3", "MP3", 2, 48000);
}

int test_transcoder_remuxes_and_encodes_same_audio() {
	return CheckRepeatedAudioMapping(false, false);
}

int test_transcoder_encodes_and_remuxes_same_audio() {
	return CheckRepeatedAudioMapping(true, false);
}

int test_transcoder_encodes_same_audio_twice_with_remux() {
	return CheckRepeatedAudioMapping(false, true);
}

int test_transcoder_remuxes_audio_while_measuring_encode() {
	return CheckRepeatedAudioMapping(false, false, true);
}

int test_transcoder_duplicates_nonzero_audio_origin() {
	return CheckRepeatedAudioMapping(false, false, false, true);
}

int test_transcoder_measures_only_one_duplicate_encode() {
	return CheckRepeatedAudioMapping(false, true, true);
}

int test_transcoder_measures_both_duplicate_encodes() {
	return CheckRepeatedAudioMapping(false, true, true, false, true);
}

int test_transcoder_remuxes_video_with_two_original_and_encoded_audios() {
	const auto eac3 = Registry::Instance().FindCodec("E-AC3");
	TEST_REQUIRE(eac3);
	if (!eac3->get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	const auto output = OutputPath("pipeline/video-remux-duplicate-surround.mkv");
	auto logger = MakeLogger();
	Transcoder job{TestLocation(FixturePath("video/bluray_like_hdr10.mkv")),
		TestLocation(output), logger, 2000000000LL};
	job.Video(0).Remux();
	for (const int origin : {1, 2}) {
		job.Audio(origin).Remux();
		job.Audio(origin).Codec(eac3->get()).BitRate(384000);
	}
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	const auto opened = File::Open(StormByte::Safe::String{output.string()});
	TEST_REQUIRE(opened && opened->Streams().size() == 5);
	constexpr std::array codecs{"H.265"sv, "AC-3"sv, "E-AC3"sv, "AAC"sv, "E-AC3"sv};
	for (std::size_t index = 0; index < codecs.size(); ++index) {
		const auto stream = opened->Streams()[index];
		TEST_REQUIRE(stream.Codec().Name() == codecs[index]);
		if (index != 0) {
			const auto audio = stream.Audio();
			TEST_REQUIRE(audio && audio->Channels() == 6 && audio->SampleRate() == 48000);
		}
	}
	std::size_t decoders = 0;
	std::size_t encoders = 0;
	std::size_t remuxers = 0;
	for (const auto& stage : job.Telemetry()->Stages()) {
		const auto name = TestView(stage.Name);
		if (name.starts_with("Decoder(")) {
			TEST_REQUIRE(name != "Decoder(t=0)"sv);
			++decoders;
		}
		if (name.starts_with("Encoder("))
			++encoders;
		if (name.starts_with("Remuxer("))
			++remuxers;
	}
	TEST_REQUIRE(decoders == 2 && encoders == 2 && remuxers == 3);
	return 0;
}

int test_transcoder_recovers_rejected_initial_avi_mp3_packet() {
	return CheckRejectedAviPacket("01wb", 0, false);
}

int test_transcoder_recovers_rejected_avi_mp3_packet_with_loudnorm() {
	return CheckRejectedAviPacket("01wb", 30, true);
}

int test_transcoder_recovers_rejected_avi_video_packet() {
	return CheckRejectedAviPacket("00dc", 0, false);
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_recovers_rejected_initial_avi_mp3_packet", test_transcoder_recovers_rejected_initial_avi_mp3_packet},
		TestEntry{"test_transcoder_recovers_rejected_avi_mp3_packet_with_loudnorm", test_transcoder_recovers_rejected_avi_mp3_packet_with_loudnorm},
		TestEntry{"test_transcoder_recovers_rejected_avi_video_packet", test_transcoder_recovers_rejected_avi_video_packet},
		TestEntry{"test_transcoder_remuxes_video_with_two_original_and_encoded_audios", test_transcoder_remuxes_video_with_two_original_and_encoded_audios},
		TestEntry{"test_transcoder_remuxes_and_encodes_same_audio", test_transcoder_remuxes_and_encodes_same_audio},
		TestEntry{"test_transcoder_encodes_and_remuxes_same_audio", test_transcoder_encodes_and_remuxes_same_audio},
		TestEntry{"test_transcoder_encodes_same_audio_twice_with_remux", test_transcoder_encodes_same_audio_twice_with_remux},
		TestEntry{"test_transcoder_remuxes_audio_while_measuring_encode", test_transcoder_remuxes_audio_while_measuring_encode},
		TestEntry{"test_transcoder_duplicates_nonzero_audio_origin", test_transcoder_duplicates_nonzero_audio_origin},
		TestEntry{"test_transcoder_measures_only_one_duplicate_encode", test_transcoder_measures_only_one_duplicate_encode},
		TestEntry{"test_transcoder_measures_both_duplicate_encodes", test_transcoder_measures_both_duplicate_encodes},
		TestEntry{"test_transcoder_extracts_single_ac3_track_from_bluray_mkv", test_transcoder_extracts_single_ac3_track_from_bluray_mkv},
		TestEntry{"test_transcoder_extracts_single_aac_track_from_bluray_mkv", test_transcoder_extracts_single_aac_track_from_bluray_mkv},
		TestEntry{"test_transcoder_remuxes_single_opus_file_to_mka", test_transcoder_remuxes_single_opus_file_to_mka},
		TestEntry{"test_transcoder_remuxes_wav_file_to_wav", test_transcoder_remuxes_wav_file_to_wav},
		TestEntry{"test_transcoder_opus_encode_output_file_properties", test_transcoder_opus_encode_output_file_properties},
		TestEntry{"test_transcoder_aac_encode_from_stereo_wav", test_transcoder_aac_encode_from_stereo_wav},
		TestEntry{"test_transcoder_rejects_51_audio_to_mp3_without_downmix", test_transcoder_rejects_51_audio_to_mp3_without_downmix},
		TestEntry{"test_transcoder_allows_71_to_ac3_51_without_downmix_filter", test_transcoder_allows_71_to_ac3_51_without_downmix_filter},
		TestEntry{"test_transcoder_allows_71_to_eac3_51_without_downmix_filter", test_transcoder_allows_71_to_eac3_51_without_downmix_filter},
		TestEntry{"test_transcoder_encodes_stereo_audio_to_mp3", test_transcoder_encodes_stereo_audio_to_mp3},
	};
	return RunSelectedTest(argc, argv, tests);
}
