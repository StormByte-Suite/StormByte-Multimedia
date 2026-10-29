#include "helpers.hxx"

#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using StormByte::Multimedia::Backend::Pipeline::Ceiling;

namespace {
#ifdef __linux__
	std::uint64_t ReadCalls() {
		std::ifstream input("/proc/self/io");
		std::string key;
		std::uint64_t value = 0;
		while (input >> key >> value) {
			if (key == "syscr:")
				return value;
		}
		throw std::runtime_error("cannot read syscr from /proc/self/io");
	}
#endif

	int test_packet_ceilings_do_not_query_memory() {
		auto plan = StormByte::Safe::MakeShared<Plan>(
			TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/ceiling-unused.mkv")), 2000000000LL);
		plan->add(Track{0, StormByte::Multimedia::Type::Video});
		plan->add(Track{1, StormByte::Multimedia::Type::Audio});
#ifdef __linux__
		const auto before = ReadCalls();
#endif
		for (int iteration = 0; iteration < 1000; ++iteration) {
			for (const auto& track : plan->Tracks()) {
				for (const auto producer : std::array{Producer::Demuxer, Producer::Remuxer, Producer::Muxer}) {
					const Ceiling ceiling{plan, producer, *track};
					TEST_REQUIRE(ceiling.Frames() == 0);
					TEST_REQUIRE(ceiling.Packets() == 8);
					TEST_REQUIRE(ceiling.Park() == (producer == Producer::Demuxer ? 8 : 0));
				}
			}
		}
#ifdef __linux__
		TEST_REQUIRE(ReadCalls() - before < 10);
#endif
		const Ceiling decodedVideo{plan, Producer::Decoder, plan->Tracks()[0]};
		TEST_REQUIRE(decodedVideo.Frames() >= 2 && decodedVideo.Frames() <= 32);
		TEST_REQUIRE(decodedVideo.Packets() == decodedVideo.Frames());
		const Ceiling decodedAudio{plan, Producer::Decoder, plan->Tracks()[1]};
		TEST_REQUIRE(decodedAudio.Frames() == 8 && decodedAudio.Packets() == 8);
		const auto fullFrames = decodedVideo.Frames();
		plan->Resources(ResourceUsage::Medium);
		const Ceiling mediumVideo{plan, Producer::Decoder, plan->Tracks()[0]};
		TEST_REQUIRE(mediumVideo.Frames() >= 2 && mediumVideo.Frames() <= fullFrames);
		TEST_REQUIRE(mediumVideo.Packets() == mediumVideo.Frames());
		plan->Resources(ResourceUsage::Low);
		const Ceiling lowVideo{plan, Producer::Decoder, plan->Tracks()[0]};
		TEST_REQUIRE(lowVideo.Frames() >= 2 && lowVideo.Frames() <= mediumVideo.Frames());
		TEST_REQUIRE(lowVideo.Packets() == lowVideo.Frames());
		return 0;
	}

	int test_resource_budget_resolution_and_minima() {
		using Usage = ResourceUsage;
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Full, 0) == 1);
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Full, 16) == 16);
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Medium, 1) == 1);
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Medium, 8) == 4);
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Medium, 9) == 4);
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Low, 1) == 1);
		TEST_REQUIRE(Ceiling::ResourceThreads(Usage::Low, 8) == 2);
		TEST_REQUIRE(Ceiling::StageThreads(Usage::Full, 8, 4) == 0);
		TEST_REQUIRE(Ceiling::StageThreads(Usage::Medium, 8, 4) == 1);
		TEST_REQUIRE(Ceiling::StageThreads(Usage::Low, 16, 2) == 1);
		TEST_REQUIRE(Ceiling::StageThreads(Usage::Low, 16, 0) == 2);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(0) == 1);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(1) == 1);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(2) == 2);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(3) == 3);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(5) == 3);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(6) == 4);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(11) == 4);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(12) == 5);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(23) == 5);
		TEST_REQUIRE(Ceiling::SvtParallelismLevel(24) == 6);
		TEST_REQUIRE(Ceiling::ResourceFrames(Usage::Full, 27) == 27);
		TEST_REQUIRE(Ceiling::ResourceFrames(Usage::Medium, 27) == 14);
		TEST_REQUIRE(Ceiling::ResourceFrames(Usage::Low, 27) == 7);
		TEST_REQUIRE(Ceiling::ResourceFrames(Usage::Medium, 2) == 2);
		TEST_REQUIRE(Ceiling::ResourceFrames(Usage::Low, 0) == 2);
		return 0;
	}
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_resource_budget_resolution_and_minima", test_resource_budget_resolution_and_minima},
		TestEntry{"test_packet_ceilings_do_not_query_memory", test_packet_ceilings_do_not_query_memory},
	};
	return RunSelectedTest(argc, argv, tests);
}
