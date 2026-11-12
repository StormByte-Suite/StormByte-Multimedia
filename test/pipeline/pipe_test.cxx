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

#include "helpers.hxx"

#include <StormByte/multimedia/backend/pipeline/activity.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/backend/pipeline/queue_probe.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

using StormByte::ByteSize;
using StormByte::Multimedia::Backend::Pipeline::Pipe;
using StormByte::Multimedia::Backend::Pipeline::QueueProbe;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;

namespace {
	using StormByte::Multimedia::Backend::Pipeline::ActivityBinding;
	using StormByte::Multimedia::Backend::Pipeline::ActivityScope;

	bool Conserved(const WorkerActivitySnapshot& value) {
		const auto total = value.Unknown + value.Preparing + value.Processing + value.WaitingInput
			+ value.WaitingOutput + value.WaitingBudget + value.Paused;
		// All boundaries use the same integer-nanosecond tick: tolerance is zero.
		return total == value.Elapsed() && value.Unknown >= 0ns && value.Preparing >= 0ns
			&& value.Processing >= 0ns && value.WaitingInput >= 0ns && value.WaitingOutput >= 0ns
			&& value.WaitingBudget >= 0ns && value.Paused >= 0ns;
	}

	bool Await(const WorkerActivity& worker, ActivityState state) {
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (std::chrono::steady_clock::now() < deadline) {
			if (worker.Snapshot().State == state)
				return true;
			std::this_thread::sleep_for(1ms);
		}
		return false;
	}

	// -------------------
	// Activity
	// -------------------
	int test_activity_input_wait_open_interval() {
		Pipe pipe;
		StageTelemetry stage;
		std::atomic<bool> ready{false};
		std::thread thread([&] {
			ActivityBinding binding(&stage.MainActivity(), stage);
			ActivityScope process(ActivityState::Processing);
			pipe.WaitWake(&ready, [](void* data) noexcept {
				return static_cast<std::atomic<bool>*>(data)->load();
			});
		});
		const bool waiting = Await(stage.MainActivity(), ActivityState::WaitingInput);
		const auto first = stage.MainActivity().Snapshot();
		std::this_thread::sleep_for(20ms);
		const auto second = stage.MainActivity().Snapshot();
		ready.store(true);
		pipe.Wake();
		thread.join();
		TEST_REQUIRE(waiting && Conserved(first) && Conserved(second));
		TEST_REQUIRE(second.WaitingInput - first.WaitingInput >= 20ms);
		TEST_REQUIRE(second.Processing == first.Processing);
		TEST_REQUIRE(stage.MainActivity().Snapshot().State == ActivityState::Finished);
		return 0;
	}

	int test_activity_nested_states_and_generation() {
		StageTelemetry stage;
		auto& worker = stage.MainActivity();
		TEST_REQUIRE(!stage.Activity().Available);
		WorkerActivitySnapshot first;
		WorkerActivitySnapshot second;
		{
			ActivityBinding binding(&worker, stage);
			ActivityScope processing(ActivityState::Processing);
			first = worker.Snapshot();
			std::this_thread::sleep_for(20ms);
			second = worker.Snapshot();
			TEST_REQUIRE(first.State == ActivityState::Processing && second.Processing - first.Processing >= 20ms);
			{
				ActivityScope input(ActivityState::WaitingInput);
				std::this_thread::sleep_for(10ms);
				{
					ActivityScope output(ActivityState::WaitingOutput);
					std::this_thread::sleep_for(10ms);
					{
						ActivityScope budget(ActivityState::WaitingBudget);
						std::this_thread::sleep_for(10ms);
						TEST_REQUIRE(worker.Snapshot().State == ActivityState::WaitingBudget);
					}
					TEST_REQUIRE(worker.Snapshot().State == ActivityState::WaitingOutput);
				}
				TEST_REQUIRE(worker.Snapshot().State == ActivityState::WaitingInput);
			}
			const auto before = worker.Snapshot();
			{
				ActivityScope paused(ActivityState::Paused);
				const auto paused_first = worker.Snapshot();
				std::this_thread::sleep_for(20ms);
				const auto paused_second = worker.Snapshot();
				TEST_REQUIRE(paused_second.Paused - paused_first.Paused >= 20ms);
				TEST_REQUIRE(paused_second.Processing == paused_first.Processing);
				TEST_REQUIRE(Conserved(paused_second));
			}
			const auto after = worker.Snapshot();
			TEST_REQUIRE(after.Paused - before.Paused >= 20ms);
		}
		const auto final = worker.Snapshot();
		TEST_REQUIRE(Conserved(final) && final.State == ActivityState::Finished);
		TEST_REQUIRE(final.WaitingInput >= 10ms && final.WaitingOutput >= 10ms && final.WaitingBudget >= 10ms);
		worker.Exchange(ActivityState::Processing);
		worker.Finish(true);
		std::this_thread::sleep_for(10ms);
		TEST_REQUIRE(worker.Snapshot() == final);
		worker.Start();
		const auto restarted = worker.Snapshot();
		TEST_REQUIRE(restarted.Id == final.Id && restarted.Generation == final.Generation + 1);
		TEST_REQUIRE(restarted.State == ActivityState::Preparing);
		worker.Finish(true);
		TEST_REQUIRE(worker.Snapshot().State == ActivityState::Failed);
		TEST_REQUIRE(Conserved(worker.Snapshot()));
		TEST_REQUIRE(Conserved(first) && Conserved(second));
		TEST_REQUIRE(second.Processing - first.Processing <= second.SampledAt - first.SampledAt);
		return 0;
	}

	int CheckAdmissionActivity(bool budget_wait) {
		StormByte::Buffer::ByteBudget budget{ByteSize{1}};
		StormByte::Buffer::ByteBudget::Lease lease;
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe consumer;
		consumer.Buffering(policy, StormByte::Size{1});
		producer.To(-1) >> consumer;
		if (budget_wait) {
			TEST_REQUIRE(budget.TryReserve(ByteSize{1}, lease) == StormByte::Buffer::QueueStatus::Ok);
		}
		else
			producer << StormByte::Safe::MakeShared<Packet>();
		StageTelemetry stage;
		std::thread thread([&] {
			ActivityBinding binding(&stage.MainActivity(), stage);
			ActivityScope process(ActivityState::Processing);
			producer << StormByte::Safe::MakeShared<Packet>();
		});
		const auto state = budget_wait ? ActivityState::WaitingBudget : ActivityState::WaitingOutput;
		const bool waiting = Await(stage.MainActivity(), state);
		const auto first = stage.MainActivity().Snapshot();
		std::this_thread::sleep_for(150ms);
		const auto second = stage.MainActivity().Snapshot();
		if (budget_wait)
			lease.Reset();
		else {
			Pipe::Item::PointerType item;
			consumer >> item;
		}
		thread.join();
		TEST_REQUIRE(waiting && Conserved(first) && Conserved(second));
		// Admission retries every 100 ms; time checking admission is processing, not a wait.
		const auto processing = second.Processing - first.Processing;
		TEST_REQUIRE(processing >= 0ns);
		if (budget_wait) {
			TEST_REQUIRE(second.WaitingBudget - first.WaitingBudget >= 20ms);
			TEST_REQUIRE(second.WaitingOutput == 0ns);
			TEST_REQUIRE(second.WaitingBudget - first.WaitingBudget + processing == second.SampledAt - first.SampledAt);
		}
		else {
			TEST_REQUIRE(second.WaitingOutput - first.WaitingOutput >= 20ms);
			TEST_REQUIRE(second.WaitingBudget == 0ns);
			TEST_REQUIRE(second.WaitingOutput - first.WaitingOutput + processing == second.SampledAt - first.SampledAt);
		}
		TEST_REQUIRE(Conserved(stage.MainActivity().Snapshot()));
		return 0;
	}

	int test_activity_output_wait_open_interval() {
		return CheckAdmissionActivity(false);
	}

	int test_activity_shared_budget_wait_open_interval() {
		return CheckAdmissionActivity(true);
	}

	// -------------------
	// Wiring
	// -------------------
	int test_accounting_precedes_first_push() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		policy.QueueBytes(ByteSize{12});
		Pipe producer;
		Pipe consumer;
		consumer.Buffering(policy, StormByte::Size{2});
		producer.To(-1) >> consumer;
		TEST_REQUIRE(consumer.Snapshot(-1).Limits.Elements() == StormByte::Size{2});
		Pipe::Item::PointerType item = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(producer.Out().TryPush(-1, item, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{7});
		consumer.Capacity(-1, StormByte::Size{2});
		const auto snapshot = consumer.Snapshot(-1);
		TEST_REQUIRE(snapshot.Elements == StormByte::Size{1});
		TEST_REQUIRE(snapshot.Bytes == ByteSize{7});
		TEST_REQUIRE(snapshot.Limits.Elements() == StormByte::Size{2});
		TEST_REQUIRE(snapshot.Limits.Bytes() == ByteSize{12});
		TEST_REQUIRE(!snapshot.Closed);
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(consumer.InputEof());
		return 0;
	}

	int test_clone_accounting_precedes_first_push() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe primary;
		Pipe consumer;
		consumer.Buffering(policy);
		producer.To(-1) >> primary;
		producer.CloneTo(-1, consumer);
		producer << StormByte::Safe::MakeShared<Packet>();
		consumer.Capacity(-1, StormByte::Size{2});
		TEST_REQUIRE(consumer.Snapshot(-1).Elements == StormByte::Size{1});
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{1});
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(consumer.InputEof());
		return 0;
	}

	int test_plan_policy_configures_already_wired_inputs() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe consumer;
		producer.To(-1) >> consumer;
		consumer.Buffering(policy, StormByte::Size{1});
		Pipe::Item::PointerType item = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(producer.Out().TryPush(-1, item, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{7});
		item = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(producer.Out().TryPush(-1, item, ByteSize{7}) == StormByte::Buffer::QueueStatus::WouldBlock);
		TEST_REQUIRE(consumer.Snapshot(-1).PeakElements == StormByte::Size{1});
		consumer.Capacity(-1, StormByte::Size{1});
		consumer.Abort();
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return 0;
	}

	int test_redirected_clone_configures_destination() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe primary;
		Pipe previous;
		Pipe consumer;
		consumer.Buffering(policy);
		producer.To(-1) >> primary;
		producer.CloneTo(-1, previous);
		TEST_REQUIRE(producer.RedirectCloneTo(-1, previous, consumer));
		producer << StormByte::Safe::MakeShared<Packet>();
		consumer.Capacity(-1, StormByte::Size{2});
		TEST_REQUIRE(consumer.Snapshot(-1).Elements == StormByte::Size{1});
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{1});
		TEST_REQUIRE(previous.InputEof());
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(consumer.InputEof());
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return 0;
	}

	int test_shared_writers_keep_budget_and_eof() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe first;
		Pipe second;
		Pipe consumer;
		consumer.Buffering(policy);
		first.To(-1) >> consumer;
		second.To(-1) >> consumer;
		Pipe::Item::PointerType one = StormByte::Safe::MakeShared<Packet>();
		Pipe::Item::PointerType two = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(first.Out().TryPush(-1, one, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		first.Close(nullptr, nullptr);
		TEST_REQUIRE(second.Out().TryPush(-1, two, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{14});
		consumer.Capacity(-1, StormByte::Size{2});
		second.Close(nullptr, nullptr);
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		consumer >> popped;
		TEST_REQUIRE(popped);
		TEST_REQUIRE(consumer.InputEof());
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return 0;
	}

	// -------------------
	// Queue probes
	// -------------------
	std::optional<QueueProbe::Reading> FindReading(const QueueProbe& probe, int key, bool pending = false) {
		for (auto& reading : probe.Read())
			if (reading.Key == key && reading.Pending == pending)
				return reading;
		return std::nullopt;
	}

	int test_probe_distinguishes_absent_empty_unlimited_zero_and_closed() {
		Pipe producer;
		Pipe consumer;
		const auto probe = consumer.Probe();
		TEST_REQUIRE(probe);
		TEST_REQUIRE(probe->Read().empty());
		// A key known only from wiring metadata is absent, not empty.
		probe->Producer(7, producer.Probe()->Id());
		auto absent = FindReading(*probe, 7);
		TEST_REQUIRE(absent && !absent->Snapshot.Exists && !absent->Final);
		producer.To(-1) >> consumer;
		auto empty = FindReading(*probe, -1);
		TEST_REQUIRE(empty && empty->Snapshot.Exists);
		TEST_REQUIRE(empty->Snapshot.Elements == StormByte::Size{0});
		TEST_REQUIRE(!empty->Snapshot.Closed);
		TEST_REQUIRE(empty->Producers.size() == 1 && empty->Producers.front() == producer.Probe()->Id());
		consumer.Capacity(-1, StormByte::Size{0});
		auto unlimited = FindReading(*probe, -1);
		TEST_REQUIRE(!unlimited->Snapshot.Limits.ElementsLimited());
		TEST_REQUIRE(consumer.In().Limits(-1, StormByte::Buffer::QueueLimits{StormByte::Size{0}})
			== StormByte::Buffer::QueueStatus::Ok);
		auto zero = FindReading(*probe, -1);
		TEST_REQUIRE(zero->Snapshot.Limits.ElementsLimited());
		TEST_REQUIRE(zero->Snapshot.Limits.Elements() == StormByte::Size{0});
		TEST_REQUIRE(zero->Id == empty->Id);
		consumer.Capacity(-1, StormByte::Size{2});
		producer << StormByte::Safe::MakeShared<Packet>();
		auto filled = FindReading(*probe, -1);
		TEST_REQUIRE(filled->Snapshot.Elements == StormByte::Size{1});
		TEST_REQUIRE(filled->Snapshot.PeakElements == StormByte::Size{1});
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(FindReading(*probe, -1)->Snapshot.Closed);
		return 0;
	}

	int test_probe_keeps_final_snapshot_after_pipe_destruction() {
		StormByte::Safe::Shared<QueueProbe> probe;
		std::uint64_t id = 0;
		{
			Pipe producer;
			Pipe consumer;
			producer.To(-1) >> consumer;
			consumer.Capacity(-1, StormByte::Size{3});
			producer << StormByte::Safe::MakeShared<Packet>();
			producer << StormByte::Safe::MakeShared<Packet>();
			probe = consumer.Probe();
			id = FindReading(*probe, -1)->Id;
			TEST_REQUIRE(!FindReading(*probe, -1)->Final);
		}
		const auto final = FindReading(*probe, -1);
		TEST_REQUIRE(final && final->Final);
		TEST_REQUIRE(final->Id == id);
		TEST_REQUIRE(final->Snapshot.Exists);
		TEST_REQUIRE(final->Snapshot.PeakElements == StormByte::Size{2});
		TEST_REQUIRE(final->Snapshot.Limits.Elements() == StormByte::Size{3});
		return 0;
	}

	int test_probe_budget_identity_deduplicates_shared_budgets() {
		StormByte::Buffer::ByteBudget shared{ByteSize{64}};
		StormByte::Buffer::ByteBudget other{ByteSize{64}};
		StormByte::Buffer::ByteBudget unlimited;
		StormByte::Buffer::ByteBudget zero{ByteSize{0}};
		BufferingPolicy sharedPolicy;
		sharedPolicy.Budget(shared);
		BufferingPolicy otherPolicy;
		otherPolicy.Budget(other);
		BufferingPolicy unlimitedPolicy;
		unlimitedPolicy.Budget(unlimited);
		BufferingPolicy zeroPolicy;
		zeroPolicy.Budget(zero);
		Pipe source;
		Pipe a;
		Pipe b;
		Pipe c;
		Pipe d;
		Pipe e;
		a.Buffering(sharedPolicy, StormByte::Size{2});
		b.Buffering(sharedPolicy, StormByte::Size{2});
		c.Buffering(otherPolicy, StormByte::Size{2});
		d.Buffering(unlimitedPolicy, StormByte::Size{2});
		e.Buffering(zeroPolicy, StormByte::Size{2});
		source.To(0) >> a;
		source.To(1) >> b;
		source.To(2) >> c;
		source.To(3) >> d;
		source.To(4) >> e;
		const auto ra = FindReading(*a.Probe(), 0);
		const auto rb = FindReading(*b.Probe(), 1);
		const auto rc = FindReading(*c.Probe(), 2);
		const auto rd = FindReading(*d.Probe(), 3);
		const auto re = FindReading(*e.Probe(), 4);
		TEST_REQUIRE(ra && rb && rc && rd && re);
		TEST_REQUIRE(ra->Budget && rb->Budget && rc->Budget && rd->Budget && re->Budget);
		TEST_REQUIRE(ra->Budget->Id == rb->Budget->Id);
		TEST_REQUIRE(ra->Budget->Id != rc->Budget->Id);
		TEST_REQUIRE(rd->Budget->Id != ra->Budget->Id && rd->Budget->Id != rc->Budget->Id);
		TEST_REQUIRE(re->Budget->Id != ra->Budget->Id && re->Budget->Id != rd->Budget->Id);
		TEST_REQUIRE(ra->Handle && ra->Handle->Snapshot().Limited);
		TEST_REQUIRE(rd->Handle && !rd->Handle->Snapshot().Limited);
		TEST_REQUIRE(re->Handle && re->Handle->Snapshot().Limited);
		TEST_REQUIRE(re->Handle->Snapshot().Limit == ByteSize{0});
		// Comparing budget identities never changes accounting.
		TEST_REQUIRE(shared.Snapshot().Reserved == ByteSize{0});
		TEST_REQUIRE(shared.Snapshot().Peak == ByteSize{0});
		TEST_REQUIRE(shared.Snapshot().Pressure == 0);
		return 0;
	}

	int test_probe_pending_counters_and_close() {
		Pipe owner;
		const auto probe = owner.Probe();
		probe->PendingOpen(1, StormByte::Size{0});
		probe->PendingOpen(2, StormByte::Size{4});
		probe->PendingSize(2, StormByte::Size{4});
		probe->PendingPressure(2);
		probe->PendingWait(2, 1000);
		probe->PendingSize(2, StormByte::Size{1});
		const auto unlimited = FindReading(*probe, 1, true);
		const auto bounded = FindReading(*probe, 2, true);
		TEST_REQUIRE(unlimited && bounded);
		TEST_REQUIRE(unlimited->Snapshot.Exists && !unlimited->Snapshot.Limits.ElementsLimited());
		TEST_REQUIRE(bounded->Snapshot.Limits.ElementsLimited());
		TEST_REQUIRE(bounded->Snapshot.Limits.Elements() == StormByte::Size{4});
		TEST_REQUIRE(!bounded->Snapshot.Limits.BytesLimited());
		TEST_REQUIRE(bounded->Snapshot.Elements == StormByte::Size{1});
		TEST_REQUIRE(bounded->Snapshot.PeakElements == StormByte::Size{4});
		TEST_REQUIRE(bounded->Snapshot.Pressure == 1);
		TEST_REQUIRE(bounded->Snapshot.Waits == 1);
		TEST_REQUIRE(bounded->Snapshot.WaitNanoseconds == 1000);
		TEST_REQUIRE(!bounded->Snapshot.Closed);
		probe->PendingClose(true);
		const auto closed = FindReading(*probe, 2, true);
		TEST_REQUIRE(closed->Snapshot.Closed);
		TEST_REQUIRE(closed->Snapshot.Elements == StormByte::Size{0});
		TEST_REQUIRE(closed->Id == bounded->Id);
		return 0;
	}
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		// -------------------
		// Activity
		// -------------------
		TestEntry{"test_activity_input_wait_open_interval", test_activity_input_wait_open_interval},
		TestEntry{"test_activity_nested_states_and_generation", test_activity_nested_states_and_generation},
		TestEntry{"test_activity_output_wait_open_interval", test_activity_output_wait_open_interval},
		TestEntry{"test_activity_shared_budget_wait_open_interval", test_activity_shared_budget_wait_open_interval},
		// -------------------
		// Wiring
		// -------------------
		TestEntry{"test_accounting_precedes_first_push", test_accounting_precedes_first_push},
		TestEntry{"test_clone_accounting_precedes_first_push", test_clone_accounting_precedes_first_push},
		TestEntry{"test_plan_policy_configures_already_wired_inputs", test_plan_policy_configures_already_wired_inputs},
		TestEntry{"test_redirected_clone_configures_destination", test_redirected_clone_configures_destination},
		TestEntry{"test_shared_writers_keep_budget_and_eof", test_shared_writers_keep_budget_and_eof},
		TestEntry{"test_probe_distinguishes_absent_empty_unlimited_zero_and_closed",
			test_probe_distinguishes_absent_empty_unlimited_zero_and_closed},
		TestEntry{"test_probe_keeps_final_snapshot_after_pipe_destruction", test_probe_keeps_final_snapshot_after_pipe_destruction},
		TestEntry{"test_probe_budget_identity_deduplicates_shared_budgets", test_probe_budget_identity_deduplicates_shared_budgets},
		TestEntry{"test_probe_pending_counters_and_close", test_probe_pending_counters_and_close}
	};
	return RunSelectedTest(argc, argv, tests);
}
