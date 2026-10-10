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

#include <StormByte/size.hxx>
#include <StormByte/buffer/sink.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/condition_variable.hxx>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/pair.hxx>
#include <StormByte/safe/unique_lock.hxx>
#include <StormByte/multimedia/visibility.h>

#include <chrono>
#include <cstddef>
#include <StormByte/safe/deque.hxx>
#include <StormByte/safe/unordered_set.hxx>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Pipe
	 * @brief Input and output hoppers of one stage.
	 *
	 * Private. Step and Filter::FFmpeg compose one Pipe each.
	 * Not installed. @c operator>> between Pipes is redirect
	 * (Notify destination input, then Bind). It does not set
	 * Capacity: the call site uses the consumer's
	 * InputCeiling on @ref Capacity after Bind.
	 *
	 * Item flow is left to right, same as the public tube:
	 * @c pipe << item / @c item >> pipe write to Out (Push,
	 * blocks on the bound consumer Capacity). @c pipe >> item
		 * is Pop from In only; the Host delegates Wait to this Pipe.
	 *
	 * The free @c operator>>(item, pipe) overloads are declared
	 * in this namespace (not friend-only) so GCC can define them
	 * out of line. They move the unit into @c pipe Out.
	 *
	 * @ref CloneTo may be called more than once (per-track VMAF
	 * plus a general VMAF). Each write clones to every dest.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Pipe {
		public:
			/**
			 * @brief Unit carried by the stage hoppers.
			 */
			using Item = StormByte::Multimedia::Pipeline::Item;

			/**
			 * @brief Track-keyed sink retaining boundary-safe unit pointers.
			 */
			using ItemSink = StormByte::Buffer::Sink<Item::PointerType>;

			/**
			 * @brief Creates empty hoppers and provider-owned wait synchronization.
			 */
			Pipe() noexcept;

			/**
			 * @brief Copying a registered consumer is not allowed.
			 * @param other Source pipe.
			 */
			Pipe(const Pipe& other) = delete;

			/**
			 * @brief Moving a registered consumer is not allowed.
			 * @param other Source pipe.
			 */
			Pipe(Pipe&& other) noexcept = delete;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source pipe.
			 * @return This pipe.
			 */
			Pipe& operator=(const Pipe& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source pipe.
			 * @return This pipe.
			 */
			Pipe& operator=(Pipe&& other) noexcept = delete;

			/**
			 * @brief Drops @c Notify on In, Out and clone hoppers.
			 *
			 * Registrations are removed before the owned CV is destroyed.
			 */
			~Pipe() noexcept;

			/**
			 * @brief Notifies all stage waiters.
			 */
			void Wake() noexcept;

			/**
			 * @brief Waits for a stored notification until a predicate is satisfied.
			 * @param owner Provider-local context valid throughout the call.
			 * @param ready Non-null predicate reading synchronized control state.
			 * @note Predicate changes must be published before Wake. Notifications
			 * between predicate evaluation and waiting are retained by a generation
			 * counter. Registered input hoppers publish data and EOF notifications
			 * to the same counter.
			 */
			void WaitWake(void* owner, bool (*ready)(void*) noexcept) noexcept;

			/**
			 * @brief Waits on the owner predicate and completes under the wait lock.
			 * @param owner Provider-local context, valid throughout the call.
			 * @param ready Predicate evaluated with the wait mutex held.
			 * @param completed Records elapsed wait time and runs the owner wake hook.
			 *
			 * Callbacks must be non-null and are invoked synchronously, never retained.
			 * Timing starts after acquiring the mutex. The completion callback runs
			 * before releasing it, preserving the owner's post-wake lock scope.
			 * Data, EOF and explicit wake notifications are retained by a generation
			 * counter, including those racing with predicate evaluation.
			 */
			void Wait(void* owner, bool (*ready)(void*) noexcept,
				void (*completed)(void*, std::chrono::nanoseconds) noexcept) noexcept;

			/**
			 * @brief Input hopper.
			 * @return In sink.
			 */
			ItemSink& In() noexcept;

			/**
			 * @brief Input hopper.
			 * @return In sink.
			 */
			const ItemSink& In() const noexcept;

			/**
			 * @brief Output hopper.
			 * @return Out sink.
			 */
			ItemSink& Out() noexcept;

			/**
			 * @brief Output hopper.
			 * @return Out sink.
			 */
			const ItemSink& Out() const noexcept;

			/**
			 * @brief Ceiling of the input hopper for @p track.
			 * @param track Hopper key.
			 * @param n Max queued items. Caller skips this when 0.
			 *
			 * After Bind, Out and In are the same hopper: this is
			 * the only cap. There is no out ceiling.
			 */
			void Capacity(int track, StormByte::Size n) noexcept;

			/**
			 * @brief Registers the owned consumer CV on In (Launch and Bind).
			 */
			void Listen() noexcept;

			/**
			 * @brief Eof on In, Out and every CloneTo hopper.
			 *
			 * CloseOutput uses this so analytics looks see Eof
			 * when the producer finishes (not only on Halt).
			 */
			void Close() noexcept;

			/**
			 * @brief Fork each write: clone onto @p dest In, original to Out.
			 * @param track Hopper key.
			 * @param dest Analytics (or look) consumer.
			 * @return @p dest.
			 *
			 * Notify dest In, Bind a clone hopper. Several CloneTo
			 * calls fork to several dests. Call before Emit.
			 */
			Pipe& CloneTo(int track, Pipe& dest) noexcept;

			/**
			 * @brief Drain Out until a consumer Binds.
			 */
			void Drain() noexcept;

			/**
			 * @brief Whether In has a unit or is EoF.
			 * @return @c In().Ready().
			 */
			bool Ready() const noexcept;

			/**
			 * @brief Whether In has seen Eof.
			 * @return @c In().EoF().
			 */
			bool InputEof() const noexcept;

			/**
			 * @class Lane
			 * @brief One-track redirect: @c from.To(track) >> dest.
			 */
			class STORMBYTE_MULTIMEDIA_PRIVATE Lane {
				public:
					/**
					 * @brief Wire this track onto @p dest.
					 * @param dest Consumer pipe.
					 * @return @p dest.
					 *
					 * Empty dest In: first producer (Out onto In).
					 * Dest In already wired (CloneTo): extra producer
					 * (In onto Out). Bind is not a Route API.
					 */
					Pipe& operator>>(Pipe& dest) noexcept;

				private:
					/**
					 * @brief Allows the owning Pipe to construct a lane.
					 */
					friend class Pipe;

					/**
					 * @brief Selects a track of a borrowed source pipe.
					 * @param from Source pipe, which must outlive the lane.
					 * @param track Hopper key.
					 */
					Lane(Pipe& from, int track) noexcept;

					Pipe* m_from;	///< Borrowed source pipe
					int m_track;	///< Selected hopper key
			};

			/**
			 * @brief Redirect of one hopper key.
			 * @param track Hopper key.
			 * @return Lane for @c >> dest.
			 */
			Lane To(int track) noexcept;

			/**
			 * @brief Notify dest In, Bind every hopper.
			 * @param dest Consumer pipe.
			 * @return @p dest.
			 */
			Pipe& operator>>(Pipe& dest) noexcept;

			/**
			 * @brief Pop one unit from In into @p item. Does not Wait.
			 * @param item Destination pointer (may become empty).
			 * @return *this.
			 */
			Pipe& operator>>(Item::PointerType& item) noexcept;

			/**
			 * @brief Push @p item to Out. Blocks on dest Capacity.
			 * @param item Unit. Empty is a no-op.
			 * @return *this.
			 *
			 * Key is @c item->Track(). If @ref CloneTo ran, a Clone
			 * is Push ed to the clone hopper first (does not steal
			 * the original; that still blocks on dest Capacity).
			 */
			Pipe& operator<<(Item::PointerType item) noexcept;

			/**
			 * @brief Write: @p item flows into @p pipe Out.
			 * @param item Unit. Moved. Empty is a no-op.
			 * @param pipe Destination pipe.
			 * @return @p pipe.
			 *
			 * Friend so the free function can call @c operator<<.
			 * A matching namespace declaration lives after this class
			 * (GCC will not define a friend-only operator out of line).
			 */
			friend Pipe& operator>>(Item::PointerType& item, Pipe& pipe) noexcept;

			/**
			 * @brief Write: @p item flows into @p pipe Out.
			 * @param item Unit. Empty is a no-op.
			 * @param pipe Destination pipe.
			 * @return @p pipe.
			 *
			 * Rvalue overload of the free write. Same namespace
			 * declaration as the lvalue overload.
			 */
			friend Pipe& operator>>(Item::PointerType&& item, Pipe& pipe) noexcept;

		private:
			StormByte::Safe::ConditionVariable m_wake;	///< Provider-owned consumer CV
			StormByte::Safe::Mutex m_wait;				///< Mutex held through wait completion
			StormByte::Safe::Atomic<std::size_t> m_wakeGeneration{0};	///< Published explicit control notifications
			ItemSink m_in;						///< Input buckets
			ItemSink m_out;						///< Output buckets

			/**
			 * @brief Track-specific clone destination.
			 */
			using Fork = StormByte::Safe::Pair<int, StormByte::Safe::Shared<ItemSink>>;

			StormByte::Safe::Deque<Fork> m_forks;	///< CloneTo destinations.
			StormByte::Safe::Mutex m_forkMutex;	///< Guards clone registration and snapshots.
			bool m_closed = false;			///< Output and clone destinations have reached EOF.
			StormByte::Safe::UnorderedSet<int> m_inTracks;	///< Keys already wired on In
	};

	/**
	 * @brief Write: lvalue @p item flows into @p pipe Out.
	 * @param item Unit. Moved. Empty is a no-op.
	 * @param pipe Destination pipe.
	 * @return @p pipe.
	 *
	 * Namespace declaration required by GCC next to the friend
	 * inside @ref Pipe. Implementation is @c pipe << std::move(item).
	 */
	Pipe& operator>>(Pipe::Item::PointerType& item, Pipe& pipe) noexcept;

	/**
	 * @brief Write: rvalue @p item flows into @p pipe Out.
	 * @param item Unit. Empty is a no-op.
	 * @param pipe Destination pipe.
	 * @return @p pipe.
	 *
	 * Namespace declaration required by GCC next to the friend
	 * inside @ref Pipe. Implementation is @c pipe << std::move(item).
	 */
	Pipe& operator>>(Pipe::Item::PointerType&& item, Pipe& pipe) noexcept;
}
