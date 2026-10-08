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

#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/safe/unique_lock.hxx>

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace StormByte::Multimedia::Pipeline;

namespace {
	std::int64_t NowNs() noexcept {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void UpdateMaximum(StormByte::Safe::Atomic<std::int64_t>& target, std::int64_t value) noexcept {
		std::int64_t current = target.load(StormByte::Safe::MemoryOrder::Relaxed);
		while (current < value && !target.compare_exchange_weak(current, value,
				StormByte::Safe::MemoryOrder::Relaxed, StormByte::Safe::MemoryOrder::Relaxed)) {}
	}

	void UpdateMinimum(StormByte::Safe::Atomic<std::int64_t>& target, std::int64_t value) noexcept {
		std::int64_t current = target.load(StormByte::Safe::MemoryOrder::Relaxed);
		while (current > value && !target.compare_exchange_weak(current, value,
				StormByte::Safe::MemoryOrder::Relaxed, StormByte::Safe::MemoryOrder::Relaxed)) {}
	}

	void UpdateMaximum(StormByte::Safe::Atomic<std::uint64_t>& target, std::uint64_t value) noexcept {
		std::uint64_t current = target.load(StormByte::Safe::MemoryOrder::Relaxed);
		while (current < value && !target.compare_exchange_weak(current, value,
				StormByte::Safe::MemoryOrder::Relaxed, StormByte::Safe::MemoryOrder::Relaxed)) {}
	}

	void UpdateMinimum(StormByte::Safe::Atomic<std::uint64_t>& target, std::uint64_t value) noexcept {
		std::uint64_t current = target.load(StormByte::Safe::MemoryOrder::Relaxed);
		while (current > value && !target.compare_exchange_weak(current, value,
				StormByte::Safe::MemoryOrder::Relaxed, StormByte::Safe::MemoryOrder::Relaxed)) {}
	}

	std::optional<std::uint64_t> ResidentBytes() noexcept {
#if defined(_WIN32)
		PROCESS_MEMORY_COUNTERS_EX counters{};
		counters.cb = sizeof(counters);
		if (!GetProcessMemoryInfo(GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
			return std::nullopt;
		return static_cast<std::uint64_t>(counters.WorkingSetSize);
#elif defined(__APPLE__)
		mach_task_basic_info_data_t info{};
		mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
		if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
			reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
			return std::nullopt;
		return static_cast<std::uint64_t>(info.resident_size);
#elif defined(__linux__)
		const int fd = open("/proc/self/statm", O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			return std::nullopt;
		char text[128]{};
		const ssize_t length = read(fd, text, sizeof(text));
		close(fd);
		if (length <= 0)
			return std::nullopt;
		std::string_view contents{text, static_cast<std::size_t>(length)};
		const auto separator = contents.find(' ');
		if (separator == std::string_view::npos)
			return std::nullopt;
		contents.remove_prefix(separator + 1);
		std::uint64_t resident_pages = 0;
		const auto parsed = std::from_chars(contents.data(), contents.data() + contents.size(), resident_pages);
		if (parsed.ec != std::errc{})
			return std::nullopt;
		const long page_size = sysconf(_SC_PAGESIZE);
		if (page_size <= 0)
			return std::nullopt;
		return resident_pages * static_cast<std::uint64_t>(page_size);
#else
		return std::nullopt;
#endif
	}

	const char* StateName(State state) noexcept {
		switch (state) {
			case State::Created: return "created";
			case State::Ready: return "ready";
			case State::Stopping: return "stopping";
			case State::Stopped: return "stopped";
			case State::Failed: return "failed";
			default: return "unknown";
		}
	}

	std::string OptionalBytes(const StormByte::Safe::Optional<std::uint64_t>& value) {
		return value ? std::format("{} B", value.value()) : "unavailable";
	}
}

StageTelemetry::StageTelemetry() noexcept
: m_process_calls(0),
	m_input_frames(0),
	m_input_packets(0),
	m_output_frames(0),
	m_output_packets(0),
	m_process_total_ns(0),
	m_process_min_ns(std::numeric_limits<std::int64_t>::max()),
	m_process_max_ns(0),
	m_wait_count(0),
	m_wait_total_ns(0),
	m_wait_max_ns(0),
	m_setup_ns(0),
	m_started_ns(0),
	m_finished_ns(0),
	m_state(static_cast<int>(State::Created)) {}

StageTelemetry::~StageTelemetry() noexcept = default;

void StageTelemetry::SetOrigin(std::string_view origin) const noexcept {
	StormByte::Safe::UniqueLock lock(m_origin_lock);
	m_origin = StormByte::Safe::String{origin};
}

StormByte::Safe::String StageTelemetry::Origin() const noexcept {
	StormByte::Safe::UniqueLock lock(m_origin_lock);
	return m_origin;
}

void StageTelemetry::RecordInput(Kind kind) noexcept {
	if (kind == Kind::Frame)
		m_input_frames.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
	else if (kind == Kind::Packet)
		m_input_packets.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
}

void StageTelemetry::RecordOutput(Kind kind) noexcept {
	if (kind == Kind::Frame)
		m_output_frames.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
	else if (kind == Kind::Packet)
		m_output_packets.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
}

void StageTelemetry::RecordProcess(std::chrono::nanoseconds duration) noexcept {
	const std::int64_t ns = std::max<std::int64_t>(0, duration.count());
	m_process_calls.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
	m_process_total_ns.fetch_add(ns, StormByte::Safe::MemoryOrder::Relaxed);
	UpdateMinimum(m_process_min_ns, ns);
	UpdateMaximum(m_process_max_ns, ns);
}

void StageTelemetry::RecordWait(std::chrono::nanoseconds duration) noexcept {
	const std::int64_t ns = std::max<std::int64_t>(0, duration.count());
	m_wait_count.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
	m_wait_total_ns.fetch_add(ns, StormByte::Safe::MemoryOrder::Relaxed);
	UpdateMaximum(m_wait_max_ns, ns);
}

void StageTelemetry::RecordSetup(std::chrono::nanoseconds duration) noexcept {
	m_setup_ns.store(std::max<std::int64_t>(0, duration.count()), StormByte::Safe::MemoryOrder::Relaxed);
}

void StageTelemetry::SetState(State state) noexcept {
	m_state.store(static_cast<int>(state), StormByte::Safe::MemoryOrder::Release);
}

void StageTelemetry::SetError(StormByte::Safe::String reason) noexcept {
	StormByte::Safe::UniqueLock lock(m_error_lock);
	m_error = std::move(reason);
	SetState(State::Failed);
}

void StageTelemetry::Start() noexcept {
	m_started_ns.store(NowNs(), StormByte::Safe::MemoryOrder::Release);
	m_finished_ns.store(0, StormByte::Safe::MemoryOrder::Release);
}

void StageTelemetry::Finish() noexcept {
	m_finished_ns.store(NowNs(), StormByte::Safe::MemoryOrder::Release);
}

std::uint64_t StageTelemetry::ProcessCalls() const noexcept {
	return m_process_calls.load(StormByte::Safe::MemoryOrder::Relaxed);
}

std::uint64_t StageTelemetry::InputFrames() const noexcept {
	return m_input_frames.load(StormByte::Safe::MemoryOrder::Relaxed);
}

std::uint64_t StageTelemetry::InputPackets() const noexcept {
	return m_input_packets.load(StormByte::Safe::MemoryOrder::Relaxed);
}

std::uint64_t StageTelemetry::OutputFrames() const noexcept {
	return m_output_frames.load(StormByte::Safe::MemoryOrder::Relaxed);
}

std::uint64_t StageTelemetry::OutputPackets() const noexcept {
	return m_output_packets.load(StormByte::Safe::MemoryOrder::Relaxed);
}

std::chrono::nanoseconds StageTelemetry::ProcessTotal() const noexcept {
	return std::chrono::nanoseconds(m_process_total_ns.load(StormByte::Safe::MemoryOrder::Relaxed));
}

std::chrono::nanoseconds StageTelemetry::ProcessMean() const noexcept {
	const auto count = ProcessCalls();
	if (count == 0)
		return std::chrono::nanoseconds::zero();
	return ProcessTotal() / static_cast<std::chrono::nanoseconds::rep>(count);
}

std::chrono::nanoseconds StageTelemetry::ProcessMinimum() const noexcept {
	const auto value = m_process_min_ns.load(StormByte::Safe::MemoryOrder::Relaxed);
	return value == std::numeric_limits<std::int64_t>::max()
		? std::chrono::nanoseconds::zero() : std::chrono::nanoseconds(value);
}

std::chrono::nanoseconds StageTelemetry::ProcessMaximum() const noexcept {
	return std::chrono::nanoseconds(m_process_max_ns.load(StormByte::Safe::MemoryOrder::Relaxed));
}

std::uint64_t StageTelemetry::WaitCount() const noexcept {
	return m_wait_count.load(StormByte::Safe::MemoryOrder::Relaxed);
}

std::chrono::nanoseconds StageTelemetry::WaitTotal() const noexcept {
	return std::chrono::nanoseconds(m_wait_total_ns.load(StormByte::Safe::MemoryOrder::Relaxed));
}

std::chrono::nanoseconds StageTelemetry::WaitMaximum() const noexcept {
	return std::chrono::nanoseconds(m_wait_max_ns.load(StormByte::Safe::MemoryOrder::Relaxed));
}

std::chrono::nanoseconds StageTelemetry::SetupTime() const noexcept {
	return std::chrono::nanoseconds(m_setup_ns.load(StormByte::Safe::MemoryOrder::Relaxed));
}

std::chrono::nanoseconds StageTelemetry::Elapsed() const noexcept {
	const std::int64_t started = m_started_ns.load(StormByte::Safe::MemoryOrder::Acquire);
	if (started == 0)
		return std::chrono::nanoseconds::zero();
	const std::int64_t finished = m_finished_ns.load(StormByte::Safe::MemoryOrder::Acquire);
	return std::chrono::nanoseconds(std::max<std::int64_t>(0,
		(finished == 0 ? NowNs() : finished) - started));
}

State StageTelemetry::Status() const noexcept {
	return static_cast<State>(m_state.load(StormByte::Safe::MemoryOrder::Acquire));
}

StormByte::Safe::Optional<StormByte::Safe::String> StageTelemetry::Error() const noexcept {
	StormByte::Safe::UniqueLock lock(m_error_lock);
	return m_error;
}

StageTelemetry::operator StormByte::Safe::String() const {
	const auto process_total = std::chrono::duration_cast<std::chrono::microseconds>(ProcessTotal()).count();
	const auto process_mean = std::chrono::duration_cast<std::chrono::microseconds>(ProcessMean()).count();
	const auto process_min = std::chrono::duration_cast<std::chrono::microseconds>(ProcessMinimum()).count();
	const auto process_max = std::chrono::duration_cast<std::chrono::microseconds>(ProcessMaximum()).count();
	const auto wait_total = std::chrono::duration_cast<std::chrono::microseconds>(WaitTotal()).count();
	const auto wait_max = std::chrono::duration_cast<std::chrono::microseconds>(WaitMaximum()).count();
	const auto setup = std::chrono::duration_cast<std::chrono::microseconds>(SetupTime()).count();
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Elapsed()).count();
	const auto error = Error();
	std::string report = std::format(
		"origin={} state={} calls={} in(frame={},packet={}) out(frame={},packet={}) "
		"process(total={}us,mean={}us,min={}us,max={}us) "
		"wait(n={},total={}us,max={}us) setup={}us elapsed={}us",
		static_cast<std::string>(Origin()), StateName(Status()), ProcessCalls(),
		InputFrames(), InputPackets(),
		OutputFrames(), OutputPackets(), process_total, process_mean,
		process_min, process_max, WaitCount(), wait_total, wait_max, setup, elapsed);
	if (error)
		report += std::format(" error={}", static_cast<std::string>(error.value()));
	return StormByte::Safe::String{std::string_view{report}};
}

JobTelemetry::JobTelemetry() noexcept
: m_memory_current(0),
	m_memory_min(std::numeric_limits<std::uint64_t>::max()),
	m_memory_max(0),
	m_memory_samples(0) {}

JobTelemetry::~JobTelemetry() noexcept = default;

void JobTelemetry::RegisterStage(StormByte::Safe::String name,
	StormByte::Safe::Shared<const StageTelemetry> metrics) noexcept {
	if (!metrics)
		return;
	StormByte::Safe::UniqueLock lock(m_stages_lock);
	if (std::find_if(m_stages.begin(), m_stages.end(), [&metrics](const Stage& stage) {
			return stage.Metrics.get() == metrics.get();
		}) != m_stages.end())
		return;
	metrics->SetOrigin(static_cast<std::string_view>(name));
	m_stages.push_back(Stage{std::move(name), std::move(metrics)});
}

StormByte::Safe::Vector<JobTelemetry::Stage> JobTelemetry::Stages() const noexcept {
	StormByte::Safe::UniqueLock lock(m_stages_lock);
	return m_stages;
}

void JobTelemetry::SampleMemory() noexcept {
	const auto resident = ResidentBytes();
	if (!resident)
		return;
	m_memory_current.store(*resident, StormByte::Safe::MemoryOrder::Relaxed);
	UpdateMinimum(m_memory_min, *resident);
	UpdateMaximum(m_memory_max, *resident);
	m_memory_samples.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::Relaxed);
}

StormByte::Safe::Optional<std::uint64_t> JobTelemetry::MemoryCurrent() const noexcept {
	if (MemorySamples() == 0)
		return std::nullopt;
	return m_memory_current.load(StormByte::Safe::MemoryOrder::Relaxed);
}

StormByte::Safe::Optional<std::uint64_t> JobTelemetry::MemoryMinimum() const noexcept {
	if (MemorySamples() == 0)
		return std::nullopt;
	return m_memory_min.load(StormByte::Safe::MemoryOrder::Relaxed);
}

StormByte::Safe::Optional<std::uint64_t> JobTelemetry::MemoryMaximum() const noexcept {
	if (MemorySamples() == 0)
		return std::nullopt;
	return m_memory_max.load(StormByte::Safe::MemoryOrder::Relaxed);
}

StormByte::Safe::Optional<std::uint64_t> JobTelemetry::PeakMemory() const noexcept {
	return MemoryMaximum();
}

std::uint64_t JobTelemetry::MemorySamples() const noexcept {
	return m_memory_samples.load(StormByte::Safe::MemoryOrder::Relaxed);
}

JobTelemetry::operator StormByte::Safe::String() const {
	std::string report = std::format("process_rss(current={},min={},max={},peak={},samples={})",
		OptionalBytes(MemoryCurrent()), OptionalBytes(MemoryMinimum()),
		OptionalBytes(MemoryMaximum()), OptionalBytes(PeakMemory()), MemorySamples());
	for (const auto& entry: Stages()) {
		const Stage stage = entry;
		const std::string metrics = static_cast<std::string>(*stage.Metrics);
		report += std::format("\n{}: {}", static_cast<std::string>(stage.Name), metrics);
	}
	return StormByte::Safe::String{std::string_view{report}};
}
