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

#include <StormByte/multimedia/backend/pipeline/worker.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/condition_variable.hxx>
#include <StormByte/safe/list.hxx>
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/thread.hxx>
#include <StormByte/safe/unique_lock.hxx>
#include <StormByte/safe/vector.hxx>

#include <cstddef>

namespace StormByte::Multimedia::Pipeline {
	class Demuxer;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker
 * @brief Concrete stage bodies.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	class Demux;

	/**
	 * @class FeedThread
	 * @brief Base-owned per-track feeder joined before its Demux owner dies.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE FeedThread final {
		public:
			/**
			 * @brief Starts the feeder for @p track.
			 * @param owner Demux whose lifetime extends through Join.
			 * @param track Origin stream index.
			 */
			FeedThread(Demux& owner, int track);

			FeedThread(const FeedThread&) = delete;

			FeedThread(FeedThread&&) noexcept = delete;

			/**
			 * @brief Joins the execution before releasing this handle.
			 */
			~FeedThread() noexcept;

			FeedThread& operator=(const FeedThread&) = delete;

			FeedThread& operator=(FeedThread&&) noexcept = delete;

			/**
			 * @brief Track assigned to this feeder.
			 * @return Origin stream index.
			 */
			int Track() const noexcept;

			/**
			 * @brief Joins the feeder if it is running.
			 */
			void Join() noexcept;

		private:
			StormByte::Safe::Thread m_thread;	///< Base-owned execution.
			int m_track;	///< Origin stream index.
	};
	}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Backend::Pipeline::Detail::Worker::FeedThread);

	/**
	 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker
	 * @brief Concrete stage bodies.
	 *
	 * @ingroup multimedia_pipeline
	 */
	namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	/**
	 * @class Demux
	 * @brief Reads interleaved packets from the Plan origin.
	 *
	 * Source pumper. Each Process is one Read. Empty Read is source
	 * EoF: @ref Ended only when not measuring. A measure EoF parks
	 * until Rewind. Flush is unused.
	 *
	 * Read never Emits. A per-track feeder thread Pushes parked
	 * packets (that call may block). Read waits only if that track's
	 * park hits @ref ParkCeiling.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Demux final: public StormByte::Multimedia::Backend::Pipeline::Worker {
		public:
			/**
			 * @brief Demux body bound to @p owner.
			 * @param owner Public demuxer (also the Host).
			 */
			explicit Demux(StormByte::Multimedia::Pipeline::Demuxer& owner) noexcept;

			Demux(const Demux&) = delete;
			Demux(Demux&&) noexcept = delete;
			~Demux() noexcept override;
			Demux& operator=(const Demux&) = delete;
			Demux& operator=(Demux&&) noexcept = delete;

			/**
			 * @brief Waits for a Plan, checks it and opens the origin.
			 */
			void Setup() noexcept override;

			/**
			 * @brief One origin Read. Empty item is a source tick.
			 * @param item Ignored.
			 */
			void Process(StormByte::Multimedia::Pipeline::Item::PointerType item) noexcept override;

		private:
			friend class FeedThread;

			void Flush() noexcept override;

			/**
			 * @brief Starts a feeder for @p track if missing.
			 * @param track Origin stream index.
			 */
			void EnsureFeed(int track) noexcept;

			/**
			 * @brief Blocking Emit of parked packets for one track.
			 * @param track Origin stream index.
			 */
			void FeedTrack(int track) noexcept;

			/**
			 * @brief Stops every feeder and joins.
			 */
			void StopFeed() noexcept;

			/**
			 * @brief Whether any park still holds a packet.
			 * @return true if a feeder still has work.
			 */
			bool ParkPending() const noexcept;

			static constexpr std::size_t ParkCeiling = 2048;	///< Per-track compressed park

			StormByte::Multimedia::Pipeline::Demuxer& m_owner;	///< Public demuxer
			StormByte::Safe::Map<int, StormByte::Safe::List<StormByte::Multimedia::Pipeline::Packet::PointerType>> m_park;
			StormByte::Safe::Vector<StormByte::Safe::Shared<FeedThread>> m_feeds;	///< One Base-owned feeder per track.
			mutable StormByte::Safe::Mutex m_parkMutex;	///< Guards @ref m_park.
			StormByte::Safe::ConditionVariable m_parkCv;	///< Read / feeder rendezvous.
			StormByte::Safe::Atomic<bool> m_feedStop{false};	///< Feeder halt.
	};
}
