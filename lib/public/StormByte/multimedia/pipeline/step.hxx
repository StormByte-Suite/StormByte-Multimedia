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

#include <StormByte/buffer/sink.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace StormByte::Multimedia::Backend::Pipeline {
	class Host;
	class Pipe;
	class Pumper;
	class Worker;
}

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	class Decoder;
	class Demuxer;
	class Encoder;
	class Muxer;
	class Remuxer;
	class Route;
	class Filters;
	class Step;

	/**
	 * @brief Shares the Plan and connects output from @p from to @p to.
	 *
	 * Demuxer fan-out is not this operator. A demuxer binds one origin
	 * index at a time using the corresponding operator>> overload.
	 *
	 * Analytics are configured separately through @ref Filters.
	 *
	 * @param from Producer step.
	 * @param to Consumer step.
	 * @return @p to.
	 */
	STORMBYTE_MULTIMEDIA_PUBLIC class Step& operator>>(Step& from, Step& to) noexcept;

	/**
	 * @class Step
	 * @brief A pipeline stage that receives and emits media items.
	 *
	 * Connect stages with operator>> and inspect their lifecycle with
	 * @ref Status. The accepted and produced item kinds describe which
	 * connections are compatible.
	 *
	 * Log scopes use the @c StormByte/Multimedia/ prefix followed by
	 * the producer name. The supplied logger controls throttling.
	 *
	 * @ref Filters can attach analytics without changing the items
	 * delivered to the next processing stage.
	 *
	 * @par DLL boundary
	 * Backend storage and synchronization are created and released by Multimedia.
	 * Retained owners use Safe storage. Compatible C++ ABI and loaded Base,
	 * Multimedia and derived-class providers are required through destruction.
	 * Derived classes must preserve these ownership and lifetime guarantees.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Step {
		friend class Demuxer;
		friend class Muxer;
		friend class Remuxer;
		friend class Route;
		friend class Filters;
		friend Decoder& operator>>(Demuxer& demuxer, Decoder& decoder) noexcept;
		friend Demuxer& operator>>(class Plan&& plan, Demuxer& demuxer) noexcept;
		friend Demuxer& operator>>(StormByte::Safe::Shared<class Plan> plan, Demuxer& demuxer) noexcept;
		friend Encoder& operator>>(Encoder& encoder, Muxer& muxer) noexcept;
		friend Muxer& operator>>(Demuxer& demuxer, Muxer& muxer) noexcept;
		friend Remuxer& operator>>(Demuxer& demuxer, Remuxer& remuxer) noexcept;
		friend Remuxer& operator>>(Remuxer& remuxer, Muxer& muxer) noexcept;
		friend Step& operator>>(Step& from, Step& to) noexcept;

		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			Step(const Step& other) = delete;
			Step(Step&& other) noexcept = delete;

			/**
			 * @brief Destructor. Waits for stage execution to finish.
			 */
			virtual ~Step() noexcept;

			Step& operator=(const Step& other) = delete;
			Step& operator=(Step&& other) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @name Sinks
			 * @{
			 */

			/**
			 * @brief Notifies all waiters of this step.
			 */
			void Wake() noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Plan
			 * @{
			 */

			/**
			 * @brief Bound job intention, if any.
			 * @return Shared Plan, or empty.
			 */
			inline const StormByte::Safe::Shared<class Plan>& Plan() const noexcept {
				return m_plan;
			}

			/**
			 * @}
			 */

			/**
			 * @name Flow
			 * @{
			 */

			/**
			 * @brief Kinds this step consumes.
			 * @return Mask set at construction.
			 */
			inline const Kinds& Receives() const noexcept {
				return m_receives;
			}

			/**
			 * @brief Kinds this step emits.
			 * @return Mask set at construction.
			 */
			inline const Kinds& Produces() const noexcept {
				return m_produces;
			}

			/**
			 * @brief Current lifecycle value.
			 * @return @ref State of this step only.
			 */
			State Status() const noexcept;

			/**
			 * @brief Shared stage counters, retained independently of this Step.
			 * @return Const metrics handle identifying this stage.
			 */
			StormByte::Safe::Shared<const StageTelemetry> Telemetry() const noexcept;

			/**
			 * @brief Whether this step can take work.
			 *
			 * By default, @ref Status must be State::Ready. Muxer also
			 * requires @ref Muxer::Armed before writing output.
			 *
			 * @return true when the stage is open for work.
			 */
			virtual bool Ready() const noexcept;

			/**
			 * @brief Requests shutdown and notifies connected stages.
			 *
			 * Returns without waiting for completion. Safe to call more than
			 * once. A stopped stage cannot be restarted.
			 */
			void Stop() noexcept;

			/**
			 * @brief Maximum number of queued input items.
			 * @return Max queued items. 0 means unbounded.
			 *
			 * Derived stages override this to specify their input limit.
			 */
			virtual std::size_t InputCeiling() const noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Failure
			 * @{
			 */

			/**
			 * @brief Whether this step has failed.
			 * @return true iff Status is Failed.
			 */
			bool Failed() const noexcept;

			/**
			 * @brief Failure text.
			 * @return Message, or empty.
			 */
			const StormByte::Safe::Optional<StormByte::Safe::String>& Error() const noexcept;

			/**
			 * @brief Marks the stage as failed and notifies connected stages.
			 *
			 * Returns without waiting for completion. Stores the reason
			 * in @ref Error but does not log it.
			 *
			 * @param reason Message.
			 */
			void Fail(StormByte::Safe::String reason) noexcept;

			/**
			 * @brief Copies borrowed failure text before calling the DLL-safe overload.
			 * @param reason Borrowed message, used only during this call.
			 */
			STORMBYTE_FORCE_INLINE void Fail(std::string_view reason) noexcept {
				Fail(StormByte::Safe::String(reason));
			}

			/**
			 * @}
			 */

		protected:

			/**
			 * @name Construction
			 * @{
			 */

			/**
			 * @brief Constructs a stage in State::Created.
			 * @param log Base-heap shared logger. Prefer @c StormByte::Logger::ThreadedLog
			 *        when several workers write. A plain @c Log is accepted
			 *        for single-thread use. Empty pointer means no log.
			 * @param name Stage name for log scope and default @ref Label.
			 * @param receives Kinds this step consumes.
			 * @param produces Kinds this step emits.
			 */
			Step(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				enum Producer name,
				Kinds receives, Kinds produces) noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Work
			 * @{
			 */

			/**
			 * @brief Sends @p item to connected consumers.
			 * @param item Unit to emit. Empty is a no-op.
			 *
			 * Configured analytics receive copies without changing @p item.
			 */
			void Emit(Item::PointerType item) noexcept;

			/**
			 * @brief Creates a distinct item through @ref Item::Clone.
			 * @param item Unit to clone.
			 * @return Owning pointer, or empty if @p item cannot clone.
			 *
			 * Media buffers are shared according to @ref Item::Clone;
			 * the returned item does not duplicate the media payload.
			 */
			Item::PointerType CloneItem(const Item& item) const noexcept;

			/**
			 * @brief Waits until input is available, the stage fails or stops,
			 *        or @ref WakeNow.
			 *
			 * After the CV unblocks, runs @ref AfterWait on this
			 * worker. Step does not interpret @ref WakeNow.
			 */
			virtual void Wait() noexcept;

			/**
			 * @brief Extra reason to leave @ref Wait.
			 * @return true to wake without available input. Default false.
			 *
			 * Leaf office. Step does not know why the leaf wakes.
			 */
			virtual bool WakeNow() const noexcept;

			/**
			 * @brief Work on the waiting worker after @ref Wait unblocks.
			 *
			 * Default no-op. Leaf office.
			 */
			virtual void AfterWait() noexcept;

			/**
			 * @brief Starts stage initialization and processing.
			 *
			 * Idempotent. No-op without an execution handler, or if the
			 * stage has already Failed or Stopped.
			 */
			void Launch() noexcept;

			/**
			 * @brief Stops the stage and waits for execution to finish.
			 *
			 * Safe to call more than once. Returns after stage execution
			 * finishes. Filter plugins do not need to call this.
			 */
			void Halt() noexcept;

			/**
			 * @class Join
			 * @brief Ensures a derived stage stops before its resources expire.
			 *
			 * Declare this guard last in a derived stage. Its destructor
			 * waits for stage execution to finish. Not a filter plugin API.
			 */
			class Join final {
				public:
					explicit Join(Step& step) noexcept: m_step(step) {}
					Join(const Join&) = delete;
					Join(Join&&) noexcept = delete;
					~Join() noexcept {
						m_step.Halt();
					}
					Join& operator=(const Join&) = delete;
					Join& operator=(Join&&) noexcept = delete;

				private:
					Step& m_step;	///< Borrowed stage that must outlive this guard; halted when the guard is destroyed.
			};

			/**
			 * @brief Worker must return (Stop, Halt or Fail).
			 * @return true if Status is Stopping, Stopped or Failed.
			 */
			bool Stopping() const noexcept;

			/**
			 * @brief Adds one Process duration to the step summary.
			 * @param microseconds Wall time of that Process call.
			 */
			void RecordWork(std::int64_t microseconds) noexcept;

			/**
			 * @brief Duration of the last timed Process, or 0.
			 * @return Microseconds.
			 */
			std::int64_t LastWork() const noexcept;

			/**
			 * @brief Writes min/max Process time at Debug. No-op if none.
			 */
			void DumpWork() noexcept;

			/**
			 * @internal
			 * @brief Owner surface for the Pumper and Worker.
			 * @return Host implemented by this Step.
			 * @endinternal
			 */
			Backend::Pipeline::Host& Face() noexcept;

			/**
			 * @internal
			 * @brief Takes ownership of @p pumper and binds @p worker.
			 * @param pumper Source, Through or Sink. Must not be empty.
			 * @param worker Stage body. Must not be empty.
			 *
			 * Construct both owners with @c StormByte::Safe::Heap::MakeUnique.
			 * Their storage is released on Base's heap after execution stops.
			 * No-op if a pumper is already mounted. Does not Launch.
			 * @endinternal
			 */
			void Mount(StormByte::Safe::Unique<Backend::Pipeline::Pumper> pumper,
				StormByte::Safe::Unique<Backend::Pipeline::Worker> worker) noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Logging
			 * @{
			 */

			/**
			 * @brief Display name of this step.
			 * @return @ref Producer name (`Encoder`) unless a leaf overrides
			 *         it (`Encoder(libx265)`).
			 */
			virtual StormByte::Safe::String Label() const noexcept;

			/**
			 * @brief Writes one log line on the scoped module logger.
			 * @param level StormByte::Logger::Level of this line.
			 * @param message Already-formatted text (caller may use std::format).
			 *        Must not include the level name; Logger prints that.
			 *
			 * No-op without a logger. Overrides may customize delivery.
			 * The component scope uses the @c StormByte/Multimedia/ prefix
			 * followed by the stage name.
			 */
			virtual void Log(StormByte::Logger::Level level, std::string_view message) noexcept;

			/**
			 * @}
			 */

			StormByte::Safe::Shared<StormByte::Logger::Log> m_log;	///< Scoped logger retained through Base-heap shared ownership
			enum Producer m_name;								///< UseLog leaf / default Label
			Kinds m_receives;									///< Receives
			Kinds m_produces;									///< Produces

		private:
			/**
			 * @brief Provider-local backend ownership and synchronization.
			 */
			class PrivateState;

			PrivateState* m_state;								///< Provider-owned backend state

		protected:
			/**
			 * @internal
			 * @brief In / out hoppers of this stage.
			 * @return The composed Pipe.
			 * @endinternal
			 */
			Backend::Pipeline::Pipe& pipe() noexcept;

			/**
			 * @internal
			 * @brief In / out hoppers of this stage.
			 * @return The composed Pipe.
			 * @endinternal
			 */
			const Backend::Pipeline::Pipe& pipe() const noexcept;

		private:
			/**
			 * @brief Provider-local Host adapter for the mounted worker.
			 */
			class Surface;

			/**
			 * @brief Eof on the Pipe (In, Out, clone hopper).
			 */
			void CloseHoppers() noexcept;

			StormByte::Safe::Shared<class Plan> m_plan;				///< Current plan retained through Base-heap shared ownership
			StormByte::Safe::Optional<StormByte::Safe::String> m_error;	///< Fail message
			StormByte::Safe::Shared<StageTelemetry> m_telemetry;		///< Counters retained by telemetry snapshots
			bool m_exhausted;										///< Source Ended()
			std::uint64_t m_workN;									///< Timed Process calls
			std::int64_t m_workMin;									///< Fastest Process, us
			std::int64_t m_workMax;									///< Slowest Process, us
			std::int64_t m_lastWork;								///< Last Process, us
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Step);
