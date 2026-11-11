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

#include <StormByte/multimedia/backend/pipeline/topology.hxx>
#include <StormByte/safe/unique_lock.hxx>

#include <algorithm>
#include <map>
#include <new>
#include <utility>

namespace StormByte::Multimedia::Backend::Pipeline {
	namespace {
		using namespace StormByte::Multimedia::Pipeline;

		struct BudgetEntry {
			std::optional<StormByte::Buffer::ByteBudget> Handle;
			std::vector<std::uint64_t> Queues;
		};
	}

	void Topology::Reset() noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		m_stages.clear();
		m_budgets.clear();
		m_final.reset();
		++m_run;
		m_available = false;
	}

	void Topology::Register(TopologyDescriptor stage) {
		if (!stage.Probe)
			return;
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (m_final)
			return;
		const auto id = stage.Probe->Id();
		for (const auto& existing : m_stages) {
			if (existing.Probe && existing.Probe->Id() == id)
				return;
		}
		m_stages.push_back(std::move(stage));
		m_available = true;
	}

	void Topology::Freeze() noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (m_final || !m_available)
			return;
		try {
			// Keep budget identities so later runs sharing the budget reuse the id.
			std::vector<StormByte::Safe::Shared<BudgetIdentity>> identities;
			for (const auto& stage : m_stages) {
				for (const auto& reading : stage.Probe->Read()) {
					if (reading.Budget && std::find(identities.begin(), identities.end(), reading.Budget) == identities.end())
						identities.push_back(reading.Budget);
				}
			}
			m_final = Build(true);
			m_budgets = std::move(identities);
			m_stages.clear();
		} catch (const StormByte::Safe::AllocationError&) {
			// Live probes still report final values after their stages detach.
		} catch (const std::bad_alloc&) {
		}
	}

	TopologySnapshot Topology::Snapshot() const {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (m_final)
			return *m_final;
		return Build(false);
	}

	TopologySnapshot Topology::Build(bool final) const {
		const auto state = final ? TopologySnapshotState::Final : TopologySnapshotState::Live;
		const std::size_t count = m_stages.size();
		std::vector<TopologyStage> stages;
		std::vector<std::vector<QueueProbe::Reading>> readings;
		std::map<std::uint64_t, std::size_t> index;
		stages.reserve(count);
		readings.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			const auto& source = m_stages[i];
			TopologyStage stage;
			stage.Id = source.Probe->Id();
			stage.Kind = source.Kind;
			stage.Filter = source.Filter;
			stage.Track = source.Track;
			stage.OutputTrack = source.OutputTrack;
			stage.Media = source.Media;
			stage.FilterPosition = source.FilterPosition;
			stage.FilterName = source.FilterName;
			stage.Name = source.Name;
			stage.Sequence = static_cast<std::uint32_t>(i);
			stage.Metrics = source.Metrics;
			index.emplace(stage.Id, i);
			stages.push_back(std::move(stage));
			readings.push_back(source.Probe->Read());
		}

		std::vector<TopologyQueue> queues;
		std::map<std::uint64_t, BudgetEntry> budgets;
		std::vector<std::pair<std::size_t, std::size_t>> edges;
		for (std::size_t i = 0; i < count; ++i) {
			for (auto& reading : readings[i]) {
				TopologyQueue queue;
				queue.Id = reading.Id;
				queue.Kind = reading.Pending ? TopologyQueueKind::DemuxPending : TopologyQueueKind::Pipe;
				queue.Key = reading.Key;
				queue.Consumer = stages[i].Id;
				queue.BytesAccounted = !reading.Pending;
				queue.State = final || reading.Final ? TopologySnapshotState::Final : TopologySnapshotState::Live;
				queue.Media = stages[i].Media;
				queue.Snapshot = reading.Snapshot;
				for (const auto producer : reading.Producers) {
					const auto found = index.find(producer);
					if (found == index.end() || found->second == i)
						continue;
					queue.Producers.push_back(producer);
					edges.emplace_back(found->second, i);
					stages[found->second].Queues.push_back(TopologyQueueRef{queue.Id, TopologyQueueRole::Output});
					if (!queue.Media)
						queue.Media = stages[found->second].Media;
				}
				if (reading.Budget) {
					queue.Budget = reading.Budget->Id;
					auto& entry = budgets[reading.Budget->Id];
					if (!entry.Handle && reading.Handle)
						entry.Handle = std::move(reading.Handle);
					entry.Queues.push_back(queue.Id);
				}
				stages[i].Queues.push_back(TopologyQueueRef{queue.Id,
					reading.Pending ? TopologyQueueRole::Pending : TopologyQueueRole::Input});
				queues.push_back(std::move(queue));
			}
		}

		// Longest producer path; bounded so a malformed cycle cannot spin.
		std::vector<std::uint32_t> order(count, 0);
		for (std::size_t round = 0; round < count; ++round) {
			bool changed = false;
			for (const auto& [producer, consumer] : edges) {
				if (order[consumer] < order[producer] + 1) {
					order[consumer] = order[producer] + 1;
					changed = true;
				}
			}
			if (!changed)
				break;
		}
		for (std::size_t i = 0; i < count; ++i)
			stages[i].Order = order[i];
		std::stable_sort(stages.begin(), stages.end(), [](const TopologyStage& left, const TopologyStage& right) {
			return left.Order < right.Order;
		});

		TopologySnapshot out;
		out.Available = m_available;
		out.State = state;
		out.Run = m_run;
		out.Stages.reserve(stages.size());
		for (auto& stage : stages)
			out.Stages.push_back(std::move(stage));
		out.Queues.reserve(queues.size());
		for (auto& queue : queues)
			out.Queues.push_back(std::move(queue));
		out.Budgets.reserve(budgets.size());
		for (auto& [id, entry] : budgets) {
			TopologyBudget budget;
			budget.Id = id;
			budget.State = state;
			for (const auto queue : entry.Queues)
				budget.Queues.push_back(queue);
			if (entry.Handle)
				budget.Snapshot = entry.Handle->Snapshot();
			out.Budgets.push_back(std::move(budget));
		}
		return out;
	}
}
