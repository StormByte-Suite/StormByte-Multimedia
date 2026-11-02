/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/multimedia/registry.hxx>
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

static_assert(!noexcept(Registry::Instance()));
static_assert(!noexcept(std::declval<const Registry&>().CodecList(MediaType::Video)));
static_assert(!noexcept(std::declval<const Registry&>().ContainerList()));
static_assert(!noexcept(std::declval<const Registry&>().FindCodec("hevc")));
static_assert(!noexcept(std::declval<const Registry&>().FindContainer("mkv")));

#ifndef __has_feature
#define __has_feature(feature) 0
#endif

#if defined(__linux__) && defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)

namespace {
	bool fail_allocations = false;
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
	if (fail_allocations && size >= 512)
		return ENOMEM;
	void* allocation = __libc_memalign(alignment, size);
	if (!allocation)
		return ENOMEM;
	*pointer = allocation;
	return 0;
}

extern "C" void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
	if (fail_allocations && size >= 512) {
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
	return errors;
}

#else

int main() {
	std::fputs("Allocation interposition requires Linux/glibc without AddressSanitizer; compile-time contracts were checked.\n", stderr);
	return 77;
}

#endif
