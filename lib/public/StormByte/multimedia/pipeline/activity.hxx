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

#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/vector.hxx>

#include <chrono>
#include <cstdint>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia processing types.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Asynchronous pipeline telemetry.
		 */
		namespace Pipeline {
			/**
			 * @brief Exclusive wall-clock activity of one instrumented worker, not CPU use.
			 * Unknown is unclassified time, never an inferred idle/inefficient state.
			 */
			enum class ActivityState: std::uint8_t {
				Unknown, ///< Unclassified worker administration or unsupported wait.
				Preparing, ///< Executing Setup, excluding instrumented waits.
				Processing, ///< Executing Process, excluding instrumented waits.
				WaitingInput, ///< Waiting for upstream data or readiness.
				WaitingOutput, ///< Waiting for downstream local capacity or pending output to drain.
				WaitingBudget, ///< Waiting for admission to the shared byte budget.
				Paused, ///< Actually blocked by a cooperative pause.
				Finished, ///< Execution ended, including cancellation.
				Failed ///< Execution ended with a stage failure.
			};

			/**
			 * @brief Concurrent execution domains; codec-internal threads are not observed.
			 */
			enum class ActivityRole: std::uint8_t {
				Main, ///< Stage's pumper thread.
				DemuxFeeder ///< Independent per-track demux output thread.
			};

			/**
			 * @brief Coherent, allocation-free value for one worker execution.
			 *
			 * Steady-clock references are process-local nanoseconds, not UTC.
			 * SampledAt includes the currently open interval; after termination it
			 * equals FinishedAt and the entire value is immutable until another Start.
			 * The seven durations sum exactly to Elapsed(), including unclassified time.
			 * Compare only samples with the same topology Run, stage Id, worker Id and
			 * Generation. Workers are independent: never sum their percentages as one
			 * exclusive stage state.
			 */
			struct WorkerActivitySnapshot {
				std::uint64_t Id = 0; ///< Process-unique worker identity.
				std::uint64_t Generation = 0; ///< Increments on each new execution.
				ActivityRole Role = ActivityRole::Main; ///< Execution domain.
				int Track = -1; ///< Source track for a demux feeder; -1 for the main worker.
				bool Available = false; ///< False until this execution starts.
				ActivityState State = ActivityState::Unknown; ///< State at SampledAt.
				std::chrono::nanoseconds StartedAt{}; ///< Beginning of this generation.
				std::chrono::nanoseconds SampledAt{}; ///< Coherent reference tick, frozen at termination.
				std::chrono::nanoseconds FinishedAt{}; ///< Termination tick; zero while live.
				std::chrono::nanoseconds Unknown{}; ///< Unclassified wall time.
				std::chrono::nanoseconds Preparing{}; ///< Setup wall time.
				std::chrono::nanoseconds Processing{}; ///< Process wall time minus instrumented waits.
				std::chrono::nanoseconds WaitingInput{}; ///< Upstream waits.
				std::chrono::nanoseconds WaitingOutput{}; ///< Local downstream waits.
				std::chrono::nanoseconds WaitingBudget{}; ///< Aggregate budget waits.
				std::chrono::nanoseconds Paused{}; ///< Cooperative pause waits.

				/**
				 * @brief Observed lifetime of this execution, frozen on termination.
				 * @return Wall time accounted by the seven categories.
				 */
				std::chrono::nanoseconds Elapsed() const noexcept { return SampledAt - StartedAt; }

				/**
				 * @brief Compare complete samples, including identities and reference ticks.
				 * @param other Sample to compare.
				 * @return True when every field matches.
				 */
				bool operator==(const WorkerActivitySnapshot& other) const noexcept = default;
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::WorkerActivitySnapshot);

namespace StormByte {
	namespace Multimedia {
		namespace Pipeline {
			/**
			 * @brief Activity coverage and independent workers of a stage.
			 *
			 * Available means at least one worker started. Complete is false if worker
			 * registration failed. No workers / unavailable measurements mean unknown,
			 * not zero work. WaitReasons distinguishes observed Buffer admission waits
			 * by their real cause, not queue occupancy. Internal codec/I/O waits remain
			 * part of Processing unless explicitly instrumented.
			 */
			struct StageActivitySnapshot {
				bool Available = false; ///< At least one instrumented execution started.
				bool Complete = true; ///< False when a worker registration was lost.
				bool WaitReasons = true; ///< Local capacity and shared budget waits are distinguished.
				bool InternalWorkersObserved = false; ///< Codec, filter-helper and I/O service threads are outside coverage.
				StormByte::Safe::Vector<WorkerActivitySnapshot> Workers; ///< Separate domains, never an exclusive aggregate.
			};

			/**
			 * @brief Provider-owned clock for one execution thread.
			 *
			 * Readers may snapshot concurrently. Producer transitions must be serialized
			 * by their worker. Start begins a new generation in Preparing. Exchange
			 * closes the previous category at the same clock tick it opens the next;
			 * nested scopes restore the previous category, never add overlapping time.
			 * Finish is idempotent and terminal; Exchange cannot reopen a finished clock.
			 * Snapshots are scalar values, and all owned storage/synchronization use
			 * Safe types with provider-local destruction. Keep Multimedia and Base
			 * loaded for the lifetime of retained handles; compatible C++ ABI is required.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC WorkerActivity final {
				public:
					/**
					 * @brief Construct an unstarted worker clock.
					 * @param role Execution domain.
					 * @param track Source track for a demux feeder, otherwise -1.
					 */
					explicit WorkerActivity(ActivityRole role = ActivityRole::Main, int track = -1);

					WorkerActivity(const WorkerActivity&) = delete;

					WorkerActivity(WorkerActivity&&) noexcept = delete;

					/**
					 * @brief Destroy synchronization in the providing library.
					 */
					~WorkerActivity() noexcept;

					WorkerActivity& operator=(const WorkerActivity&) = delete;

					WorkerActivity& operator=(WorkerActivity&&) noexcept = delete;

					/**
					 * @brief Begin a generation in Preparing, or leave a running generation unchanged.
					 */
					void Start() noexcept;

					/**
					 * @brief Close the current interval and open a nonterminal category.
					 * @param state New category; terminal values are reserved for Finish.
					 * @return Previous category, for restoring a nested scope.
					 */
					ActivityState Exchange(ActivityState state) noexcept;

					/**
					 * @brief Freeze this generation once; subsequent transitions have no effect.
					 * @param failed Whether execution ended with a stage failure.
					 */
					void Finish(bool failed = false) noexcept;

					/**
					 * @brief Copy state, totals and reference tick under the same short lock.
					 * @return Owned scalar value including the interval still open.
					 */
					WorkerActivitySnapshot Snapshot() const noexcept;

				private:
					mutable StormByte::Safe::Mutex m_mutex; ///< Serializes state/tick publication and readers.
					WorkerActivitySnapshot m_value; ///< Closed intervals and execution identity.
					std::chrono::nanoseconds m_changed{}; ///< Last interval boundary.
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::WorkerActivity);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::StageActivitySnapshot);
