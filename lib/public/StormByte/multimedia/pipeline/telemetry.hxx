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

#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/telemetry.hxx>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string_view>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Audio and video processing types.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @class StageTelemetry
			 * @brief Thread-safe counters and timings for one pipeline worker.
			 *
			 * Input/output counts distinguish frames and packets. Process
			 * timings include source ticks and end-of-input flush calls;
			 * input counters count only actual units. Wait timings measure
			 * time blocked on the stage's pipeline condition. Snapshots may
			 * be read while the worker runs.
			 *
			 * @par Memory
			 * Memory is deliberately not attributed to a stage. Multimedia
			 * and FFmpeg stages share a process heap, and some buffers are
			 * shared between stages. Use @ref JobTelemetry for sampled
			 * process resident memory.
			 *
			 * @par DLL boundary
			 * Owned values use Safe storage and destruction is provider-local.
			 * Compatible C++ ABI is required. Derived types must preserve this
			 * guarantee for their own fields and lifetime operations, including
			 * an out-of-line destructor for provider-owned allocations.
			 * Copying and moving stage telemetry are not supported.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC StageTelemetry: public StormByte::Telemetry {
				public:
					/**
					 * @brief Construct empty stage counters.
					 */
					StageTelemetry() noexcept;

					/**
					 * @brief Release stage telemetry in its providing library.
					 */
					~StageTelemetry() noexcept override;

					/**
					 * @brief Set the stage origin label.
					 * @param origin Human-readable stage identity.
					 */
					void SetOrigin(std::string_view origin) const noexcept;

					/**
					 * @brief Stage identity recorded by its owning object.
					 * @return Owned StormByte UTF-8 string.
					 */
					StormByte::Safe::String Origin() const noexcept;

					/**
					 * @brief Record one consumed unit.
					 * @param kind Frame or packet kind.
					 */
					void RecordInput(Kind kind) noexcept;

					/**
					 * @brief Record one emitted unit.
					 * @param kind Frame or packet kind.
					 */
					void RecordOutput(Kind kind) noexcept;

					/**
					 * @brief Record one Process call duration.
					 * @param duration Wall time spent inside Process.
					 */
					void RecordProcess(std::chrono::nanoseconds duration) noexcept;

					/**
					 * @brief Record one completed blocking wait.
					 * @param duration Wall time spent waiting.
					 */
					void RecordWait(std::chrono::nanoseconds duration) noexcept;

					/**
					 * @brief Record setup duration.
					 * @param duration Wall time spent in worker Setup.
					 */
					void RecordSetup(std::chrono::nanoseconds duration) noexcept;

					/**
					 * @brief Set the latest stage lifecycle state.
					 * @param state Current worker state.
					 */
					void SetState(State state) noexcept;

					/**
					 * @brief Store a stage failure reason.
					 * @param reason Failure text.
					 */
					void SetError(StormByte::Safe::String reason) noexcept;

					/**
					 * @brief Mark the start of stage setup.
					 */
					void Start() noexcept;

					/**
					 * @brief Mark the end of stage execution.
					 */
					void Finish() noexcept;

					/**
					 * @brief Number of Process calls, including source ticks and flushes.
					 * @return Process call count.
					 */
					std::uint64_t ProcessCalls() const noexcept;

					/**
					 * @brief Number of consumed frames.
					 * @return Frame count.
					 */
					std::uint64_t InputFrames() const noexcept;

					/**
					 * @brief Number of consumed packets.
					 * @return Packet count.
					 */
					std::uint64_t InputPackets() const noexcept;

					/**
					 * @brief Number of emitted frames.
					 * @return Frame count.
					 */
					std::uint64_t OutputFrames() const noexcept;

					/**
					 * @brief Number of emitted packets.
					 * @return Packet count.
					 */
					std::uint64_t OutputPackets() const noexcept;

					/**
					 * @brief Sum of Process call durations.
					 * @return Total wall time.
					 */
					std::chrono::nanoseconds ProcessTotal() const noexcept;

					/**
					 * @brief Mean Process call duration.
					 * @return Mean wall time, or zero before the first call.
					 */
					std::chrono::nanoseconds ProcessMean() const noexcept;

					/**
					 * @brief Minimum Process call duration.
					 * @return Minimum wall time, or zero before the first call.
					 */
					std::chrono::nanoseconds ProcessMinimum() const noexcept;

					/**
					 * @brief Maximum Process call duration.
					 * @return Maximum wall time.
					 */
					std::chrono::nanoseconds ProcessMaximum() const noexcept;

					/**
					 * @brief Number of blocking waits.
					 * @return Wait count.
					 */
					std::uint64_t WaitCount() const noexcept;

					/**
					 * @brief Sum of blocking wait durations.
					 * @return Total wait time.
					 */
					std::chrono::nanoseconds WaitTotal() const noexcept;

					/**
					 * @brief Maximum blocking wait duration.
					 * @return Longest wait.
					 */
					std::chrono::nanoseconds WaitMaximum() const noexcept;

					/**
					 * @brief Worker setup duration.
					 * @return Setup wall time.
					 */
					std::chrono::nanoseconds SetupTime() const noexcept;

					/**
					 * @brief Total elapsed time since Start.
					 * @return Elapsed wall time, or zero before Start.
					 */
					std::chrono::nanoseconds Elapsed() const noexcept;

					/**
					 * @brief Latest worker lifecycle state.
					 * @return Current or final state.
					 */
					State Status() const noexcept;

					/**
					 * @brief Failure text, if the stage failed.
					 * @return Failure text or empty.
					 */
					StormByte::Safe::Optional<StormByte::Safe::String> Error() const noexcept;

					/**
					 * @brief Flatten all stage counters into an owned Safe::String.
					 * @return Human-readable snapshot.
					 */
					operator StormByte::Safe::String() const override;

				private:
					std::atomic<std::uint64_t> m_process_calls;						///< Process calls.

					std::atomic<std::uint64_t> m_input_frames;						///< Consumed frames.

					std::atomic<std::uint64_t> m_input_packets;						///< Consumed packets.

					std::atomic<std::uint64_t> m_output_frames;						///< Emitted frames.

					std::atomic<std::uint64_t> m_output_packets;					///< Emitted packets.

					std::atomic<std::int64_t> m_process_total_ns;					///< Total Process duration.

					std::atomic<std::int64_t> m_process_min_ns;						///< Minimum Process duration.

					std::atomic<std::int64_t> m_process_max_ns;						///< Maximum Process duration.

					std::atomic<std::uint64_t> m_wait_count;						///< Blocking waits.

					std::atomic<std::int64_t> m_wait_total_ns;						///< Total blocking wait duration.

					std::atomic<std::int64_t> m_wait_max_ns;						///< Maximum blocking wait duration.

					std::atomic<std::int64_t> m_setup_ns;							///< Setup duration.

					std::atomic<std::int64_t> m_started_ns;							///< Steady-clock start tick.

					std::atomic<std::int64_t> m_finished_ns;						///< Steady-clock finish tick.

					std::atomic<State> m_state;										///< Latest lifecycle state.

					mutable std::mutex m_error_lock;								///< Protects failure text.

					StormByte::Safe::Optional<StormByte::Safe::String> m_error;		///< Safe-owned failure text.

					mutable std::mutex m_origin_lock;								///< Protects origin label.

					mutable StormByte::Safe::String m_origin;						///< Stage origin label.
			};

			/**
			 * @struct TelemetryStage
			 * @brief Named stage telemetry retained past worker teardown.
			 *
			 * Fields and implicit value operations use Safe-owned storage.
			 * Compatible C++ ABI and the StageTelemetry derived-type lifetime
			 * contract are required for retained metric handles.
			 */
			struct TelemetryStage {
				StormByte::Safe::String Name;							///< Display name, including track scope when applicable.

				StormByte::Safe::Shared<const StageTelemetry> Metrics;	///< Shared stage counter handle retained across DLL boundaries.
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::StageTelemetry);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TelemetryStage);

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Audio and video processing types.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @class JobTelemetry
			 * @brief Retained job telemetry with per-stage snapshots and RSS samples.
			 *
			 * Resident memory is sampled process-wide. Minimum and maximum
			 * are the lowest and highest successful samples during this job;
			 * PeakMemory is the sampled maximum, not an allocator-exact or
			 * OS-lifetime peak. Sampling is nominally every 20 ms while the
			 * job is running. It is not attributed to individual stages.
			 *
			 * @par DLL boundary
			 * Owned values use Safe storage and destruction is provider-local.
			 * Compatible C++ ABI and the StageTelemetry derived-type lifetime
			 * contract are required. Copying and moving are not supported.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC JobTelemetry final: public StormByte::Telemetry {
				public:
					/**
					 * @brief Named stage telemetry retained past worker teardown.
					 */
					using Stage = TelemetryStage;

					/**
					 * @brief Construct empty job telemetry.
					 */
					JobTelemetry() noexcept;

					/**
					 * @brief Release job telemetry in its providing library.
					 */
					~JobTelemetry() noexcept override;

					/**
					 * @brief Add a stage handle once.
					 * @param name Display name.
					 * @param metrics Stage counters to retain.
					 */
					void RegisterStage(StormByte::Safe::String name,
						StormByte::Safe::Shared<const StageTelemetry> metrics) noexcept;

					/**
					 * @brief Snapshot registered stages in registration order.
					 * @return Owned list of names and shared metric handles.
					 */
					StormByte::Safe::Vector<Stage> Stages() const noexcept;

					/**
					 * @brief Sample current process resident memory.
					 *
					 * Unsupported platforms or unavailable OS counters add no sample.
					 */
					void SampleMemory() noexcept;

					/**
					 * @brief Current process resident bytes at the last sample.
					 * @return Bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> MemoryCurrent() const noexcept;

					/**
					 * @brief Lowest process resident memory sample.
					 * @return Bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> MemoryMinimum() const noexcept;

					/**
					 * @brief Highest process resident memory sample.
					 * @return Bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> MemoryMaximum() const noexcept;

					/**
					 * @brief Alias for the maximum sampled resident-memory value.
					 * @return Sampled peak bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> PeakMemory() const noexcept;

					/**
					 * @brief Number of successful resident-memory samples.
					 * @return Sample count.
					 */
					std::uint64_t MemorySamples() const noexcept;

					/**
					 * @brief Flatten memory and stage counters into an owned Safe::String.
					 * @return Multi-line human-readable snapshot.
					 */
					operator StormByte::Safe::String() const override;

				private:
					mutable std::mutex m_stages_lock;				///< Protects registered stage list.

					StormByte::Safe::Vector<Stage> m_stages;		///< Safe-owned registered stage snapshots.

					std::atomic<std::uint64_t> m_memory_current;	///< Last resident-memory sample.

					std::atomic<std::uint64_t> m_memory_min;		///< Lowest resident-memory sample.

					std::atomic<std::uint64_t> m_memory_max;		///< Highest resident-memory sample.

					std::atomic<std::uint64_t> m_memory_samples;	///< Successful sample count.
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::JobTelemetry);
