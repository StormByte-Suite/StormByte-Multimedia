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

#pragma once

#include <StormByte/buffer/byte_budget.hxx>
#include <StormByte/buffer/sink.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/size.hxx>

#include <atomic>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Next process-wide stage, queue or budget topology id.
	 * @return Nonzero id, never reused in the process.
	 */
	STORMBYTE_MULTIMEDIA_PRIVATE std::uint64_t NextTopologyId() noexcept;

	/**
	 * @brief Stable identity of one shared ByteBudget state.
	 *
	 * The zero-byte anchor lease lets later handles of the same budget be
	 * recognised with ByteBudget::Lease::BelongsTo. It reserves no bytes and
	 * keeps only the small budget state alive, never a pipeline stage.
	 */
	struct STORMBYTE_MULTIMEDIA_PRIVATE BudgetIdentity {
		std::uint64_t Id = 0;							///< Topology budget id.
		StormByte::Buffer::ByteBudget::Lease Anchor;	///< Empty when the budget refused a zero-byte anchor.

		/**
		 * @brief Whether sharing can be proven through @ref Anchor.
		 * @return False for a budget closed before any anchor existed.
		 */
		bool Identified() const noexcept { return static_cast<bool>(Anchor); }
	};

	/**
	 * @brief Resolve the shared identity of @p budget.
	 * @param budget Budget handle.
	 * @return Existing identity when a live anchor matches. Otherwise a new
	 *         identity, anchored only if @p budget is still open; a closed
	 *         budget yields an unidentified one instead of a false match.
	 * @throw Safe::AllocationError or std::bad_alloc, including when the anchor
	 *        lease of an open budget cannot be allocated.
	 */
	STORMBYTE_MULTIMEDIA_PRIVATE StormByte::Safe::Shared<BudgetIdentity>
		IdentifyBudget(const StormByte::Buffer::ByteBudget& budget);

	/**
	 * @class QueueProbe
	 * @brief Thread-safe telemetry view of one Pipe's input queues.
	 *
	 * Owned through Safe::Shared by the Pipe and by job topologies. The
	 * borrowed sink pointer is cleared by @ref Detach under the probe mutex
	 * before the sink dies, after final snapshots were captured, so readers
	 * never touch a destroyed sink and never keep the stage alive.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE QueueProbe {
		public:
			using ItemSink = StormByte::Buffer::Sink<StormByte::Multimedia::Pipeline::Item::PointerType>;

			/**
			 * @brief One queue reading.
			 */
			struct Reading {
				int Key = 0;													///< Sink key or pending track.
				std::uint64_t Id = 0;											///< Queue id.
				bool Pending = false;											///< Demux pending list instead of a Sink hopper.
				bool Final = false;												///< Captured at detach.
				std::vector<std::uint64_t> Producers;							///< Producer probe ids.
				StormByte::Safe::Shared<BudgetIdentity> Budget;					///< Budget identity, if configured.
				std::optional<StormByte::Buffer::ByteBudget> Handle;			///< Budget handle for live snapshots.
				StormByte::Buffer::QueueSnapshot Snapshot;						///< Queue accounting.
			};

			/**
			 * @brief Observe @p sink until @ref Detach.
			 * @param sink Borrowed input sink.
			 * @throw Safe::AllocationError or std::bad_alloc.
			 */
			explicit QueueProbe(const ItemSink& sink);

			QueueProbe(const QueueProbe&) = delete;
			QueueProbe& operator=(const QueueProbe&) = delete;

			/**
			 * @brief Stage id of the owning pipe.
			 * @return Process-unique id.
			 */
			std::uint64_t Id() const noexcept { return m_id; }

			/**
			 * @brief Capture final snapshots and stop reading the sink.
			 */
			void Detach() noexcept;

			/**
			 * @brief Record that probe @p producer writes into @p key.
			 * @param key Sink key.
			 * @param producer Producer probe id.
			 */
			void Producer(int key, std::uint64_t producer) noexcept;

			/**
			 * @brief Forget a producer link moved elsewhere.
			 * @param key Sink key.
			 * @param producer Producer probe id.
			 */
			void RemoveProducer(int key, std::uint64_t producer) noexcept;

			/**
			 * @brief Record the budget configured for @p key.
			 * @param key Sink key.
			 * @param budget Budget charged by the hopper.
			 */
			void Budget(int key, const StormByte::Buffer::ByteBudget& budget) noexcept;

			/**
			 * @brief Register a pending list for @p track.
			 * @param track Source track.
			 * @param limit Element cap; zero means unlimited.
			 */
			void PendingOpen(int track, StormByte::Size limit) noexcept;

			/**
			 * @brief Publish pending occupancy after a push or pop.
			 * @param track Source track.
			 * @param size Current element count.
			 */
			void PendingSize(int track, StormByte::Size size) noexcept;

			/**
			 * @brief Record one pending admission that met a full list.
			 * @param track Source track.
			 */
			void PendingPressure(int track) noexcept;

			/**
			 * @brief Record one completed pending admission wait.
			 * @param track Source track.
			 * @param nanoseconds Wait duration.
			 */
			void PendingWait(int track, std::int64_t nanoseconds) noexcept;

			/**
			 * @brief Mark all pending lists closed.
			 * @param cleared Whether queued entries were discarded.
			 */
			void PendingClose(bool cleared) noexcept;

			/**
			 * @brief Mark all pending lists open again for a new feed.
			 */
			void PendingReopen() noexcept;

			/**
			 * @brief Whether bookkeeping was lost to an allocation failure.
			 * @return True if a producer link, budget, pending list or final key may be missing.
			 */
			bool Incomplete() const noexcept { return m_incomplete.load(std::memory_order_acquire); }

			/**
			 * @brief Read every known queue.
			 * @return Owned readings.
			 * @throw Safe::AllocationError or std::bad_alloc.
			 */
			std::vector<Reading> Read() const;

		private:
			struct KeyState {
				std::uint64_t Id = 0;
				std::vector<std::uint64_t> Producers;
				StormByte::Safe::Shared<BudgetIdentity> Budget;
				std::optional<StormByte::Buffer::ByteBudget> Handle;
				std::optional<StormByte::Buffer::QueueSnapshot> Final;
			};

			struct PendingState {
				std::uint64_t Id = 0;
				StormByte::Buffer::QueueSnapshot Snapshot;
			};

			KeyState& Key(int key) const;

			mutable StormByte::Safe::Mutex m_mutex;			///< Guards every field below.
			const std::uint64_t m_id;						///< Stage id.
			const ItemSink* m_sink;							///< Borrowed sink; null after Detach.
			mutable std::map<int, KeyState> m_keys;			///< Known sink keys.
			std::map<int, PendingState> m_pending;			///< Demux pending lists.
			std::atomic<bool> m_incomplete{false};			///< Set when bookkeeping could not allocate.
	};
}
