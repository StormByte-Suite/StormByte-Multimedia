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
#include <StormByte/multimedia/pipeline/activity.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/telemetry.hxx>

#include <chrono>
#include <cstdint>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Provider-owned stage and queue topology of one job.
	 */
	class Topology;

	/**
	 * @brief Backend coordinator for transcoding jobs.
	 */
	class Transcoder;
}

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
			 * timings include source ticks and end-of-input flush calls,
			 * including output-delivery waits nested inside those calls;
			 * input counters count only actual units. Wait timings measure
			 * time blocked on the stage's pipeline condition. Snapshots may
			 * be read while the worker runs.
			 * Use Activity() for exclusive live per-worker wall times, not
			 * ProcessTotal()/Elapsed() or queue occupancy as utilization.
			 * Owned labels and error snapshots may allocate and propagate allocation failures.
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
					StageTelemetry();

					/**
					 * @brief Release stage telemetry in its providing library.
					 */
					~StageTelemetry() noexcept override;

					/**
					 * @brief Read independent worker activity, including open intervals.
					 * @return Owned values and explicit instrumentation coverage.
					 */
					StageActivitySnapshot Activity() const;

					/**
					 * @brief Access the main worker clock for producer instrumentation.
					 * @return Clock owned by this stage telemetry.
					 */
					WorkerActivity& MainActivity() noexcept;

					/**
					 * @brief Register another execution domain without affecting the job on allocation failure.
					 * @param role Execution domain.
					 * @param track Source track or -1 when inapplicable.
					 * @return Retained clock, or empty with Activity().Complete false.
					 */
					StormByte::Safe::Shared<WorkerActivity> RegisterActivityWorker(ActivityRole role, int track) noexcept;

					/**
					 * @brief Set the stage origin label.
					 * @param origin Human-readable stage identity.
					 */
					void SetOrigin(std::string_view origin) const;

					/**
					 * @brief Stage identity recorded by its owning object.
					 * @return Owned StormByte UTF-8 string.
					 */
					StormByte::Safe::String Origin() const;

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
					void SetError(StormByte::Safe::String reason);

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
					StormByte::Safe::Optional<StormByte::Safe::String> Error() const;

					/**
					 * @brief Flatten all stage counters into an owned Safe::String.
					 * @return Human-readable snapshot.
					 */
					operator StormByte::Safe::String() const override;

				private:
					WorkerActivity m_activity; ///< Independent main worker clock.
					mutable StormByte::Safe::Mutex m_activity_lock; ///< Guards additional worker registration.
					StormByte::Safe::Vector<StormByte::Safe::Shared<WorkerActivity>> m_activity_workers; ///< Retained additional clocks.
					bool m_activity_complete = true; ///< False after a lost registration.
					StormByte::Safe::Atomic<std::uint64_t> m_process_calls;	///< Process calls.

					StormByte::Safe::Atomic<std::uint64_t> m_input_frames;	///< Consumed frames.

					StormByte::Safe::Atomic<std::uint64_t> m_input_packets;	///< Consumed packets.

					StormByte::Safe::Atomic<std::uint64_t> m_output_frames;	///< Emitted frames.

					StormByte::Safe::Atomic<std::uint64_t> m_output_packets;	///< Emitted packets.

					StormByte::Safe::Atomic<std::int64_t> m_process_total_ns;	///< Total Process duration.

					StormByte::Safe::Atomic<std::int64_t> m_process_min_ns;	///< Minimum Process duration.

					StormByte::Safe::Atomic<std::int64_t> m_process_max_ns;	///< Maximum Process duration.

					StormByte::Safe::Atomic<std::uint64_t> m_wait_count;	///< Blocking waits.

					StormByte::Safe::Atomic<std::int64_t> m_wait_total_ns;	///< Total blocking wait duration.

					StormByte::Safe::Atomic<std::int64_t> m_wait_max_ns;	///< Maximum blocking wait duration.

					StormByte::Safe::Atomic<std::int64_t> m_setup_ns;	///< Setup duration.

					StormByte::Safe::Atomic<std::int64_t> m_started_ns;	///< Steady-clock start tick.

					StormByte::Safe::Atomic<std::int64_t> m_finished_ns;	///< Steady-clock finish tick.

					StormByte::Safe::Atomic<int> m_state;	///< Latest lifecycle state.

					mutable StormByte::Safe::Mutex m_error_lock;	///< Protects failure text.

					StormByte::Safe::Optional<StormByte::Safe::String> m_error;		///< Safe-owned failure text.

					mutable StormByte::Safe::Mutex m_origin_lock;	///< Protects origin label.

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

			/**
			 * @enum TopologyStageKind
			 * @brief Kind of real pipeline stage reported by @ref JobTelemetry::Topology.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC TopologyStageKind: std::uint8_t {
				Demuxer,			///< Container reader; owns the demux pending queues.
				Decoder,			///< Track decoder feeding a decode/encode lane.
				Filter,				///< User or built-in filter leaf (see @ref TopologyFilterKind).
				Encoder,			///< Track encoder feeding the muxer.
				Remuxer,			///< Stream-copy stage feeding the muxer.
				Muxer,				///< Container writer.
				InspectionDecoder	///< Internal decoder created only to feed analytics filters.
			};

			/**
			 * @enum TopologyFilterKind
			 * @brief Filter family of a @ref TopologyStageKind::Filter stage.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC TopologyFilterKind: std::uint8_t {
				None,		///< Stage is not a filter.
				Process,	///< Frame-processing filter (Filter::Process and derived).
				Packet,		///< Packet filter (Filter::Packet and derived).
				Analytics,	///< Observing analytics filter (Filter::Analytics and derived).
				Other		///< Filter of another family.
			};

			/**
			 * @enum TopologyQueueRole
			 * @brief Relation between a stage and one queue.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC TopologyQueueRole: std::uint8_t {
				Input,		///< The stage consumes this queue.
				Output,		///< The stage is a registered producer of this queue.
				Pending		///< Stage-owned pending queue (demux park) before its output queues.
			};

			/**
			 * @enum TopologyQueueKind
			 * @brief Storage that backs a @ref TopologyQueue.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC TopologyQueueKind: std::uint8_t {
				Pipe,			///< Buffer::Sink hopper owned by its consumer stage.
				DemuxPending	///< Demuxer per-track pending list; elements only, no byte accounting.
			};

			/**
			 * @enum TopologySnapshotState
			 * @brief Whether values are read live or are frozen copies.
			 */
			enum class STORMBYTE_MULTIMEDIA_PUBLIC TopologySnapshotState: std::uint8_t {
				Live,		///< Read from the running queue or budget when the snapshot was taken.
				Final,		///< Captured when the owning stage or job run was torn down; never changes again.
				Unfrozen	///< Snapshot only: the run was torn down but its final copy could not be allocated. Queues keep their own Final captures; budgets stay Live and may still change. Complete is false.
			};

			/**
			 * @struct TopologyQueueRef
			 * @brief Stage-to-queue link in a topology snapshot.
			 */
			struct TopologyQueueRef {
				std::uint64_t Queue = 0;									///< @ref TopologyQueue::Id.
				TopologyQueueRole Role = TopologyQueueRole::Input;			///< Stage relation to that queue.
			};

		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TopologyQueueRef);

namespace StormByte {
	namespace Multimedia {
		namespace Pipeline {
			/**
			 * @struct TopologyStage
			 * @brief Structured identity of one real pipeline stage.
			 *
			 * GUI code should use these fields rather than parse @ref Name.
			 * Absent optionals mean "not applicable" (for example the demuxer
			 * has no track and global analytics have no filter position).
			 */
			struct TopologyStage {
				std::uint64_t Id = 0;													///< Stable stage id, unique in the process.
				TopologyStageKind Kind = TopologyStageKind::Demuxer;					///< Stage kind.
				TopologyFilterKind Filter = TopologyFilterKind::None;					///< Filter family; None unless Kind is Filter.
				StormByte::Safe::Optional<int> Track;									///< Input (source) track index.
				StormByte::Safe::Optional<int> OutputTrack;								///< Output (muxer) track index.
				StormByte::Safe::Optional<enum StormByte::Multimedia::Type> Media;	///< Media handled; absent for multi-track stages.
				StormByte::Safe::Optional<std::uint32_t> FilterPosition;				///< Zero-based position in the track filter list.
				StormByte::Safe::String FilterName;										///< Filter constructor name; empty when not a filter.
				StormByte::Safe::String Name;											///< @ref JobTelemetry::Stages name for the same counters; origin label for stages not listed there.
				std::uint32_t Order = 0;												///< Longest producer hop count from a source stage.
				std::uint32_t Sequence = 0;												///< Registration order within the run.
				StormByte::Safe::Shared<const StageTelemetry> Metrics;					///< Shared stage counters.
				StageActivitySnapshot Activity;											///< Independent worker samples, frozen by value in a final topology.
				StormByte::Safe::Vector<TopologyQueueRef> Queues;						///< Inputs, outputs and pending queues of this stage.
			};

			/**
			 * @struct TopologyQueue
			 * @brief One real queue and its Buffer accounting.
			 *
			 * Each queue appears once even when several producers share it.
			 * `Snapshot.Exists == false` marks a key known to the topology
			 * that is not (or no longer) wired. Unlimited bounds have
			 * `Limits.ElementsLimited()` / `Limits.BytesLimited()` false; an
			 * engaged bound equal to zero is a real zero cap.
			 */
			struct TopologyQueue {
				std::uint64_t Id = 0;													///< Stable queue id, unique in the process.
				TopologyQueueKind Kind = TopologyQueueKind::Pipe;						///< Backing storage.
				int Key = 0;															///< Hopper key in the consumer (track index for pending queues).
				StormByte::Safe::Optional<std::uint64_t> Consumer;						///< Consumer stage id; for pending queues the owning demuxer.
				StormByte::Safe::Vector<std::uint64_t> Producers;						///< Registered producer stage ids.
				StormByte::Safe::Optional<std::uint64_t> Budget;						///< @ref TopologyBudget::Id charged by this queue, if any.
				bool BytesAccounted = true;												///< False when the queue never accounts bytes.
				TopologySnapshotState State = TopologySnapshotState::Live;				///< Live or final reading.
				StormByte::Safe::Optional<enum StormByte::Multimedia::Type> Media;	///< Media carried, when known.
				StormByte::Buffer::QueueSnapshot Snapshot;								///< Limits, occupancy, peaks, pressure, waits, Closed and Exists.
			};

			/**
			 * @struct TopologyBudget
			 * @brief Shared byte budget reported once for every queue that charges it.
			 * Identity is shared by all valid handles of the same budget,
			 * including handles first observed after closure.
			 * An invalid handle is omitted and marks the topology incomplete.
			 */
			struct TopologyBudget {
				std::uint64_t Id = 0;											///< Stable entry id; equal ids mean the same shared budget.
				TopologySnapshotState State = TopologySnapshotState::Live;		///< Live or final reading.
				bool Identified = true;											///< True for valid handles, including closed budgets.
				StormByte::Safe::Vector<std::uint64_t> Queues;					///< Queue ids charging this budget in this snapshot.
				StormByte::Buffer::BudgetSnapshot Snapshot;						///< Aggregate reservation counters.
			};

		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TopologyStage);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TopologyQueue);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TopologyBudget);

namespace StormByte {
	namespace Multimedia {
		namespace Pipeline {
			/**
			 * @struct TopologySnapshot
			 * @brief Owned, coherent-per-queue view of the pipeline stages and queues.
			 *
			 * Different queues are read one after another, not atomically as a
			 * group. Values are Buffer accounting: bytes are logical caller
			 * weights, not resident memory; a full queue is normal backpressure,
			 * not an error. Elements and bytes are independent units; no
			 * batch size or per-item average is implied.
			 */
			struct TopologySnapshot {
				bool Available = false;												///< False before a run registered its topology.
				bool RecorderFailed = false;										///< The latest run could not allocate its topology recorder: nothing was recorded, `Available` is false and `Complete` is false. The job itself is unaffected.
				bool Complete = true;												///< False when telemetry bookkeeping could not allocate; stages, queues, producer links, budgets or pending lists may be missing. Never affects the job.
				TopologySnapshotState State = TopologySnapshotState::Live;			///< Final once the run tore its stages down.
				std::uint64_t Run = 0;												///< Run generation; increases on each registration reset.
				StormByte::Safe::Vector<TopologyStage> Stages;						///< Stages in pipeline order, then registration order.
				StormByte::Safe::Vector<TopologyQueue> Queues;						///< Unique queues.
				StormByte::Safe::Vector<TopologyBudget> Budgets;					///< Unique budgets.
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::StageTelemetry);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TelemetryStage);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::TopologySnapshot);

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
			 * Stage registration and owned snapshots, including engaged memory
			 * optionals, may allocate and propagate allocation failures.
			 *
			 * @ref Topology exposes the stages and Buffer queues of the latest
			 * run, live while it runs and final after teardown.
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
					 * @throw Safe::AllocationError if synchronization storage cannot be allocated.
					 */
					JobTelemetry();

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
						StormByte::Safe::Shared<const StageTelemetry> metrics);

					/**
					 * @brief Snapshot registered stages in registration order.
					 * @return Owned list of names and shared metric handles.
					 */
					StormByte::Safe::Vector<Stage> Stages() const;

					/**
					 * @brief Snapshot the real stages, queues and shared budgets of the job.
					 *
					 * Thread-safe and usable while the job runs, is paused, after
					 * cancellation or failure, and after the Transcoder was destroyed
					 * (the returned handle of Transcoder::Telemetry() is enough).
					 * The snapshot owns no stage and no queue: live readings go through
					 * provider-owned probes that freeze to final values when their
					 * stage is destroyed. After teardown the topology becomes
					 * @ref TopologySnapshotState::Final if the final copy succeeds,
					 * otherwise @ref TopologySnapshotState::Unfrozen.
					 * @return Owned topology; `Available` is false before a run registers
					 *         it. If a run could not allocate its recorder,
					 *         `RecorderFailed` is true and `Complete` false, so it is
					 *         never mistaken for a job that has not run yet.
					 * @throw Safe::AllocationError or std::bad_alloc when owned storage cannot be allocated.
					 */
					TopologySnapshot Topology() const;

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
					StormByte::Safe::Optional<std::uint64_t> MemoryCurrent() const;

					/**
					 * @brief Lowest process resident memory sample.
					 * @return Bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> MemoryMinimum() const;

					/**
					 * @brief Highest process resident memory sample.
					 * @return Bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> MemoryMaximum() const;

					/**
					 * @brief Alias for the maximum sampled resident-memory value.
					 * @return Sampled peak bytes or empty if no sample succeeded.
					 */
					StormByte::Safe::Optional<std::uint64_t> PeakMemory() const;

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
					friend class StormByte::Multimedia::Backend::Pipeline::Transcoder;

					/**
					 * @brief Provider-owned topology recorder, created on first use.
					 *
					 * An allocation or lock failure records @ref TopologySnapshot::RecorderFailed
					 * for readers instead of being silent; a later successful
					 * acquisition clears it.
					 * @return Shared recorder, or empty when it could not be allocated.
					 */
					StormByte::Safe::Shared<StormByte::Multimedia::Backend::Pipeline::Topology> TopologyRecorder() noexcept;

					mutable StormByte::Safe::Mutex m_stages_lock;	///< Protects registered stage list and topology owner.

					StormByte::Safe::Shared<StormByte::Multimedia::Backend::Pipeline::Topology> m_topology;	///< Provider-owned topology recorder.

					StormByte::Safe::Atomic<bool> m_topology_failed;	///< Latest recorder acquisition failed (zero-initialized).

					StormByte::Safe::Vector<Stage> m_stages;		///< Safe-owned registered stage snapshots.

					StormByte::Safe::Atomic<std::uint64_t> m_memory_current;	///< Last resident-memory sample.

					StormByte::Safe::Atomic<std::uint64_t> m_memory_min;	///< Lowest resident-memory sample.

					StormByte::Safe::Atomic<std::uint64_t> m_memory_max;	///< Highest resident-memory sample.

					StormByte::Safe::Atomic<std::uint64_t> m_memory_samples;	///< Successful sample count.
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::JobTelemetry);
