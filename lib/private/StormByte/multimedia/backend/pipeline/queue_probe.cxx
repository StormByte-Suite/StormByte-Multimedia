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

#include <StormByte/multimedia/backend/pipeline/queue_probe.hxx>
#include <StormByte/safe/unique_lock.hxx>

#include <algorithm>
#include <atomic>
#include <new>

namespace StormByte::Multimedia::Backend::Pipeline {
	namespace {
		struct BudgetRegistry {
			StormByte::Safe::Mutex Mutex;
			std::vector<StormByte::Safe::Weak<BudgetIdentity>> Entries;
		};

		BudgetRegistry& Registry() noexcept {
			static BudgetRegistry registry;
			return registry;
		}
	}

	std::uint64_t NextTopologyId() noexcept {
		static std::atomic<std::uint64_t> next{1};
		return next.fetch_add(1, std::memory_order_relaxed);
	}

	StormByte::Safe::Shared<BudgetIdentity> IdentifyBudget(const StormByte::Buffer::ByteBudget& budget) {
		auto& registry = Registry();
		StormByte::Safe::UniqueLock lock(registry.Mutex);
		bool expired = false;
		for (const auto& entry : registry.Entries) {
			auto identity = entry.lock();
			if (!identity) {
				expired = true;
				continue;
			}
			if (identity->Anchor.BelongsTo(budget))
				return identity;
		}
		if (expired)
			std::erase_if(registry.Entries, [](const auto& entry) { return !entry.lock(); });
		auto identity = StormByte::Safe::Shared<BudgetIdentity>::MakePointer<BudgetIdentity>();
		identity->Id = NextTopologyId();
		// A zero-byte charge never changes Reserved, Peak, Pressure or Waits.
		if (budget.TryReserve(StormByte::ByteSize{0}, identity->Anchor) == StormByte::Buffer::QueueStatus::Ok)
			registry.Entries.push_back(StormByte::Safe::Weak<BudgetIdentity>{identity});
		return identity;
	}

	QueueProbe::QueueProbe(const ItemSink& sink)
	:	m_id(NextTopologyId()), m_sink(&sink) {}

	QueueProbe::KeyState& QueueProbe::Key(int key) const {
		auto found = m_keys.find(key);
		if (found == m_keys.end()) {
			found = m_keys.emplace(key, KeyState{}).first;
			found->second.Id = NextTopologyId();
		}
		return found->second;
	}

	void QueueProbe::Detach() noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (!m_sink)
			return;
		try {
			for (const int key : m_sink->Keys())
				Key(key);
		} catch (const StormByte::Safe::AllocationError&) {
		} catch (const std::bad_alloc&) {
		}
		for (auto& [key, state] : m_keys)
			state.Final = m_sink->Snapshot(key);
		m_sink = nullptr;
	}

	void QueueProbe::Producer(int key, std::uint64_t producer) noexcept try {
		StormByte::Safe::UniqueLock lock(m_mutex);
		auto& state = Key(key);
		if (std::find(state.Producers.begin(), state.Producers.end(), producer) == state.Producers.end())
			state.Producers.push_back(producer);
	} catch (const StormByte::Safe::AllocationError&) {
	} catch (const std::bad_alloc&) {
	}

	void QueueProbe::RemoveProducer(int key, std::uint64_t producer) noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (const auto found = m_keys.find(key); found != m_keys.end())
			std::erase(found->second.Producers, producer);
	}

	void QueueProbe::Budget(int key, const StormByte::Buffer::ByteBudget& budget) noexcept try {
		auto identity = IdentifyBudget(budget);
		StormByte::Safe::UniqueLock lock(m_mutex);
		auto& state = Key(key);
		state.Budget = std::move(identity);
		state.Handle = budget;
	} catch (const StormByte::Safe::AllocationError&) {
	} catch (const std::bad_alloc&) {
	}

	void QueueProbe::PendingOpen(int track, StormByte::Size limit) noexcept try {
		StormByte::Safe::UniqueLock lock(m_mutex);
		auto found = m_pending.find(track);
		if (found == m_pending.end()) {
			found = m_pending.emplace(track, PendingState{}).first;
			found->second.Id = NextTopologyId();
			found->second.Snapshot.Exists = true;
		}
		StormByte::Buffer::QueueLimits limits;
		if (limit > StormByte::Size{0})
			limits.Elements(limit);
		found->second.Snapshot.Limits = limits;
	} catch (const StormByte::Safe::AllocationError&) {
	} catch (const std::bad_alloc&) {
	}

	void QueueProbe::PendingSize(int track, StormByte::Size size) noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (const auto found = m_pending.find(track); found != m_pending.end()) {
			auto& snapshot = found->second.Snapshot;
			snapshot.Elements = size;
			snapshot.PeakElements = std::max(snapshot.PeakElements, size);
		}
	}

	void QueueProbe::PendingPressure(int track) noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (const auto found = m_pending.find(track); found != m_pending.end())
			++found->second.Snapshot.Pressure;
	}

	void QueueProbe::PendingWait(int track, std::int64_t nanoseconds) noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (const auto found = m_pending.find(track); found != m_pending.end()) {
			++found->second.Snapshot.Waits;
			found->second.Snapshot.WaitNanoseconds += nanoseconds;
		}
	}

	void QueueProbe::PendingClose(bool cleared) noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		for (auto& [track, state] : m_pending) {
			state.Snapshot.Closed = true;
			if (cleared)
				state.Snapshot.Elements = StormByte::Size{0};
		}
	}

	void QueueProbe::PendingReopen() noexcept {
		StormByte::Safe::UniqueLock lock(m_mutex);
		for (auto& [track, state] : m_pending)
			state.Snapshot.Closed = false;
	}

	std::vector<QueueProbe::Reading> QueueProbe::Read() const {
		StormByte::Safe::UniqueLock lock(m_mutex);
		if (m_sink) {
			for (const int key : m_sink->Keys())
				Key(key);
		}
		std::vector<Reading> out;
		out.reserve(m_keys.size() + m_pending.size());
		for (const auto& [key, state] : m_keys) {
			Reading reading;
			reading.Key = key;
			reading.Id = state.Id;
			reading.Producers = state.Producers;
			reading.Budget = state.Budget;
			reading.Handle = state.Handle;
			if (m_sink)
				reading.Snapshot = m_sink->Snapshot(key);
			else {
				reading.Final = true;
				if (state.Final)
					reading.Snapshot = *state.Final;
			}
			out.push_back(std::move(reading));
		}
		for (const auto& [track, state] : m_pending) {
			Reading reading;
			reading.Key = track;
			reading.Id = state.Id;
			reading.Pending = true;
			reading.Final = !m_sink;
			reading.Snapshot = state.Snapshot;
			out.push_back(std::move(reading));
		}
		return out;
	}
}
