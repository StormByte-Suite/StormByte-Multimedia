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

#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/type_traits.hxx>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Detail
 * @brief Private Hold storage for @ref StormByte::Multimedia::Pipeline::Filter::Chain.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Detail {
	/**
	 * @class HoldQueue
	 * @brief Type-erased park used by @ref StormByte::Multimedia::Pipeline::Filter::Chain.
	 *
	 * One instance per node. @ref Guard serialises this park.
	 * @ref Notify wakes @ref Wait after Enqueue, Dequeue, Clear or Reserve.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE HoldQueue {
		public:
			/**
			 * @brief Destructor.
			 */
			virtual ~HoldQueue() noexcept = default;

			/**
			 * @brief Caps the park at @p n units. @p n of 0 clears the cap.
			 * @param n Maximum units, or 0.
			 */
			virtual void Reserve(std::uint8_t n) noexcept = 0;

			/**
			 * @brief Drops parked units and @ref Reserve (0).
			 */
			virtual void Clear() noexcept = 0;

			/**
			 * @brief Whether no unit is parked.
			 * @return true when empty.
			 */
			virtual bool Empty() const noexcept = 0;

			/**
			 * @brief Parked units.
			 * @return Count.
			 */
			virtual std::uint8_t Size() const noexcept = 0;

			/**
			 * @brief Wakes every waiter on this park.
			 */
			virtual void Notify() noexcept = 0;

			/**
			 * @brief Sleeps on @p lock until @ref Notify.
			 * @param lock Unique lock already owning @ref Guard.
			 */
			virtual void Wait(std::unique_lock<std::recursive_mutex>& lock) noexcept = 0;

			/**
			 * @brief Lock for this park only.
			 * @return Mutex.
			 */
			std::recursive_mutex& Guard() noexcept {
				return m_guard;
			}

		protected:
			/**
			 * @brief Empty park.
			 */
			HoldQueue() noexcept = default;

			/**
			 * @brief Copy constructor (deleted).
			 */
			HoldQueue(const HoldQueue&) = delete;

			/**
			 * @brief Move constructor (deleted).
			 */
			HoldQueue(HoldQueue&&) = delete;

			/**
			 * @brief Copy assignment (deleted).
			 * @return *this.
			 */
			HoldQueue& operator=(const HoldQueue&) = delete;

			/**
			 * @brief Move assignment (deleted).
			 * @return *this.
			 */
			HoldQueue& operator=(HoldQueue&&) = delete;

			mutable std::recursive_mutex m_guard;	///< Held-node only
	};

	/**
	 * @class HoldQueueOf
	 * @brief Typed park of @p Unit.
	 * @tparam Unit @ref StormByte::Multimedia::Pipeline::Frame or
	 *         @ref StormByte::Multimedia::Pipeline::Packet.
	 *
	 * @ingroup multimedia_pipeline
	 */
	template<typename Unit>
	requires (
		StormByte::Type::SameAs<Unit, StormByte::Multimedia::Pipeline::Frame>
		|| StormByte::Type::SameAs<Unit, StormByte::Multimedia::Pipeline::Packet>
	)
	class STORMBYTE_MULTIMEDIA_PRIVATE HoldQueueOf: public HoldQueue {
		public:
			/**
			 * @brief Empty typed park.
			 */
			HoldQueueOf() noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~HoldQueueOf() noexcept override = default;

			/**
			 * @brief Caps the park at @p n units.
			 * @param n Maximum units, or 0.
			 */
			void Reserve(std::uint8_t n) noexcept override {
				m_cap = n;
				m_cv.notify_all();
			}

			/**
			 * @brief Drops parked units and @ref Reserve (0).
			 */
			void Clear() noexcept override {
				m_units.clear();
				m_cap = 0;
				m_cv.notify_all();
			}

			/**
			 * @brief Whether no unit is parked.
			 * @return true when empty.
			 */
			bool Empty() const noexcept override {
				return m_units.empty();
			}

			/**
			 * @brief Parked units.
			 * @return Count.
			 */
			std::uint8_t Size() const noexcept override {
				return static_cast<std::uint8_t>(m_units.size());
			}

			/**
			 * @brief Wakes waiters.
			 */
			void Notify() noexcept override {
				m_cv.notify_all();
			}

			/**
			 * @brief Sleeps until @ref Notify.
			 * @param lock Owned @ref Guard.
			 */
			void Wait(std::unique_lock<std::recursive_mutex>& lock) noexcept override {
				m_cv.wait(lock);
			}

			/**
			 * @brief Parks @p unit if under the cap. Caller holds @ref Guard.
			 * @param unit Unit to take.
			 * @return false when the cap is 0 or already full. The unit is left untouched.
			 */
			bool Enqueue(Unit&& unit) noexcept {
				if (m_cap == 0 || m_units.size() >= m_cap)
					return false;
				m_units.push_back(std::move(unit));
				m_cv.notify_all();
				return true;
			}

			/**
			 * @brief Pops the oldest unit into @p unit. Caller holds @ref Guard.
			 * @param unit Destination.
			 * @return false when empty.
			 */
			bool Dequeue(Unit& unit) noexcept {
				if (m_units.empty())
					return false;
				unit = std::move(m_units.front());
				m_units.pop_front();
				m_cv.notify_all();
				return true;
			}

		private:
			std::deque<Unit> m_units;					///< Parked units, oldest first
			std::uint8_t m_cap = 0;						///< Set by @ref Reserve
			std::condition_variable_any m_cv;			///< Wait / Notify
	};
}
