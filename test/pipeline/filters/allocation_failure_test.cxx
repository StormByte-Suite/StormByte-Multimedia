/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include "../helpers.hxx"

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/nlmeans.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/safe/exception.hxx>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <thread>
#include <utility>

#ifndef __has_feature
#define __has_feature(feature) 0
#endif

#if defined(__linux__) && defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)

namespace {
	namespace Pipeline = StormByte::Multimedia::Pipeline;
	thread_local bool exhausted = false;
	thread_local std::size_t fail_aligned_at = std::numeric_limits<std::size_t>::max();
	thread_local std::size_t denied = 0;
	std::atomic<bool> failed_scratch{false};
	std::atomic<bool> coordinator_failed{false};

	bool Reject(std::size_t size, bool aligned = false) noexcept {
		if (!exhausted && (!aligned || size < fail_aligned_at))
			return false;
		exhausted = true;
		++denied;
		errno = ENOMEM;
		return true;
	}

	class TestStep final: public Pipeline::Step {
		public:
			TestStep(): Step({}, Pipeline::Producer::Encoder, {}, {}) {}
	};

	class ExhaustedJob final: public Pipeline::Transcoder {
		public:
			using Transcoder::Transcoder;
			bool ExhaustWorker = false;
			bool PauseAtPlan = false;
			std::atomic<bool> PlanReady{false};

		protected:
			void OnConfigure() noexcept override {
				if (ExhaustWorker) {
					// Initialize libc++abi's per-thread exception state before denying calloc.
					try {
						throw std::bad_alloc{};
					} catch (const std::bad_alloc&) {
					}
					denied = 0;
					exhausted = true;
				}
			}

			void OnError(const StormByte::Safe::String& message) noexcept override {
				coordinator_failed.store(exhausted && denied > 0 && message.empty(),
					std::memory_order_release);
			}

			void OnPlan(const Pipeline::Plan&) noexcept override {
				if (PauseAtPlan) {
					Pause();
					PlanReady.store(true, std::memory_order_release);
				}
			}

	};

	int TestCoordinatorExhaustion(bool worker) {
		coordinator_failed.store(false, std::memory_order_release);
		ExhaustedJob job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")),
			TestLocation(OutputPath("pipeline/filters/coordinator-allocation-failure.mkv")),
			MakeLogger(), 2000000000LL};
		job.Video(0).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.ExhaustWorker = worker;
		denied = 0;
		exhausted = !worker;
		job.Run();
		if (!worker)
			exhausted = false;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (!coordinator_failed.load(std::memory_order_acquire)
			&& std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		TEST_REQUIRE(coordinator_failed.load(std::memory_order_acquire));
		TEST_REQUIRE(job.Status() == Pipeline::Status::Error);
		TEST_REQUIRE(!job.Error());
		return 0;
	}

	int TestCoordinatorStartExhaustion() {
		return TestCoordinatorExhaustion(false);
	}

	int TestCoordinatorWorkerExhaustion() {
		return TestCoordinatorExhaustion(true);
	}

	int TestWriterDestructionExhaustion() {
		const auto path = TestLocation(OutputPath("pipeline/filters/unused-writer-oom.mkv"));
		{
			StormByte::Buffer::IO::BufferedFileWriter writer{path};
			static_cast<void>(writer.Telemetry());
			denied = 0;
			exhausted = true;
		}
		exhausted = false;
		TEST_REQUIRE(denied > 0);
		return 0;
	}

	int TestCoordinatorCancelExhaustion() {
		ExhaustedJob job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")),
			TestLocation(OutputPath("pipeline/filters/coordinator-cancel-allocation-failure.mkv")),
			MakeLogger(), 2000000000LL};
		job.Video(0).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.PauseAtPlan = true;
		job.Run();
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (!job.PlanReady.load(std::memory_order_acquire)
			&& std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		TEST_REQUIRE(job.PlanReady.load(std::memory_order_acquire));
		denied = 0;
		exhausted = true;
		job.Cancel();
		exhausted = false;
		TEST_REQUIRE(denied > 0);
		TEST_REQUIRE(job.Plan()->State() == Pipeline::Plan::State::Failed);
		return 0;
	}

	class FailingNlMeans final: public Pipeline::Filter::Video::NlMeans {
		public:
			using Pipeline::Filter::FFmpeg::Fail;

			explicit FailingNlMeans(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept
			: NlMeans(std::move(log)) {}

			void Process(const Pipeline::Frame& frame) noexcept override {
				if (!m_warmed) {
					NlMeans::Process(frame);
					m_warmed = true;
					return;
				}
				fail_aligned_at = static_cast<std::size_t>(AVFrame().Width())
					* AVFrame().Height() * sizeof(double);
				NlMeans::Process(frame);
				const bool caught = exhausted && denied > 0 && Failed();
				failed_scratch.store(caught, std::memory_order_release);
			}

		private:
			bool m_warmed = false;
	};

	int TestFailureClosure() {
		TestStep step;
		FailingNlMeans filter({});
		TestStep describedStep;
		FailingNlMeans describedFilter({});
		static_cast<void>(step.Error());
		static_cast<void>(filter.Status());
		exhausted = true;
		denied = 0;
		step.Fail();
		filter.Fail();
		const bool allocation_free = denied == 0;
		describedStep.Fail("stage allocation failure");
		describedFilter.Fail("filter allocation failure");
		const bool message_rejected = denied > 0;
		exhausted = false;
		TEST_REQUIRE(allocation_free);
		TEST_REQUIRE(message_rejected);
		TEST_REQUIRE(step.Failed());
		TEST_REQUIRE(filter.Failed());
		TEST_REQUIRE(!step.Error());
		TEST_REQUIRE(describedStep.Failed() && !describedStep.Error());
		TEST_REQUIRE(describedFilter.Failed());
		TEST_REQUIRE(step.Telemetry()->Status() == Pipeline::State::Failed);
		TEST_REQUIRE(filter.Telemetry()->Status() == Pipeline::State::Failed);
		return 0;
	}

	int TestNlMeansFailureJob() {
		auto codec = StormByte::Multimedia::Registry::Instance().FindCodec("VP9");
		TEST_REQUIRE(codec);
		const auto output = OutputPath("pipeline/filters/nlmeans-allocation-failure.mkv");
		auto logger = MakeLogger();
		failed_scratch.store(false, std::memory_order_release);
		{
			Pipeline::Transcoder job{TestLocation(FixturePath("video/xvid_mp3_stereo.avi")),
				TestLocation(output), logger, 2000000000LL};
			job.Video(0).Codec(codec->get())
				.Implementation(Pipeline::ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"})
				.Acceleration(StormByte::Multimedia::DecodeAcceleration::Software)
				.Filter<FailingNlMeans>(logger);
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			job.Run();
			TEST_REQUIRE(WaitForTranscoderFailure(job) == 0);
			TEST_REQUIRE(job.Status() == Pipeline::Status::Error);
		}
		TEST_REQUIRE(failed_scratch.load(std::memory_order_acquire));
		return 0;
	}
}

extern "C" void* __libc_malloc(std::size_t) noexcept;
extern "C" void* __libc_calloc(std::size_t, std::size_t) noexcept;
extern "C" void* __libc_realloc(void*, std::size_t) noexcept;
extern "C" void* __libc_memalign(std::size_t, std::size_t) noexcept;

extern "C" void* malloc(std::size_t size) noexcept {
	return Reject(size) ? nullptr : __libc_malloc(size);
}

extern "C" void* calloc(std::size_t count, std::size_t size) noexcept {
	return Reject(size) ? nullptr : __libc_calloc(count, size);
}

extern "C" void* realloc(void* pointer, std::size_t size) noexcept {
	return Reject(size) ? nullptr : __libc_realloc(pointer, size);
}

extern "C" int posix_memalign(void** pointer, std::size_t alignment, std::size_t size) noexcept {
	if (alignment < sizeof(void*) || alignment % sizeof(void*) != 0
		|| (alignment & (alignment - 1)) != 0)
		return EINVAL;
	if (Reject(size, true))
		return ENOMEM;
	void* allocation = __libc_memalign(alignment, size);
	if (!allocation)
		return ENOMEM;
	*pointer = allocation;
	return 0;
}

extern "C" void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
	return Reject(size, true) ? nullptr : __libc_memalign(alignment, size);
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_pipeline_failure_closure_without_allocations", TestFailureClosure},
		TestEntry{"test_nlmeans_scratch_failure_fails_job", TestNlMeansFailureJob},
		TestEntry{"test_transcoder_start_without_allocations", TestCoordinatorStartExhaustion},
		TestEntry{"test_transcoder_worker_without_allocations", TestCoordinatorWorkerExhaustion},
		TestEntry{"test_transcoder_cancel_without_allocations", TestCoordinatorCancelExhaustion},
		TestEntry{"test_writer_destruction_without_allocations", TestWriterDestructionExhaustion}
	};
	return RunSelectedTest(argc, argv, tests);
}

#else

int main() {
	std::fputs("Allocation interposition requires Linux/glibc without AddressSanitizer.\n", stderr);
	return 77;
}

#endif
