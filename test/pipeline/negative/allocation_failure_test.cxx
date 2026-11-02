/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include "../helpers.hxx"

#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/safe/exception.hxx>

#include <array>
#include <atomic>
#include <chrono>
#include <new>
#include <thread>
#include <utility>

namespace {
	namespace Pipeline = StormByte::Multimedia::Pipeline;

	class FailingPlanJob final: public Pipeline::Transcoder {
		public:
			using Transcoder::Transcoder;
			using Transcoder::DurationProgress;
			using Transcoder::EmptyPlan;
			using Transcoder::EmptySettled;

			std::atomic<unsigned> Errors{0};
			std::atomic<bool> EmptyError{false};
			std::atomic<bool> Done{false};
			bool EmptyOwner = false;
			bool SafeFailure = false;

		protected:
			StormByte::Safe::Shared<Pipeline::Plan> EmptyPlan(
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader>,
				StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter>,
				StormByte::Safe::Optional<std::int64_t>,
				const StormByte::Multimedia::Container*) const override {
				if (EmptyOwner)
					return {};
				if (SafeFailure)
					throw StormByte::Safe::AllocationError{};
				throw std::bad_alloc{};
			}

			void OnError(const StormByte::Safe::String& message) noexcept override {
				EmptyError.store(message.empty(), std::memory_order_relaxed);
				Errors.fetch_add(1, std::memory_order_release);
			}

			void OnDone() noexcept override {
				Done.store(true, std::memory_order_release);
			}
	};

	static_assert(!noexcept(std::declval<const FailingPlanJob&>().DurationProgress()));
	static_assert(!noexcept(std::declval<const FailingPlanJob&>().EmptySettled()));

	bool WaitForError(const FailingPlanJob& job) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (job.Errors.load(std::memory_order_acquire) == 0
			&& std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return job.Errors.load(std::memory_order_acquire) != 0;
	}

	int TestPlanAllocationFailure() {
		for (const bool safeFailure : {false, true}) {
			FailingPlanJob job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")),
				TestLocation(OutputPath("pipeline/negative/plan-allocation-failure.mkv")), MakeLogger(), 2000000000LL};
			job.SafeFailure = safeFailure;
			job.Video(0).Remux();
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			TEST_REQUIRE(WaitForError(job));
			TEST_REQUIRE(job.Status() == Pipeline::Status::Error);
			TEST_REQUIRE(job.EmptyError.load(std::memory_order_relaxed));
			TEST_REQUIRE(!job.Error());
			TEST_REQUIRE(!job.Done.load(std::memory_order_acquire));
			TEST_REQUIRE(job.Errors.load(std::memory_order_acquire) == 1);
		}
		return 0;
	}

	int TestEmptyPlanOwner() {
		FailingPlanJob job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")),
			TestLocation(OutputPath("pipeline/negative/empty-plan-owner.mkv")), MakeLogger(), 2000000000LL};
		job.EmptyOwner = true;
		job.Video(0).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitForTranscoderFailure(job) == 0);
		TEST_REQUIRE(WaitForError(job));
		TEST_REQUIRE(job.Error());
		TEST_REQUIRE(!job.EmptyError.load(std::memory_order_relaxed));
		TEST_REQUIRE(job.Errors.load(std::memory_order_acquire) == 1);
		return 0;
	}
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_transcoder_plan_allocation_failure", TestPlanAllocationFailure},
		TestEntry{"test_transcoder_empty_plan_owner", TestEmptyPlanOwner}
	};
	return RunSelectedTest(argc, argv, tests);
}
