#include "../test_helpers.hxx"

#include <StormByte/multimedia/file.hxx>

#include <array>
#include <string_view>

using namespace StormByte::Multimedia;
using namespace std::string_view_literals;
using MediaType = StormByte::Multimedia::Type;

namespace {
	auto OpenFixture(std::string_view path) {
		return File::Open(StormByte::Safe::String{FixturePath(path).string()});
	}

	int CheckTrack(const Stream& stream, int index, MediaType type,
		std::string_view codec, std::string_view language = {}, std::string_view title = {}) {
		TEST_REQUIRE(stream.Index() == index);
		TEST_REQUIRE(stream.Type() == type);
		TEST_REQUIRE(stream.Codec().Name() == codec);
		if (!language.empty()) {
			const auto actual = stream.Metadata().Language();
			TEST_REQUIRE(actual);
			TEST_REQUIRE(TestView(actual.value()) == language);
		}
		if (!title.empty()) {
			const auto actual = stream.Metadata().Title();
			TEST_REQUIRE(actual);
			TEST_REQUIRE(TestView(actual.value()) == title);
		}
		return 0;
	}

	int CheckVideo(const Stream& stream, std::uint32_t width, std::uint32_t height,
		StormByte::Multimedia::Property::PixelFormat pixelFormat,
		StormByte::Multimedia::Property::Primaries primaries,
		StormByte::Multimedia::Property::Transfer transfer,
		StormByte::Safe::Optional<StormByte::Multimedia::Property::HDR10::Source> hdr10Source) {
		const auto properties = stream.Video();
		TEST_REQUIRE(properties);
		TEST_REQUIRE(properties.value().Resolution().Width() == width);
		TEST_REQUIRE(properties.value().Resolution().Height() == height);
		TEST_REQUIRE(properties.value().Color().PixelFormat() == pixelFormat);
		TEST_REQUIRE(properties.value().Color().Primaries() == primaries);
		TEST_REQUIRE(properties.value().Color().Transfer() == transfer);
		TEST_REQUIRE(properties.value().HDR10().has_value() == hdr10Source.has_value());
		if (hdr10Source)
			TEST_REQUIRE(properties.value().HDR10()->Origin() == hdr10Source.value());
		return 0;
	}

	int CheckAudio(const Stream& stream, std::uint8_t channels, std::uint32_t sampleRate) {
		const auto properties = stream.Audio();
		TEST_REQUIRE(properties);
		TEST_REQUIRE(properties.value().Channels() == channels);
		TEST_REQUIRE(properties.value().SampleRate() == sampleRate);
		if (channels == 6)
			TEST_REQUIRE(properties.value().Layout() == StormByte::Multimedia::Property::ChannelLayout::FivePointOne);
		else if (channels == 2)
			TEST_REQUIRE(properties.value().Layout() == StormByte::Multimedia::Property::ChannelLayout::Stereo);
		return 0;
	}
}

int test_bluray_like_mkv_streams_and_cover() {
	auto opened = OpenFixture("video/bluray_like_hdr10.mkv");
	TEST_REQUIRE(opened);
	const auto& file = opened.value();
	TEST_REQUIRE(file.Container().Name() == "Matroska");
	const auto streams = file.Streams();
	TEST_REQUIRE(streams.size() == 5);
	TEST_REQUIRE(CheckTrack(streams[0], 0, MediaType::Video, "H.265"sv, {},
		"Synthetic HEVC HDR10 white-noise video"sv) == 0);
	TEST_REQUIRE(CheckVideo(streams[0], 1920, 1080,
		StormByte::Multimedia::Property::PixelFormat::YUV420P10,
		StormByte::Multimedia::Property::Primaries::BT2020,
		StormByte::Multimedia::Property::Transfer::SMPTE2084,
		StormByte::Multimedia::Property::HDR10::Source::Metadata) == 0);
	TEST_REQUIRE(CheckTrack(streams[1], 1, MediaType::Audio, "AC-3"sv, "eng"sv,
		"English 5.1 white noise"sv) == 0);
	TEST_REQUIRE(CheckAudio(streams[1], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(streams[2], 2, MediaType::Audio, "AAC"sv, "spa"sv,
		"Spanish 5.1 white noise"sv) == 0);
	TEST_REQUIRE(CheckAudio(streams[2], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(streams[3], 3, MediaType::Subtitle, "PGS"sv, "eng"sv, "English PGS"sv) == 0);
	TEST_REQUIRE(CheckTrack(streams[4], 4, MediaType::Subtitle, "PGS"sv, "spa"sv, "Spanish PGS"sv) == 0);
	TEST_REQUIRE(file.Attachments().size() == 1);
	const auto attachments = file.Attachments();
	const auto name = attachments[0].FileName();
	TEST_REQUIRE(name);
	TEST_REQUIRE(TestView(name.value()) == "synthetic-cover.png"sv);
	const auto mime = attachments[0].MimeType();
	TEST_REQUIRE(mime);
	TEST_REQUIRE(TestView(mime.value()) == "image/png"sv);
	TEST_REQUIRE(attachments[0].Payload().Available() > 0);
	return 0;
}

int test_anime_like_mkv_languages_and_font() {
	auto opened = OpenFixture("video/anime_like.mkv");
	TEST_REQUIRE(opened);
	const auto& file = opened.value();
	const auto streams = file.Streams();
	TEST_REQUIRE(streams.size() == 5);
	TEST_REQUIRE(CheckTrack(streams[0], 0, MediaType::Video, "H.265"sv, {},
		"Synthetic anime-like HEVC video"sv) == 0);
	TEST_REQUIRE(CheckVideo(streams[0], 1920, 1080,
		StormByte::Multimedia::Property::PixelFormat::YUV420P10,
		StormByte::Multimedia::Property::Primaries::BT2020,
		StormByte::Multimedia::Property::Transfer::SMPTE2084,
		StormByte::Multimedia::Property::HDR10::Source::Heuristics) == 0);
	TEST_REQUIRE(CheckTrack(streams[1], 1, MediaType::Audio, "Opus"sv, "eng"sv,
		"English 5.1 white noise"sv) == 0);
	TEST_REQUIRE(CheckAudio(streams[1], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(streams[2], 2, MediaType::Subtitle, "PGS"sv, "spa"sv, "Spanish PGS"sv) == 0);
	TEST_REQUIRE(CheckTrack(streams[3], 3, MediaType::Subtitle, "PGS"sv, "eng"sv, "English PGS"sv) == 0);
	TEST_REQUIRE(CheckTrack(streams[4], 4, MediaType::Subtitle, "PGS"sv, "jpn"sv, "Japanese PGS"sv) == 0);
	TEST_REQUIRE(file.Attachments().size() == 1);

	bool sawFont = false;
	for (const auto attachment : file.Attachments()) {
		const auto name = attachment.FileName();
		TEST_REQUIRE(name);
		sawFont = sawFont || TestView(name.value()) == "ipag-mona.ttf"sv;
	}
	TEST_REQUIRE(sawFont);
	const auto attachment = file.Attachments()[0];
	const auto fontName = attachment.FileName();
	const auto fontMime = attachment.MimeType();
	TEST_REQUIRE(fontName && TestView(fontName.value()) == "ipag-mona.ttf"sv);
	TEST_REQUIRE(fontMime && TestView(fontMime.value()) == "application/x-truetype-font"sv);
	TEST_REQUIRE(attachment.Payload().Available() > 0);
	return 0;
}

int test_vp9_mp4_and_avi_container_snapshots() {
	auto vp9 = OpenFixture("video/bluray_like_vp9.mkv");
	TEST_REQUIRE(vp9);
	TEST_REQUIRE(vp9.value().Container().Name() == "Matroska");
	const auto vp9Streams = vp9.value().Streams();
	TEST_REQUIRE(vp9Streams.size() == 5);
	TEST_REQUIRE(CheckTrack(vp9Streams[0], 0, MediaType::Video, "VP9"sv, {},
		"Synthetic VP9 white-noise video"sv) == 0);
	TEST_REQUIRE(CheckVideo(vp9Streams[0], 1280, 720,
		StormByte::Multimedia::Property::PixelFormat::YUV420P,
		StormByte::Multimedia::Property::Primaries::Unspecified,
		StormByte::Multimedia::Property::Transfer::Unspecified, std::nullopt) == 0);
	TEST_REQUIRE(CheckTrack(vp9Streams[1], 1, MediaType::Audio, "AC-3"sv, "eng"sv,
		"English 5.1 white noise"sv) == 0);
	TEST_REQUIRE(CheckAudio(vp9Streams[1], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(vp9Streams[2], 2, MediaType::Audio, "AAC"sv, "spa"sv,
		"Spanish 5.1 white noise"sv) == 0);
	TEST_REQUIRE(CheckAudio(vp9Streams[2], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(vp9Streams[3], 3, MediaType::Subtitle, "PGS"sv, "eng"sv, "English PGS"sv) == 0);
	TEST_REQUIRE(CheckTrack(vp9Streams[4], 4, MediaType::Subtitle, "PGS"sv, "spa"sv, "Spanish PGS"sv) == 0);

	auto mp4 = OpenFixture("video/bluray_like_hdr10.mp4");
	TEST_REQUIRE(mp4);
	TEST_REQUIRE(mp4.value().Container().Name() == "MP4");
	const auto mp4Streams = mp4.value().Streams();
	TEST_REQUIRE(mp4Streams.size() == 5);
	TEST_REQUIRE(CheckTrack(mp4Streams[0], 0, MediaType::Video, "H.264"sv) == 0);
	TEST_REQUIRE(CheckVideo(mp4Streams[0], 1920, 1080,
		StormByte::Multimedia::Property::PixelFormat::YUV420P,
		StormByte::Multimedia::Property::Primaries::Unspecified,
		StormByte::Multimedia::Property::Transfer::Unspecified, std::nullopt) == 0);
	TEST_REQUIRE(CheckTrack(mp4Streams[1], 1, MediaType::Audio, "AAC"sv, "eng"sv) == 0);
	TEST_REQUIRE(CheckAudio(mp4Streams[1], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(mp4Streams[2], 2, MediaType::Audio, "AAC"sv, "spa"sv) == 0);
	TEST_REQUIRE(CheckAudio(mp4Streams[2], 6, 48000) == 0);
	TEST_REQUIRE(CheckTrack(mp4Streams[3], 3, MediaType::Subtitle, "3GPP Timed Text"sv, "eng"sv) == 0);
	TEST_REQUIRE(CheckTrack(mp4Streams[4], 4, MediaType::Subtitle, "3GPP Timed Text"sv, "spa"sv) == 0);

	auto avi = OpenFixture("video/xvid_mp3_stereo.avi");
	TEST_REQUIRE(avi);
	TEST_REQUIRE(avi.value().Container().Name() == "AVI");
	const auto aviStreams = avi.value().Streams();
	TEST_REQUIRE(aviStreams.size() == 2);
	TEST_REQUIRE(CheckTrack(aviStreams[0], 0, MediaType::Video, "MPEG-4 Part 2"sv) == 0);
	TEST_REQUIRE(CheckVideo(aviStreams[0], 640, 360,
		StormByte::Multimedia::Property::PixelFormat::YUV420P,
		StormByte::Multimedia::Property::Primaries::Unspecified,
		StormByte::Multimedia::Property::Transfer::Unspecified, std::nullopt) == 0);
	TEST_REQUIRE(CheckTrack(aviStreams[1], 1, MediaType::Audio, "MP3"sv) == 0);
	TEST_REQUIRE(CheckAudio(aviStreams[1], 2, 48000) == 0);
	return 0;
}

int test_stereo_and_51_audio_fixtures() {
	struct AudioExpectation {
		std::string_view path;
		std::string_view codec;
		std::uint8_t channels;
	};
	constexpr std::array inputs{
		AudioExpectation{"audio/noise_stereo.wav", "PCM S16 LE", 2},
		AudioExpectation{"audio/noise_stereo.mp3", "MP3", 2},
		AudioExpectation{"audio/noise_51.m4a", "AAC", 6},
		AudioExpectation{"audio/noise_51.opus", "Opus", 6},
		AudioExpectation{"audio/noise_opus_source_51.opus", "Opus", 6},
		AudioExpectation{"audio/noise_opus_destination_51.opus", "Opus", 6},
	};
	for (const auto& expected : inputs) {
		auto opened = OpenFixture(expected.path);
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Type() == MediaType::Audio);
		TEST_REQUIRE(stream.Codec().Name() == expected.codec);
		auto audio = stream.Audio();
		TEST_REQUIRE(audio);
		TEST_REQUIRE(audio.value().Channels() == expected.channels);
	}
	return 0;
}

int test_random_invalid_file_is_rejected() {
	auto opened = OpenFixture("invalid/random_garbage.mkv");
	TEST_REQUIRE(!opened);
	TEST_REQUIRE(opened.error());
	TEST_REQUIRE(!std::string_view{opened.error()->what()}.empty());
	return 0;
}

int test_truncated_file_is_rejected() {
	auto opened = OpenFixture("invalid/truncated_hevc.mkv");
	TEST_REQUIRE(!opened);
	TEST_REQUIRE(opened.error());
	TEST_REQUIRE(!std::string_view{opened.error()->what()}.empty());
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_bluray_like_mkv_streams_and_cover", test_bluray_like_mkv_streams_and_cover},
		TestEntry{"test_anime_like_mkv_languages_and_font", test_anime_like_mkv_languages_and_font},
		TestEntry{"test_vp9_mp4_and_avi_container_snapshots", test_vp9_mp4_and_avi_container_snapshots},
		TestEntry{"test_stereo_and_51_audio_fixtures", test_stereo_and_51_audio_fixtures},
		TestEntry{"test_random_invalid_file_is_rejected", test_random_invalid_file_is_rejected},
		TestEntry{"test_truncated_file_is_rejected", test_truncated_file_is_rejected},
	};
	return RunSelectedTest(argc, argv, tests);
}
