/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/multimedia/registry.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/safe/exception.hxx>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <string>
#include <utility>

using StormByte::Multimedia::Registry;
using MediaType = enum StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::StageTelemetry;
using StormByte::Multimedia::Pipeline::JobTelemetry;
using StormByte::Multimedia::Pipeline::Filter::FFmpeg;
using StormByte::Multimedia::Pipeline::Filters;
using StormByte::Multimedia::Pipeline::Step;
using StormByte::Safe::String;

static_assert(!noexcept(Registry::Instance()));
static_assert(!noexcept(std::declval<const Registry&>().CodecList(MediaType::Video)));
static_assert(!noexcept(std::declval<const Registry&>().ContainerList()));
static_assert(!noexcept(std::declval<const Registry&>().FindCodec("hevc")));
static_assert(!noexcept(std::declval<const Registry&>().FindContainer("mkv")));
static_assert(!noexcept(std::declval<const StageTelemetry&>().Origin()));
static_assert(!noexcept(std::declval<const StageTelemetry&>().Error()));
static_assert(!noexcept(std::declval<const StageTelemetry&>().SetOrigin("stage")));
static_assert(!noexcept(std::declval<StageTelemetry&>().SetError(std::declval<String>())));
static_assert(!noexcept(std::declval<const JobTelemetry&>().Stages()));
static_assert(!noexcept(std::declval<JobTelemetry&>().RegisterStage(std::declval<String>(), {})));
static_assert(!noexcept(std::declval<const JobTelemetry&>().MemoryCurrent()));
static_assert(!noexcept(std::declval<const JobTelemetry&>().MemoryMinimum()));
static_assert(!noexcept(std::declval<const JobTelemetry&>().MemoryMaximum()));
static_assert(!noexcept(std::declval<const JobTelemetry&>().PeakMemory()));
static_assert(!noexcept(std::declval<const FFmpeg&>().Name()));
static_assert(!noexcept(std::declval<const FFmpeg&>().Telemetry()));
static_assert(!noexcept(std::declval<const Step&>().Telemetry()));
static_assert(!noexcept(std::declval<const Filters&>().StageTelemetries()));

#ifndef __has_feature
#define __has_feature(feature) 0
#endif

#if defined(__linux__) && defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)

namespace {
	bool fail_allocations = false;
	std::size_t aligned_failure_size = 512;

	class TelemetryTestFilter final: public FFmpeg {
		static std::string_view LongName() {
			static const std::string name(4096, 'n');
			return name;
		}

		public:
			TelemetryTestFilter(): FFmpeg({}, LongName(), {}, {}) {}

			enum StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Video;
			}

			void Clean() noexcept override {}

			void Setup() noexcept override {}
	};
}

// Interposition is confined to this single-threaded glibc regression executable.
extern "C" void* __libc_malloc(std::size_t) noexcept;
extern "C" void* __libc_realloc(void*, std::size_t) noexcept;
extern "C" void* __libc_memalign(std::size_t, std::size_t) noexcept;

extern "C" void* malloc(std::size_t size) noexcept {
	if (fail_allocations && size >= 512) {
		errno = ENOMEM;
		return nullptr;
	}
	return __libc_malloc(size);
}

extern "C" void* realloc(void* pointer, std::size_t size) noexcept {
	if (fail_allocations && size >= 512) {
		errno = ENOMEM;
		return nullptr;
	}
	return __libc_realloc(pointer, size);
}

extern "C" int posix_memalign(void** pointer, std::size_t alignment, std::size_t size) noexcept {
	if (alignment < sizeof(void*) || alignment % sizeof(void*) != 0
		|| (alignment & (alignment - 1)) != 0)
		return EINVAL;
	if (fail_allocations && size >= aligned_failure_size)
		return ENOMEM;
	void* allocation = __libc_memalign(alignment, size);
	if (!allocation)
		return ENOMEM;
	*pointer = allocation;
	return 0;
}

extern "C" void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
	if (fail_allocations && size >= aligned_failure_size) {
		errno = ENOMEM;
		return nullptr;
	}
	return __libc_memalign(alignment, size);
}

template<class Operation>
int ExpectAllocationFailure(const char* operation, Operation run) {
	bool caught = false;
	fail_allocations = true;
	try {
		run();
	} catch (const StormByte::Safe::AllocationError&) {
		caught = true;
	} catch (const std::bad_alloc&) {
		caught = true;
	}
	fail_allocations = false;
	if (caught)
		return 0;
	std::fprintf(stderr, "%s did not propagate an allocation failure\n", operation);
	return 1;
}

int CheckTelemetry() {
	const std::string label(4096, 'x');
	auto stage = StormByte::Safe::Shared<StageTelemetry>::MakePointer<StageTelemetry>();
	stage->SetOrigin(label);
	int errors = ExpectAllocationFailure("origin write", [&] { stage->SetOrigin(label); });
	errors += ExpectAllocationFailure("origin snapshot", [&] { stage->Origin(); });
	aligned_failure_size = 0;
	errors += ExpectAllocationFailure("error write", [&] { stage->SetError(String{"failed"}); });
	if (stage->Error()) {
		std::fputs("failed error write published partial data\n", stderr);
		++errors;
	}
	stage->SetError(String{"failed"});
	errors += ExpectAllocationFailure("error snapshot", [&] { stage->Error(); });
	JobTelemetry job;
	auto filter = StormByte::Safe::Shared<TelemetryTestFilter>::MakePointer<TelemetryTestFilter>();
	errors += ExpectAllocationFailure("filter name", [&] { filter->Name(); });
	errors += ExpectAllocationFailure("filter telemetry", [&] { filter->Telemetry(); });
	errors += ExpectAllocationFailure("stage registration", [&] {
		job.RegisterStage(String{"stage"}, stage);
	});
	if (!job.Stages().empty()) {
		std::fputs("failed registration published a partial stage\n", stderr);
		++errors;
	}
	job.RegisterStage(String{"stage"}, stage);
	errors += ExpectAllocationFailure("stage snapshot", [&] { job.Stages(); });
	job.SampleMemory();
	if (job.MemorySamples() == 0) {
		std::fputs("Linux memory sampling produced no sample\n", stderr);
		++errors;
	} else {
		errors += ExpectAllocationFailure("current memory", [&] { job.MemoryCurrent(); });
		errors += ExpectAllocationFailure("minimum memory", [&] { job.MemoryMinimum(); });
		errors += ExpectAllocationFailure("maximum memory", [&] { job.MemoryMaximum(); });
		errors += ExpectAllocationFailure("peak memory", [&] { job.PeakMemory(); });
	}
	if (job.Stages().size() != StormByte::Size{1} || !stage->Error()
		|| stage->Origin() != "stage") {
		std::fputs("failed telemetry operations corrupted retained values\n", stderr);
		++errors;
	}
	return errors;
}

int main() {
	std::set_terminate([] { std::_Exit(86); });
	int errors = ExpectAllocationFailure("initialization", [] { Registry::Instance(); });
	auto& registry = Registry::Instance();
	const auto codec = registry.FindCodec("hevc");
	const auto container = registry.FindContainer("mkv");
	if (!codec || !container) {
		std::fputs("registry initialization could not be retried\n", stderr);
		return 1;
	}
	errors += ExpectAllocationFailure("codec snapshot", [&] { registry.CodecList(MediaType::Video); });
	errors += ExpectAllocationFailure("container snapshot", [&] { registry.ContainerList(); });
	const std::string missing(4096, 'x');
	errors += ExpectAllocationFailure("codec lookup", [&] { registry.FindCodec(missing); });
	errors += ExpectAllocationFailure("container lookup", [&] { registry.FindContainer(missing); });
	if (registry.CodecList(MediaType::Video).empty() || registry.ContainerList().empty()
		|| !registry.FindCodec("hevc") || !registry.FindContainer("mkv")) {
		std::fputs("allocation failures changed registry contents\n", stderr);
		++errors;
	}
	return errors + CheckTelemetry();
}

#else

int main() {
	std::fputs("Allocation interposition requires Linux/glibc without AddressSanitizer; compile-time contracts were checked.\n", stderr);
	return 77;
}

#endif
