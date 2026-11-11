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

#include <StormByte/multimedia/pipeline/buffering.hxx>
#include <StormByte/multimedia/pipeline/filters/analytics/frames.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <set>
#include <thread>
#include <vector>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using namespace std::chrono_literals;

namespace {
	constexpr StormByte::ByteSize SharedBudgetBytes{std::uint64_t{256} * 1024 * 1024};

	class PacedAudio final: public Filter::Process {
		public:
			explicit PacedAudio(unsigned failAfter = 0):
				Filter::Process({}, "topology-paced-audio"), m_fail_after(failAfter) {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Audio;
			}

			void Clean() noexcept override {}

			void Setup() noexcept override {}

			void Process(const Frame&) noexcept override {
				std::this_thread::sleep_for(2ms);
				if (m_fail_after != 0 && ++m_seen >= m_fail_after)
					Fail("topology test failure");
			}

		private:
			unsigned m_fail_after;
			unsigned m_seen = 0;
	};

	struct AudioGate {
		std::atomic<bool> Entered{false};
		std::atomic<bool> Release{false};
	};

	class GatedAudio final: public Filter::Process {
		public:
			explicit GatedAudio(AudioGate& gate): Filter::Process({}, "activity-gated-audio"), m_gate(gate) {}
			GatedAudio(const GatedAudio&) = delete;
			GatedAudio(GatedAudio&&) noexcept = delete;
			~GatedAudio() noexcept override = default;
			GatedAudio& operator=(const GatedAudio&) = delete;
			GatedAudio& operator=(GatedAudio&&) noexcept = delete;

			StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Audio; }
			void Clean() noexcept override {}
			void Setup() noexcept override {}
			void Process(const Frame&) noexcept override {
				m_gate.Entered.store(true);
				while (!m_gate.Release.load())
					std::this_thread::sleep_for(1ms);
			}

		private:
			AudioGate& m_gate; ///< Test-controlled long Process call.
	};

	struct ReleaseAudio {
		AudioGate& Gate;
		~ReleaseAudio() noexcept { Gate.Release.store(true); }
	};

	bool IsTerminalStatus(Status status) noexcept {
		return status == Status::Done || status == Status::Error || status == Status::Aborted
			|| status == Status::Stopped;
	}

	template<typename T>
	const T* FindId(const StormByte::Safe::Vector<T>& items, std::uint64_t id) noexcept {
		for (const auto& item : items)
			if (item.Id == id)
				return &item;
		return nullptr;
	}

	// Structural invariants that must hold for every live or final snapshot.
	bool Consistent(const TopologySnapshot& snapshot) {
		if (!snapshot.Available)
			return snapshot.Stages.empty() && snapshot.Queues.empty() && snapshot.Budgets.empty();
		std::set<std::uint64_t> ids;
		for (const auto& stage : snapshot.Stages) {
			if (!ids.insert(stage.Id).second || !stage.Metrics)
				return false;
			if (!ActivityConserved(stage.Activity))
				return false;
			if ((stage.Kind == TopologyStageKind::Filter) != (stage.Filter != TopologyFilterKind::None))
				return false;
			for (const auto& ref : stage.Queues) {
				const auto* queue = FindId(snapshot.Queues, ref.Queue);
				if (!queue)
					return false;
				const bool producer = std::find(queue->Producers.begin(), queue->Producers.end(), stage.Id)
					!= queue->Producers.end();
				if (ref.Role == TopologyQueueRole::Input && queue->Consumer != stage.Id)
					return false;
				if (ref.Role == TopologyQueueRole::Output && !producer)
					return false;
				if (ref.Role == TopologyQueueRole::Pending
					&& (queue->Kind != TopologyQueueKind::DemuxPending || queue->Consumer != stage.Id))
					return false;
			}
		}
		for (std::size_t i = 1; i < snapshot.Stages.size(); ++i)
			if (snapshot.Stages[i - 1].Order > snapshot.Stages[i].Order)
				return false;
		for (const auto& queue : snapshot.Queues) {
			if (!ids.insert(queue.Id).second)
				return false;
			if (queue.State != snapshot.State && snapshot.State == TopologySnapshotState::Final)
				return false;
			if (queue.Consumer && !FindId(snapshot.Stages, *queue.Consumer))
				return false;
			for (const auto producer : queue.Producers)
				if (!FindId(snapshot.Stages, producer))
					return false;
			if (queue.Kind == TopologyQueueKind::DemuxPending
				&& (queue.BytesAccounted || queue.Snapshot.Limits.BytesLimited() || queue.Snapshot.Bytes != StormByte::ByteSize{0}))
				return false;
			// Backpressure: a bounded queue never holds more entries than its cap.
			const auto& limits = queue.Snapshot.Limits;
			if (queue.Snapshot.Exists && limits.ElementsLimited() && limits.Elements() > StormByte::Size{0}
				&& queue.Snapshot.Elements > limits.Elements())
				return false;
			if (queue.Budget) {
				const auto* budget = FindId(snapshot.Budgets, *queue.Budget);
				if (!budget || std::find(budget->Queues.begin(), budget->Queues.end(), queue.Id) == budget->Queues.end())
					return false;
			}
		}
		for (const auto& budget : snapshot.Budgets) {
			if (!ids.insert(budget.Id).second)
				return false;
			if (snapshot.State == TopologySnapshotState::Final && budget.State != TopologySnapshotState::Final)
				return false;
			for (const auto queue : budget.Queues) {
				const auto* owner = FindId(snapshot.Queues, queue);
				if (!owner || owner->Budget != budget.Id)
					return false;
			}
		}
		return true;
	}

	class Readers {
		public:
			explicit Readers(StormByte::Safe::Shared<const JobTelemetry> telemetry) {
				for (int i = 0; i < 3; ++i)
					m_threads.emplace_back([this, telemetry]() {
						while (!m_stop.load(std::memory_order_acquire)) {
							const auto snapshot = telemetry->Topology();
							m_reads.fetch_add(1, std::memory_order_relaxed);
							if (snapshot.Available && snapshot.State == TopologySnapshotState::Live)
								m_live.fetch_add(1, std::memory_order_relaxed);
							if (!Consistent(snapshot))
								m_broken.fetch_add(1, std::memory_order_relaxed);
							std::this_thread::yield();
						}
					});
			}

			~Readers() { Stop(); }

			void Stop() {
				m_stop.store(true, std::memory_order_release);
				for (auto& thread : m_threads)
					if (thread.joinable())
						thread.join();
			}

			std::size_t Reads() const noexcept { return m_reads.load(); }
			std::size_t Live() const noexcept { return m_live.load(); }
			std::size_t Broken() const noexcept { return m_broken.load(); }

		private:
			std::atomic<bool> m_stop{false};
			std::atomic<std::size_t> m_reads{0};
			std::atomic<std::size_t> m_live{0};
			std::atomic<std::size_t> m_broken{0};
			std::vector<std::thread> m_threads;
	};

	bool WaitLive(const JobTelemetry& telemetry, Transcoder& job) {
		const auto deadline = std::chrono::steady_clock::now() + 10s;
		while (std::chrono::steady_clock::now() < deadline) {
			const auto snapshot = telemetry.Topology();
			if (snapshot.Available && snapshot.State == TopologySnapshotState::Live && !snapshot.Stages.empty())
				return true;
			if (IsTerminalStatus(job.Status()))
				return false;
			std::this_thread::sleep_for(1ms);
		}
		return false;
	}

	std::size_t CountKind(const TopologySnapshot& snapshot, TopologyStageKind kind) {
		return static_cast<std::size_t>(std::count_if(snapshot.Stages.begin(), snapshot.Stages.end(),
			[kind](const TopologyStage& stage) { return stage.Kind == kind; }));
	}

	const TopologyQueue* RoleQueue(const TopologySnapshot& snapshot, const TopologyStage& stage, TopologyQueueRole role) {
		for (const auto& ref : stage.Queues)
			if (ref.Role == role)
				return FindId(snapshot.Queues, ref.Queue);
		return nullptr;
	}

	BufferingPolicy BoundedPolicy(const StormByte::Buffer::ByteBudget& budget) {
		BufferingPolicy policy;
		policy.Packets(QueueLimit::Bounded(4));
		policy.AudioFrames(QueueLimit::Bounded(4));
		policy.VideoFrames(QueueLimit::Bounded(4));
		policy.DemuxPending(QueueLimit::Bounded(256));
		policy.Budget(budget);
		return policy;
	}

	void Dump(const TopologySnapshot& snapshot) {
		for (const auto& stage : snapshot.Stages)
			std::cerr << "[DETAIL] stage " << stage.Id << " " << TestView(stage.Name) << " kind="
				<< static_cast<int>(stage.Kind) << " order=" << stage.Order << std::endl;
		for (const auto& queue : snapshot.Queues)
			std::cerr << "[DETAIL] queue " << queue.Id << " kind=" << static_cast<int>(queue.Kind) << " key=" << queue.Key
				<< " consumer=" << queue.Consumer.value_or(0) << " exists=" << queue.Snapshot.Exists
				<< " limited=" << queue.Snapshot.Limits.ElementsLimited() << " cap=" << queue.Snapshot.Limits.Elements()
				<< " peak=" << queue.Snapshot.PeakElements << std::endl;
	}

	int CheckFinal(const TopologySnapshot& snapshot) {
		TEST_REQUIRE(snapshot.Available);
		TEST_REQUIRE(snapshot.State == TopologySnapshotState::Final);
		TEST_REQUIRE(snapshot.Complete);
		TEST_REQUIRE(Consistent(snapshot));
		for (const auto& queue : snapshot.Queues)
			TEST_REQUIRE(queue.State == TopologySnapshotState::Final);
		for (const auto& budget : snapshot.Budgets)
			TEST_REQUIRE(budget.State == TopologySnapshotState::Final);
		std::size_t active = 0;
		for (const auto& stage : snapshot.Stages) {
			TEST_REQUIRE(stage.Activity.Complete && stage.Activity.WaitReasons);
			for (const auto& worker : stage.Activity.Workers) {
				if (!worker.Available)
					continue;
				++active;
				TEST_REQUIRE(worker.State == ActivityState::Finished || worker.State == ActivityState::Failed);
			}
		}
		TEST_REQUIRE(active > 0);
		return 0;
	}
}

int test_activity_batch_shared_budget() {
	StormByte::Buffer::ByteBudget budget{StormByte::ByteSize{1024 * 1024}};
	StormByte::Buffer::ByteBudget::Lease held;
	TEST_REQUIRE(budget.TryReserve(StormByte::ByteSize{1024 * 1024}, held) == StormByte::Buffer::QueueStatus::Ok);
	StormByte::Safe::Shared<const JobTelemetry> first_metrics;
	StormByte::Safe::Shared<const JobTelemetry> second_metrics;
	bool both_waiting = false;
	{
		Transcoder first{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/topology/activity-batch-first.mkv")), MakeLogger(), 2000000000LL};
		Transcoder second{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/topology/activity-batch-second.mkv")), MakeLogger(), 2000000000LL};
		for (auto* job : {&first, &second}) {
			job->Buffering(BoundedPolicy(budget));
			job->Video(0).Remux();
			job->Audio(1).Remux();
			TEST_REQUIRE(CheckTranscoderConfigured(*job) == 0);
		}
		first_metrics = first.Telemetry();
		second_metrics = second.Telemetry();
		first.Run();
		second.Run();
		const auto waiting = [](const JobTelemetry& metrics) {
			for (const auto& stage : metrics.Topology().Stages)
				for (const auto& worker : stage.Activity.Workers)
					if (worker.State == ActivityState::WaitingBudget)
						return true;
			return false;
		};
		const auto deadline = std::chrono::steady_clock::now() + 10s;
		while (std::chrono::steady_clock::now() < deadline) {
			if (waiting(*first_metrics) && waiting(*second_metrics)) {
				both_waiting = true;
				break;
			}
			std::this_thread::sleep_for(1ms);
		}
		held.Reset();
		TEST_REQUIRE(both_waiting);
		TEST_REQUIRE(WaitForTranscoder(first) == 0);
		TEST_REQUIRE(WaitForTranscoder(second) == 0);
	}
	const auto first = first_metrics->Topology();
	const auto second = second_metrics->Topology();
	TEST_REQUIRE(CheckFinal(first) == 0 && CheckFinal(second) == 0);
	TEST_REQUIRE(first.Budgets.size() == 1 && second.Budgets.size() == 1);
	TEST_REQUIRE(first.Budgets.front().Id == second.Budgets.front().Id);
	for (const auto* snapshot : {&first, &second}) {
		auto waited = 0ns;
		for (const auto& stage : snapshot->Stages)
			for (const auto& worker : stage.Activity.Workers)
				waited += worker.WaitingBudget;
		TEST_REQUIRE(waited > 0ns);
	}
	TEST_REQUIRE(budget.Snapshot().Reserved == StormByte::ByteSize{0});
	return 0;
}

int test_activity_live_processing_and_pause() {
	const auto codec = Registry::Instance().FindCodec("E-AC3");
	if (!codec || !codec->get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	AudioGate gate;
	Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
		TestLocation(OutputPath("pipeline/topology/activity-open-process.mkv")), MakeLogger(), 2000000000LL};
	ReleaseAudio release{gate};
	job.Video(0).Remux();
	job.Audio(1).Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"eac3"})
		.Filter<GatedAudio>(gate);
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	const auto telemetry = job.Telemetry();
	job.Run();
	const auto deadline = std::chrono::steady_clock::now() + 10s;
	while (!gate.Entered.load() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(1ms);
	TEST_REQUIRE(gate.Entered.load() && WaitLive(*telemetry, job));
	const auto sample = [&]() {
		const auto topology = telemetry->Topology();
		for (const auto& stage : topology.Stages)
			if (stage.Kind == TopologyStageKind::Filter)
				return stage.Activity.Workers.front();
		return WorkerActivitySnapshot{};
	};
	const auto first = sample();
	std::this_thread::sleep_for(30ms);
	const auto second = sample();
	TEST_REQUIRE(first.Available && first.State == ActivityState::Processing);
	TEST_REQUIRE(second.Id == first.Id && second.Generation == first.Generation);
	TEST_REQUIRE(second.Processing - first.Processing >= 30ms);
	job.Pause();
	gate.Release.store(true);
	StormByte::Safe::Optional<WorkerActivitySnapshot> paused;
	const auto pause_deadline = std::chrono::steady_clock::now() + 5s;
	while (!paused && std::chrono::steady_clock::now() < pause_deadline) {
		for (const auto& stage : telemetry->Topology().Stages)
			for (const auto& worker : stage.Activity.Workers)
				if (worker.State == ActivityState::Paused)
					paused = worker;
		std::this_thread::sleep_for(1ms);
	}
	TEST_REQUIRE(paused);
	std::this_thread::sleep_for(30ms);
	bool matched = false;
	for (const auto& stage : telemetry->Topology().Stages) {
		TEST_REQUIRE(ActivityConserved(stage.Activity));
		for (const auto& worker : stage.Activity.Workers)
			if (worker.Id == paused->Id) {
				matched = true;
				TEST_REQUIRE(worker.State == ActivityState::Paused);
				TEST_REQUIRE(worker.Paused - paused->Paused >= 30ms);
				TEST_REQUIRE(worker.Processing == paused->Processing);
			}
	}
	TEST_REQUIRE(matched);
	job.Resume();
	TEST_REQUIRE(WaitForTranscoder(job) == 0);
	return 0;
}

int test_topology_reports_filters_tracks_remux_and_shared_budget() {
	const auto codec = Registry::Instance().FindCodec("E-AC3");
	if (!codec || !codec->get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	auto logger = MakeLogger();
	StormByte::Buffer::ByteBudget shared{SharedBudgetBytes};
	StormByte::Safe::Shared<const JobTelemetry> telemetry;
	std::uint64_t run = 0;
	{
		Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/topology/filters.mkv")), logger, 2008000000LL};
		job.Buffering(BoundedPolicy(shared));
		job.Video(0).Remux().Filter<Filter::Video::CountFrames>(logger);
		job.Audio(1).Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"eac3"})
			.Filter<PacedAudio>().Filter<PacedAudio>();
		job.Filter<Filter::Video::CountFrames>(logger);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		telemetry = job.Telemetry();
		TEST_REQUIRE(telemetry);
		const auto before = telemetry->Topology();
		TEST_REQUIRE(!before.Available && before.Stages.empty() && before.Queues.empty() && before.Budgets.empty());
		Readers readers{telemetry};
		job.Run();
		TEST_REQUIRE(WaitLive(*telemetry, job));
		job.Pause();
		std::this_thread::sleep_for(50ms);
		if (job.Status() == Status::Paused) {
			const auto paused = telemetry->Topology();
			TEST_REQUIRE(paused.Available && paused.State == TopologySnapshotState::Live);
			TEST_REQUIRE(Consistent(paused));
			std::this_thread::sleep_for(20ms);
			job.Resume();
		}
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
		readers.Stop();
		TEST_REQUIRE(readers.Broken() == 0);
		TEST_REQUIRE(readers.Live() > 0);
		std::cerr << "[DETAIL] concurrent topology reads: " << readers.Reads() << std::endl;
		run = telemetry->Topology().Run;
	}
	// The transcoder is gone; the retained handle still answers with the frozen run.
	const auto snapshot = telemetry->Topology();
	Dump(snapshot);
	TEST_REQUIRE(CheckFinal(snapshot) == 0);
	TEST_REQUIRE(snapshot.Run == run);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Demuxer) == 1);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Muxer) == 1);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Remuxer) == 1);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Decoder) == 1);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Encoder) == 1);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::InspectionDecoder) >= 1);
	TEST_REQUIRE(snapshot.Stages.front().Kind == TopologyStageKind::Demuxer);
	TEST_REQUIRE(snapshot.Stages.front().Order == 0);
	TEST_REQUIRE(snapshot.Stages.front().Activity.Workers.size() >= 3);
	for (const auto& worker : snapshot.Stages.front().Activity.Workers)
		if (worker.Role == ActivityRole::DemuxFeeder)
			TEST_REQUIRE(worker.Track >= 0);
	const auto& muxer = *std::find_if(snapshot.Stages.begin(), snapshot.Stages.end(),
		[](const TopologyStage& stage) { return stage.Kind == TopologyStageKind::Muxer; });
	std::map<std::uint32_t, const TopologyStage*> paced;
	std::size_t trackAnalytics = 0;
	std::size_t globalAnalytics = 0;
	for (const auto& stage : snapshot.Stages) {
		if (stage.Kind == TopologyStageKind::Remuxer || stage.Kind == TopologyStageKind::Encoder
			|| stage.Filter == TopologyFilterKind::Process)
			TEST_REQUIRE(stage.Order < muxer.Order);
		if (stage.Kind == TopologyStageKind::Remuxer) {
			TEST_REQUIRE(stage.Track == 0 && stage.OutputTrack == 0);
			TEST_REQUIRE(stage.Media == StormByte::Multimedia::Type::Video);
			TEST_REQUIRE(RoleQueue(snapshot, stage, TopologyQueueRole::Input));
			TEST_REQUIRE(RoleQueue(snapshot, stage, TopologyQueueRole::Output));
		}
		if (stage.Kind == TopologyStageKind::Decoder || stage.Kind == TopologyStageKind::Encoder) {
			TEST_REQUIRE(stage.Track == 1 && stage.OutputTrack == 1);
			TEST_REQUIRE(stage.Media == StormByte::Multimedia::Type::Audio);
		}
		if (stage.Kind != TopologyStageKind::Filter) {
			TEST_REQUIRE(!stage.FilterPosition && stage.FilterName.empty());
			continue;
		}
		if (stage.Filter == TopologyFilterKind::Process) {
			TEST_REQUIRE(TestView(stage.FilterName) == "topology-paced-audio");
			TEST_REQUIRE(stage.Track == 1 && stage.FilterPosition);
			TEST_REQUIRE(stage.Media == StormByte::Multimedia::Type::Audio);
			TEST_REQUIRE(RoleQueue(snapshot, stage, TopologyQueueRole::Input));
			paced[*stage.FilterPosition] = &stage;
		}
		else if (stage.Filter == TopologyFilterKind::Analytics) {
			if (stage.Track) {
				TEST_REQUIRE(stage.Track == 0 && stage.FilterPosition == 0u);
				++trackAnalytics;
			}
			else {
				TEST_REQUIRE(!stage.FilterPosition);
				++globalAnalytics;
			}
		}
	}
	TEST_REQUIRE(paced.size() == 2 && paced.contains(0) && paced.contains(1));
	TEST_REQUIRE(paced[0]->Order < paced[1]->Order);
	TEST_REQUIRE(paced[0]->Id != paced[1]->Id);
	TEST_REQUIRE(trackAnalytics == 1 && globalAnalytics == 1);
	// Registered stages share counters and display names with JobTelemetry::Stages.
	const auto stages = telemetry->Stages();
	for (const auto& stage : snapshot.Stages) {
		if (stage.Kind == TopologyStageKind::InspectionDecoder)
			continue;
		TEST_REQUIRE(std::any_of(stages.begin(), stages.end(), [&stage](const auto& entry) {
			return entry.Metrics.get() == stage.Metrics.get() && TestView(entry.Name) == TestView(stage.Name);
		}));
	}
	// One shared budget appears once, referenced by every queue it charges.
	TEST_REQUIRE(snapshot.Budgets.size() == 1);
	const auto& budget = snapshot.Budgets.front();
	TEST_REQUIRE(budget.Snapshot.Limited && budget.Snapshot.Limit == SharedBudgetBytes);
	TEST_REQUIRE(budget.Queues.size() > 2);
	TEST_REQUIRE(budget.Snapshot.Reserved == shared.Snapshot().Reserved);
	std::size_t bounded = 0;
	std::size_t pending = 0;
	for (const auto& queue : snapshot.Queues) {
		if (queue.Kind == TopologyQueueKind::DemuxPending) {
			++pending;
			TEST_REQUIRE(queue.Snapshot.Limits.ElementsLimited());
			TEST_REQUIRE(queue.Snapshot.Limits.Elements() == StormByte::Size{256});
			TEST_REQUIRE(queue.Consumer == snapshot.Stages.front().Id);
			continue;
		}
		if (queue.Snapshot.Exists && queue.Snapshot.Limits.ElementsLimited()) {
			++bounded;
			TEST_REQUIRE(queue.Snapshot.PeakElements <= queue.Snapshot.Limits.Elements());
		}
	}
	TEST_REQUIRE(pending >= 1);
	TEST_REQUIRE(bounded >= 3);
	return 0;
}

int test_topology_cancel_freezes_and_survives_transcoder() {
	const auto codec = Registry::Instance().FindCodec("E-AC3");
	if (!codec || !codec->get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	auto logger = MakeLogger();
	StormByte::Buffer::ByteBudget shared{SharedBudgetBytes};
	StormByte::Safe::Shared<const JobTelemetry> telemetry;
	std::unique_ptr<Readers> readers;
	{
		Transcoder job{TestLocation(FixturePath("video/interleave_long.mkv")),
			TestLocation(OutputPath("pipeline/topology/cancel.mkv")), logger, 12012000000LL};
		job.Buffering(BoundedPolicy(shared));
		job.Video(0).Remux();
		job.Audio(1).Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"eac3"})
			.Filter<PacedAudio>();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		telemetry = job.Telemetry();
		readers = std::make_unique<Readers>(telemetry);
		job.Run();
		TEST_REQUIRE(WaitLive(*telemetry, job));
		job.Pause();
		std::this_thread::sleep_for(50ms);
		TEST_REQUIRE(job.Status() == Status::Paused);
		const auto paused = telemetry->Topology();
		TEST_REQUIRE(paused.State == TopologySnapshotState::Live && Consistent(paused));
		job.Cancel();
		const auto deadline = std::chrono::steady_clock::now() + 10s;
		while (!IsTerminalStatus(job.Status()) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(5ms);
		TEST_REQUIRE(job.Status() == Status::Aborted);
		// Readers keep running while the transcoder joins and is destroyed.
	}
	std::this_thread::sleep_for(20ms);
	readers->Stop();
	TEST_REQUIRE(readers->Broken() == 0);
	const auto snapshot = telemetry->Topology();
	TEST_REQUIRE(CheckFinal(snapshot) == 0);
	std::this_thread::sleep_for(20ms);
	const auto retained = telemetry->Topology();
	for (std::size_t i = 0; i < snapshot.Stages.size(); ++i) {
		TEST_REQUIRE(snapshot.Stages[i].Activity.Workers.size() == retained.Stages[i].Activity.Workers.size());
		for (std::size_t j = 0; j < snapshot.Stages[i].Activity.Workers.size(); ++j)
			TEST_REQUIRE(snapshot.Stages[i].Activity.Workers[j] == retained.Stages[i].Activity.Workers[j]);
	}
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Filter) == 1);
	TEST_REQUIRE(snapshot.Budgets.size() == 1);
	TEST_REQUIRE(telemetry->Topology().Queues.size() == snapshot.Queues.size());
	return 0;
}

int test_topology_failure_is_final() {
	const auto codec = Registry::Instance().FindCodec("E-AC3");
	if (!codec || !codec->get().HasAccess(Access{Operation::Write}))
		return TEST_SKIP;
	auto logger = MakeLogger();
	StormByte::Safe::Shared<const JobTelemetry> telemetry;
	{
		Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/topology/failure.mka")), logger, 2008000000LL};
		job.Audio(1).Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"eac3"})
			.Filter<PacedAudio>(5u);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		telemetry = job.Telemetry();
		Readers readers{telemetry};
		job.Run();
		TEST_REQUIRE(WaitForTranscoderFailure(job) == 0);
		readers.Stop();
		TEST_REQUIRE(readers.Broken() == 0);
	}
	const auto snapshot = telemetry->Topology();
	TEST_REQUIRE(CheckFinal(snapshot) == 0);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Filter) == 1);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Encoder) == 1);
	return 0;
}

int test_topology_unlimited_queues_and_open_failure() {
	auto logger = MakeLogger();
	StormByte::Safe::Shared<const JobTelemetry> telemetry;
	{
		Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/topology/unlimited.mkv")), logger, 2008000000LL};
		BufferingPolicy policy;
		policy.Packets(QueueLimit::Unlimited());
		policy.AudioFrames(QueueLimit::Unlimited());
		policy.VideoFrames(QueueLimit::Unlimited());
		policy.DemuxPending(QueueLimit::Unlimited());
		job.Buffering(policy);
		job.Video(0).Remux();
		job.Audio(1).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		telemetry = job.Telemetry();
		job.Run();
		TEST_REQUIRE(WaitForTranscoder(job) == 0);
	}
	const auto snapshot = telemetry->Topology();
	Dump(snapshot);
	TEST_REQUIRE(CheckFinal(snapshot) == 0);
	TEST_REQUIRE(CountKind(snapshot, TopologyStageKind::Remuxer) == 2);
	TEST_REQUIRE(snapshot.Budgets.size() == 1);
	TEST_REQUIRE(!snapshot.Budgets.front().Snapshot.Limited);
	std::size_t pending = 0;
	for (const auto& queue : snapshot.Queues) {
		if (!queue.Snapshot.Exists)
			continue;
		// The demuxer still enforces its fallback park ceiling; telemetry reports what is enforced.
		if (queue.Kind == TopologyQueueKind::DemuxPending) {
			++pending;
			continue;
		}
		TEST_REQUIRE(!queue.Snapshot.Limits.ElementsLimited());
	}
	TEST_REQUIRE(pending == 2);

	// A job that fails before any stage exists freezes an empty, still available run.
	StormByte::Safe::Shared<const JobTelemetry> failed;
	{
		Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath("pipeline/topology/missing-dir/nested/out.mkv")), logger, 2008000000LL};
		job.Video(0).Remux();
		failed = job.Telemetry();
		if (job.Status() == Status::Error)
			return 0;
		job.Run();
		const auto deadline = std::chrono::steady_clock::now() + 10s;
		while (!IsTerminalStatus(job.Status()) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(5ms);
	}
	const auto empty = failed->Topology();
	TEST_REQUIRE(Consistent(empty));
	if (empty.Available)
		TEST_REQUIRE(empty.State == TopologySnapshotState::Final);
	return 0;
}

int test_topology_closed_budget_identity() {
	auto logger = MakeLogger();
	StormByte::Buffer::ByteBudget budget{SharedBudgetBytes};
	BufferingPolicy policy;
	policy.Budget(budget);
	const auto runJob = [&](const char* name, const BufferingPolicy& buffering) {
		StormByte::Safe::Shared<const JobTelemetry> telemetry;
		Transcoder job{TestLocation(FixturePath("video/anime_like.mkv")),
			TestLocation(OutputPath(name)), logger, 2008000000LL};
		job.Buffering(buffering);
		job.Video(0).Remux();
		job.Audio(1).Remux();
		if (CheckTranscoderConfigured(job) != 0)
			return telemetry;
		telemetry = job.Telemetry();
		job.Run();
		const auto deadline = std::chrono::steady_clock::now() + 30s;
		while (!IsTerminalStatus(job.Status()) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(5ms);
		return telemetry;
	};

	const auto first = runJob("pipeline/topology/budget-open.mkv", policy);
	TEST_REQUIRE(first);
	const auto open = first->Topology();
	TEST_REQUIRE(CheckFinal(open) == 0 && open.Complete);
	TEST_REQUIRE(open.Budgets.size() == 1);
	const auto id = open.Budgets.front().Id;

	// Closed after an earlier sighting: still the same, proven budget.
	budget.Close();
	const auto second = runJob("pipeline/topology/budget-closed.mkv", policy);
	TEST_REQUIRE(second);
	const auto closed = second->Topology();
	TEST_REQUIRE(Consistent(closed) && closed.Complete);
	TEST_REQUIRE(closed.Available && !closed.Budgets.empty());
	for (const auto& entry : closed.Budgets)
		TEST_REQUIRE(entry.Id == id && entry.Snapshot.Closed);

	// Closed before any sighting: still deduplicated and distinct from other budgets.
	StormByte::Buffer::ByteBudget unseen{SharedBudgetBytes};
	unseen.Close();
	BufferingPolicy unseenPolicy;
	unseenPolicy.Budget(unseen);
	const auto third = runJob("pipeline/topology/budget-unseen.mkv", unseenPolicy);
	TEST_REQUIRE(third);
	const auto anonymous = third->Topology();
	TEST_REQUIRE(Consistent(anonymous) && anonymous.Complete);
	TEST_REQUIRE(anonymous.Available && anonymous.Budgets.size() == 1);
	for (const auto& entry : anonymous.Budgets)
		TEST_REQUIRE(!entry.Queues.empty() && entry.Id != id && entry.Snapshot.Closed);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_activity_batch_shared_budget", test_activity_batch_shared_budget},
		TestEntry{"test_activity_live_processing_and_pause", test_activity_live_processing_and_pause},
		TestEntry{"test_topology_reports_filters_tracks_remux_and_shared_budget",
			test_topology_reports_filters_tracks_remux_and_shared_budget},
		TestEntry{"test_topology_cancel_freezes_and_survives_transcoder", test_topology_cancel_freezes_and_survives_transcoder},
		TestEntry{"test_topology_failure_is_final", test_topology_failure_is_final},
		TestEntry{"test_topology_unlimited_queues_and_open_failure", test_topology_unlimited_queues_and_open_failure},
		TestEntry{"test_topology_closed_budget_identity", test_topology_closed_budget_identity}
	};
	return RunSelectedTest(argc, argv, tests);
}
