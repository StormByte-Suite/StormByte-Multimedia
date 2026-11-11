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

#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/backend/pipeline/queue_probe.hxx>
#include <StormByte/multimedia/backend/pipeline/topology.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <cstdlib>
#include <limits>
#include <optional>

#ifndef __has_feature
#define __has_feature(feature) 0
#endif

#if defined(__linux__) && defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)
#define TOPOLOGY_ALLOCATION_INTERPOSITION 1
#endif

using StormByte::ByteSize;
using StormByte::Multimedia::Backend::Pipeline::BudgetIdentity;
using StormByte::Multimedia::Backend::Pipeline::IdentifyBudget;
using StormByte::Multimedia::Backend::Pipeline::Pipe;
using StormByte::Multimedia::Backend::Pipeline::QueueProbe;
using StormByte::Multimedia::Backend::Pipeline::Topology;
using StormByte::Multimedia::Backend::Pipeline::TopologyDescriptor;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;

namespace {
	thread_local std::size_t reject_after = std::numeric_limits<std::size_t>::max();
	thread_local std::size_t denied = 0;
	// Countdown applied only on the transcoder coordinator thread; negative when disarmed.
	std::atomic<long> coordinator_reject{-1};
	std::atomic<std::size_t> coordinator_denied{0};

	[[maybe_unused]] bool CoordinatorThread() noexcept {
		char name[16] = {};
		return pthread_getname_np(pthread_self(), name, sizeof(name)) == 0
			&& std::strcmp(name, "MM-Transcoder") == 0;
	}

	[[maybe_unused]] bool Reject(bool runtime = false) noexcept {
		// glibc registers TLS destructors through calloc and aborts if that fails; spare it.
		if (!runtime && coordinator_reject.load(std::memory_order_relaxed) >= 0 && CoordinatorThread()) {
			if (coordinator_reject.fetch_sub(1, std::memory_order_acq_rel) == 0) {
				coordinator_denied.fetch_add(1, std::memory_order_relaxed);
				errno = ENOMEM;
				return true;
			}
		}
		if (reject_after == std::numeric_limits<std::size_t>::max())
			return false;
		if (reject_after-- != 0)
			return false;
		reject_after = std::numeric_limits<std::size_t>::max();
		++denied;
		errno = ENOMEM;
		return true;
	}

	std::optional<QueueProbe::Reading> FindReading(const QueueProbe& probe, int key, bool pending = false) {
		for (auto& reading : probe.Read())
			if (reading.Key == key && reading.Pending == pending)
				return reading;
		return std::nullopt;
	}

	void Register(Topology& topology, const Pipe& pipe) {
		TopologyDescriptor descriptor;
		descriptor.Kind = TopologyStageKind::Encoder;
		descriptor.Probe = pipe.Probe();
		topology.Register(std::move(descriptor));
	}

	int test_budget_identity_survives_close() {
		StormByte::Buffer::ByteBudget budget{ByteSize{64}};
		const auto first = IdentifyBudget(budget);
		TEST_REQUIRE(first);
		budget.Close();
		const auto again = IdentifyBudget(budget);
		TEST_REQUIRE(again);
		TEST_REQUIRE(again->Id == first->Id);
		return 0;
	}

	int test_closed_budget_identity_is_deduplicated() {
		StormByte::Buffer::ByteBudget closed{ByteSize{64}};
		closed.Close();
		const auto before = closed.Snapshot();
		StormByte::Buffer::ByteBudget other{ByteSize{64}};
		other.Close();
		BufferingPolicy policy;
		policy.Budget(closed);
		BufferingPolicy other_policy;
		other_policy.Budget(other);
		Pipe source;
		Pipe a;
		Pipe b;
		Pipe c;
		a.Buffering(policy, StormByte::Size{2});
		b.Buffering(policy, StormByte::Size{2});
		c.Buffering(other_policy, StormByte::Size{2});
		source.To(0) >> a;
		source.To(1) >> b;
		source.To(2) >> c;
		Topology topology;
		topology.Reset();
		Register(topology, a);
		Register(topology, b);
		Register(topology, c);
		const auto snapshot = topology.Snapshot();
		TEST_REQUIRE(snapshot.Complete);
		TEST_REQUIRE(snapshot.Budgets.size() == 2);
		std::size_t queues = 0;
		for (const auto& entry : snapshot.Budgets) {
			TEST_REQUIRE(entry.Identified && entry.Snapshot.Closed);
			queues += entry.Queues.size();
			TEST_REQUIRE(entry.Queues.size() == 1 || entry.Queues.size() == 2);
		}
		TEST_REQUIRE(queues == 3);
		const auto after = closed.Snapshot();
		TEST_REQUIRE(before.Reserved == after.Reserved && before.Peak == after.Peak);
		TEST_REQUIRE(before.Pressure == after.Pressure && before.Waits == after.Waits);
		TEST_REQUIRE(before.WaitNanoseconds == after.WaitNanoseconds && before.Closed == after.Closed);
		const auto first = IdentifyBudget(closed);
		const auto distinct = IdentifyBudget(other);
		TEST_REQUIRE(first && distinct && first->Id != distinct->Id);
		std::atomic<bool> matching{true};
		std::vector<std::thread> readers;
		for (int i = 0; i < 8; ++i) {
			readers.emplace_back([&] {
				for (int j = 0; j < 500; ++j) {
					const auto shared = IdentifyBudget(closed);
					const auto separate = IdentifyBudget(other);
					if (!shared || !separate || shared->Id != first->Id || separate->Id != distinct->Id)
						matching.store(false, std::memory_order_relaxed);
				}
			});
		}
		for (auto& reader : readers)
			reader.join();
		TEST_REQUIRE(matching.load(std::memory_order_relaxed));
		topology.Freeze();
		const auto final = topology.Snapshot();
		TEST_REQUIRE(final.Complete && final.State == TopologySnapshotState::Final);
		TEST_REQUIRE(final.Budgets.size() == 2);
		TEST_REQUIRE(IdentifyBudget(closed)->Id == first->Id);
		auto moved = std::move(other);
		TEST_REQUIRE(!IdentifyBudget(other));
		a.Probe()->Budget(9, other);
		TEST_REQUIRE(a.Probe()->Incomplete());
		return 0;
	}

	int test_pinned_budget_stays_identified_after_close() {
		StormByte::Buffer::ByteBudget budget{ByteSize{64}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Topology topology;
		auto pin = IdentifyBudget(budget);
		topology.Reset();
		topology.Pin(std::move(pin));
		budget.Close();
		Pipe source;
		Pipe a;
		Pipe b;
		a.Buffering(policy, StormByte::Size{2});
		b.Buffering(policy, StormByte::Size{2});
		source.To(0) >> a;
		source.To(1) >> b;
		Register(topology, a);
		Register(topology, b);
		const auto live = topology.Snapshot();
		TEST_REQUIRE(live.Budgets.size() == 1);
		TEST_REQUIRE(live.Budgets.front().Queues.size() == 2);
		const auto id = live.Budgets.front().Id;
		topology.Freeze();
		const auto frozen = topology.Snapshot();
		TEST_REQUIRE(frozen.State == TopologySnapshotState::Final);
		TEST_REQUIRE(frozen.Budgets.size() == 1 && frozen.Budgets.front().Id == id);
		// The frozen run retains the budget identity for the next run.
		const auto next = IdentifyBudget(budget);
		TEST_REQUIRE(next && next->Id == id);
		return 0;
	}

	int test_topology_marks_lost_registrations_incomplete() {
		Topology topology;
		topology.Reset();
		Pipe pipe;
		Register(topology, pipe);
		TEST_REQUIRE(topology.Snapshot().Complete);
		topology.Register(TopologyDescriptor{});
		TEST_REQUIRE(!topology.Snapshot().Complete);
		topology.Reset();
		Register(topology, pipe);
		TEST_REQUIRE(topology.Snapshot().Complete);
		topology.MarkIncomplete();
		TEST_REQUIRE(!topology.Snapshot().Complete);
		topology.Freeze();
		TEST_REQUIRE(!topology.Snapshot().Complete);
		return 0;
	}

#ifdef TOPOLOGY_ALLOCATION_INTERPOSITION
	int test_job_telemetry_construction_allocation_failure() {
		unsigned failures = 0;
		bool succeeded = false;
		for (std::size_t position = 0; position < 64; ++position) {
			reject_after = position;
			try {
				JobTelemetry telemetry;
				reject_after = std::numeric_limits<std::size_t>::max();
				TEST_REQUIRE(!telemetry.Topology().RecorderFailed);
				succeeded = true;
				break;
			} catch (const StormByte::Safe::AllocationError&) {
				++failures;
			} catch (const std::bad_alloc&) {
				++failures;
			}
			reject_after = std::numeric_limits<std::size_t>::max();
		}
		TEST_REQUIRE(failures > 0 && succeeded);
		return 0;
	}

	// Fails the n-th allocation of @p call; every failure must be reported, never silent.
	template<typename Call, typename Recorded>
	int CheckReported(Call call, Recorded recorded) {
		unsigned failures = 0;
		for (std::size_t position = 0; position < 64; ++position) {
			Pipe pipe;
			Topology topology;
			topology.Reset();
			Register(topology, pipe);
			const auto probe = pipe.Probe();
			const auto before = denied;
			reject_after = position;
			call(*probe);
			reject_after = std::numeric_limits<std::size_t>::max();
			const bool failed = denied != before;
			if (failed) {
				++failures;
				TEST_REQUIRE(probe->Incomplete());
				TEST_REQUIRE(!topology.Snapshot().Complete);
				continue;
			}
			TEST_REQUIRE(!probe->Incomplete());
			TEST_REQUIRE(topology.Snapshot().Complete);
			TEST_REQUIRE(recorded(*probe));
			break;
		}
		TEST_REQUIRE(failures > 0);
		return 0;
	}

	int test_probe_producer_allocation_failure_is_reported() {
		return CheckReported([](QueueProbe& probe) { probe.Producer(7, 99); },
			[](const QueueProbe& probe) {
				const auto reading = FindReading(probe, 7);
				return reading && reading->Producers.size() == 1 && reading->Producers.front() == 99;
			});
	}

	int test_probe_budget_allocation_failure_is_reported() {
		StormByte::Buffer::ByteBudget budget{ByteSize{64}};
		const int result = CheckReported([&budget](QueueProbe& probe) { probe.Budget(5, budget); },
			[](const QueueProbe& probe) {
				const auto reading = FindReading(probe, 5);
				return reading && reading->Budget;
			});
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return result;
	}

	int test_probe_pending_allocation_failure_is_reported() {
		return CheckReported([](QueueProbe& probe) { probe.PendingOpen(3, StormByte::Size{4}); },
			[](const QueueProbe& probe) {
				const auto reading = FindReading(probe, 3, true);
				return reading && reading->Snapshot.Exists && reading->Snapshot.Limits.Elements() == StormByte::Size{4};
			});
	}

	int test_topology_begin_allocation_failure_is_reported() {
		unsigned failures = 0;
		for (std::size_t position = 0; position < 64; ++position) {
			// A fresh budget each round avoids an earlier identity satisfying the lookup.
			StormByte::Buffer::ByteBudget budget{ByteSize{64}};
			Topology topology;
			const auto before = denied;
			reject_after = position;
			topology.Begin(budget);
			reject_after = std::numeric_limits<std::size_t>::max();
			Pipe pipe;
			Register(topology, pipe);
			const auto snapshot = topology.Snapshot();
			if (denied != before) {
				++failures;
				TEST_REQUIRE(!snapshot.Complete);
				continue;
			}
			TEST_REQUIRE(snapshot.Complete);
			// Successful Begin pins an identity that outlives the budget's closure.
			budget.Close();
			TEST_REQUIRE(IdentifyBudget(budget));
			break;
		}
		TEST_REQUIRE(failures > 0);
		return 0;
	}

	int test_topology_freeze_allocation_failure_is_not_final() {
		unsigned failures = 0;
		bool succeeded = false;
		for (std::size_t position = 0; position < 256 && !succeeded; ++position) {
			StormByte::Buffer::ByteBudget budget{ByteSize{64}};
			BufferingPolicy policy;
			policy.Budget(budget);
			Topology topology;
			topology.Begin(budget);
			{
				Pipe source;
				Pipe a;
				Pipe b;
				a.Buffering(policy, StormByte::Size{2});
				b.Buffering(policy, StormByte::Size{2});
				source.To(0) >> a;
				source.To(1) >> b;
				Register(topology, a);
				Register(topology, b);
				TEST_REQUIRE(topology.Snapshot().Complete);
			}
			const auto before = denied;
			reject_after = position;
			topology.Freeze();
			reject_after = std::numeric_limits<std::size_t>::max();
			const auto first = topology.Snapshot();
			TEST_REQUIRE(first.Budgets.size() == 1 && first.Queues.size() == 2);
			// Another user of the shared budget keeps changing it after teardown.
			StormByte::Buffer::ByteBudget::Lease lease;
			TEST_REQUIRE(budget.TryReserve(ByteSize{16}, lease) == StormByte::Buffer::QueueStatus::Ok);
			const auto second = topology.Snapshot();
			for (const auto& queue : first.Queues)
				TEST_REQUIRE(queue.State == TopologySnapshotState::Final);
			// Some denied allocations are tolerated (best-effort reserve), so classify by outcome.
			if (first.State != TopologySnapshotState::Final) {
				++failures;
				TEST_REQUIRE(denied != before);
				TEST_REQUIRE(first.State == TopologySnapshotState::Unfrozen && !first.Complete);
				TEST_REQUIRE(first.Budgets.front().State == TopologySnapshotState::Live);
				TEST_REQUIRE(second.State == TopologySnapshotState::Unfrozen);
				TEST_REQUIRE(second.Budgets.front().Snapshot.Reserved == ByteSize{16});
				continue;
			}
			succeeded = denied == before;
			TEST_REQUIRE(first.Complete);
			TEST_REQUIRE(first.Budgets.front().State == TopologySnapshotState::Final);
			TEST_REQUIRE(second.State == TopologySnapshotState::Final);
			TEST_REQUIRE(second.Budgets.front().Snapshot.Reserved == ByteSize{0});
		}
		TEST_REQUIRE(failures > 0 && succeeded);
		return 0;
	}

	// Recorder acquisition is among the coordinator's first allocations: sweep until it fails.
	int test_topology_recorder_allocation_failure_is_reported() {
		auto logger = MakeLogger();
		bool recorderFailed = false;
		for (long position = 0; position < 48 && !recorderFailed; ++position) {
			StormByte::Safe::Shared<const JobTelemetry> telemetry;
			bool done = false;
			{
				Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
					TestLocation(OutputPath("pipeline/topology/recorder-oom.mkv")), logger, 2008000000LL};
				job.Video(0).Remux();
				job.Audio(1).Remux();
				TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
				telemetry = job.Telemetry();
				const auto before = telemetry->Topology();
				TEST_REQUIRE(!before.Available && before.Complete && !before.RecorderFailed);
				coordinator_denied.store(0);
				coordinator_reject.store(position);
				job.Run();
				const auto deadline = std::chrono::steady_clock::now() + 30s;
				while (std::chrono::steady_clock::now() < deadline) {
					const auto status = job.Status();
					if (status == Status::Done || status == Status::Error || status == Status::Aborted
						|| status == Status::Stopped)
						break;
					std::this_thread::sleep_for(5ms);
				}
				coordinator_reject.store(-1);
				done = job.Status() == Status::Done;
			}
			const auto snapshot = telemetry->Topology();
			if (snapshot.RecorderFailed) {
				recorderFailed = true;
				TEST_REQUIRE(coordinator_denied.load() > 0);
				TEST_REQUIRE(!snapshot.Available && !snapshot.Complete);
				TEST_REQUIRE(snapshot.Stages.empty() && snapshot.Queues.empty() && snapshot.Budgets.empty());
				// Telemetry is diagnostic: losing it never fails the job.
				TEST_REQUIRE(done);
				continue;
			}
			// A finished run is never indistinguishable from one that did not run.
			if (done)
				TEST_REQUIRE(snapshot.Available || !snapshot.Complete);
		}
		TEST_REQUIRE(recorderFailed);
		return 0;
	}
#else
	int test_job_telemetry_construction_allocation_failure() { return TEST_SKIP; }
	int test_topology_recorder_allocation_failure_is_reported() { return TEST_SKIP; }
	int test_topology_begin_allocation_failure_is_reported() { return TEST_SKIP; }
	int test_topology_freeze_allocation_failure_is_not_final() { return TEST_SKIP; }
	int test_probe_producer_allocation_failure_is_reported() { return TEST_SKIP; }
	int test_probe_budget_allocation_failure_is_reported() { return TEST_SKIP; }
	int test_probe_pending_allocation_failure_is_reported() { return TEST_SKIP; }
#endif
}

#ifdef TOPOLOGY_ALLOCATION_INTERPOSITION
extern "C" void* __libc_malloc(std::size_t) noexcept;
extern "C" void* __libc_calloc(std::size_t, std::size_t) noexcept;
extern "C" void* __libc_realloc(void*, std::size_t) noexcept;
extern "C" void* __libc_memalign(std::size_t, std::size_t) noexcept;

extern "C" void* malloc(std::size_t size) noexcept {
	return Reject() ? nullptr : __libc_malloc(size);
}

extern "C" void* calloc(std::size_t count, std::size_t size) noexcept {
	return Reject(true) ? nullptr : __libc_calloc(count, size);
}

extern "C" void* realloc(void* pointer, std::size_t size) noexcept {
	return Reject() ? nullptr : __libc_realloc(pointer, size);
}

extern "C" int posix_memalign(void** pointer, std::size_t alignment, std::size_t size) noexcept {
	if (alignment < sizeof(void*) || alignment % sizeof(void*) != 0 || (alignment & (alignment - 1)) != 0)
		return EINVAL;
	if (Reject())
		return ENOMEM;
	void* allocation = __libc_memalign(alignment, size);
	if (!allocation)
		return ENOMEM;
	*pointer = allocation;
	return 0;
}

extern "C" void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
	return Reject() ? nullptr : __libc_memalign(alignment, size);
}
#endif

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_budget_identity_survives_close", test_budget_identity_survives_close},
		TestEntry{"test_closed_budget_identity_is_deduplicated", test_closed_budget_identity_is_deduplicated},
		TestEntry{"test_pinned_budget_stays_identified_after_close", test_pinned_budget_stays_identified_after_close},
		TestEntry{"test_topology_marks_lost_registrations_incomplete", test_topology_marks_lost_registrations_incomplete},
		TestEntry{"test_probe_producer_allocation_failure_is_reported", test_probe_producer_allocation_failure_is_reported},
		TestEntry{"test_probe_budget_allocation_failure_is_reported", test_probe_budget_allocation_failure_is_reported},
		TestEntry{"test_probe_pending_allocation_failure_is_reported", test_probe_pending_allocation_failure_is_reported},
		TestEntry{"test_topology_begin_allocation_failure_is_reported", test_topology_begin_allocation_failure_is_reported},
		TestEntry{"test_topology_freeze_allocation_failure_is_not_final", test_topology_freeze_allocation_failure_is_not_final},
		TestEntry{"test_topology_recorder_allocation_failure_is_reported", test_topology_recorder_allocation_failure_is_reported},
		TestEntry{"test_job_telemetry_construction_allocation_failure", test_job_telemetry_construction_allocation_failure}
	};
	return RunSelectedTest(argc, argv, tests);
}
