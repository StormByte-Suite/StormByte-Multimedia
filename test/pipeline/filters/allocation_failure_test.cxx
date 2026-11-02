/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include "../helpers.hxx"

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/nlmeans.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <StormByte/safe/exception.hxx>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>
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
		TestEntry{"test_nlmeans_scratch_failure_fails_job", TestNlMeansFailureJob}
	};
	return RunSelectedTest(argc, argv, tests);
}

#else

int main() {
	std::fputs("Allocation interposition requires Linux/glibc without AddressSanitizer.\n", stderr);
	return 77;
}

#endif
