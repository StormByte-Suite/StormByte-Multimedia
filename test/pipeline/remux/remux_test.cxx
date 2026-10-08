#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/config/attachment.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <array>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {
	int CheckRemuxOutput(const std::filesystem::path& output, std::size_t expectedAttachments = 1) {
		auto opened = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(opened);
		const auto& result = opened.value();
		std::size_t videos = 0;
		std::size_t audios = 0;
		std::size_t subtitles = 0;
		bool foundJapanese = false;
		for (const auto& stream : result.Streams()) {
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

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_remux_preserves_pgs_and_attachments", test_transcoder_remux_preserves_pgs_and_attachments},
		TestEntry{"test_transcoder_remux_omits_unselected_attachments", test_transcoder_remux_omits_unselected_attachments},
		TestEntry{"test_hand_built_pipe_remux_preserves_pgs_and_attachments", test_hand_built_pipe_remux_preserves_pgs_and_attachments},
	};
	return RunSelectedTest(argc, argv, tests);
}
