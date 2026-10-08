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

#include <StormByte/multimedia/pipeline/config/base.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/atomic.hxx>
#include <StormByte/safe/condition_variable.hxx>
#include <StormByte/safe/mutex.hxx>
#include <StormByte/safe/thread.hxx>
#include <StormByte/safe/unique_lock.hxx>
#include <StormByte/safe/vector.hxx>


/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class TranscoderSlot
	 * @brief One mapped output track of a Transcoder job.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE TranscoderSlot {
		public:
			/**
			 * @brief Empty slot.
			 */
			TranscoderSlot() noexcept = default;

			/**
			 * @brief Deep-copies the config and copies Base-owned filter handles.
			 * @param other Source slot.
			 */
			TranscoderSlot(const TranscoderSlot& other);

			/**
			 * @brief Transfers the config owner and filter handles.
			 * @param other Source slot.
			 */
			TranscoderSlot(TranscoderSlot&& other) noexcept;

			/**
			 * @brief Destroys the slot in its provider module.
			 */
			~TranscoderSlot() noexcept;

			/**
			 * @brief Deep-copies the config and copies Base-owned filter handles.
			 * @param other Source slot.
			 * @return This slot.
			 */
			TranscoderSlot& operator=(const TranscoderSlot& other);

			/**
			 * @brief Transfers the slot state.
			 * @param other Source slot.
			 * @return This slot.
			 */
			TranscoderSlot& operator=(TranscoderSlot&& other) noexcept;

			int In = -1;								///< Origin stream index
			int Out = -1;								///< Mux destination order
			StormByte::Multimedia::Type Kind = StormByte::Multimedia::Type::Unknown;	///< Media kind
			const StormByte::Multimedia::Codec* Source = nullptr;	///< Origin codec from consultation
			StormByte::Safe::Unique<StormByte::Multimedia::Pipeline::Config::Base> Config;	///< Track intention
			StormByte::Safe::Vector<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::FFmpeg>> Filters;	///< Stretch leaves
			bool Settled = false;						///< OnSettled already fired
	};
	}

	/**
	 * @brief Registers provider-owned slot state for Base Safe::Vector storage.
	 * @note Config copies clone through Multimedia; codec is a borrowed registry pointer.
	 */
	STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Backend::Pipeline::TranscoderSlot);

	/**
	 * @namespace StormByte::Multimedia::Backend::Pipeline
	 * @brief Multimedia-owned pipeline stages and unit holders.
	 *
	 * @ingroup multimedia_pipeline
	 */
	namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Transcoder
	 * @brief Runs one reader-to-writer job for the public Transcoder facade.
	 *
	 * Forwards the Demuxer @ref StormByte::Multimedia::Pipeline::Progress.
	 * Does not keep a second percent counter. Muxer is constructed with
	 * the shared log only. Destination container comes from the Plan writer.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Transcoder {
		public:
			/**
			 * @brief Idle coordinator.
			 */
			Transcoder() noexcept;

			~Transcoder() noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source coordinator.
			 */
			Transcoder(const Transcoder& other) = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Source coordinator.
			 * @return *this.
			 */
			Transcoder& operator=(const Transcoder& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Coordinator to take.
			 */
			Transcoder(Transcoder&& other) noexcept = delete;

			/**
			 * @brief Move assignment.
			 * @param other Coordinator to take.
			 * @return *this.
			 */
			Transcoder& operator=(Transcoder&& other) noexcept = delete;

			/**
			 * @brief Starts the job thread if it is not already running.
			 * @param job Public facade.
			 */
			void Start(StormByte::Multimedia::Pipeline::Transcoder& job) noexcept;

			/**
			 * @brief Requests abort and wakes a paused worker.
			 */
			void RequestCancel() noexcept;

			/**
			 * @brief Joins the worker if it is joinable.
			 */
			void Join() noexcept;

			/**
			 * @brief Blocks while the job is paused.
			 */
			void WaitIfPaused() noexcept;

			mutable StormByte::Safe::Mutex Lock;	///< Status / Error
			StormByte::Safe::Mutex PauseMutex;		///< PauseCv
			StormByte::Safe::ConditionVariable PauseCv;	///< Pause waiters
			StormByte::Safe::Atomic<StormByte::Multimedia::Pipeline::Status> Status {
				StormByte::Multimedia::Pipeline::Status::Stopped
			};											///< Public job lifecycle
			StormByte::Safe::Atomic<bool> Cancel { false };	///< Cancel requested
			StormByte::Safe::Atomic<bool> Paused { false };	///< Coordinator is paused
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Progress> Clock;	///< Demuxer clock
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::JobTelemetry> Metrics;	///< Retained stage and process metrics
			StormByte::Safe::Optional<StormByte::Safe::String> Error;			///< Failure text
			StormByte::Safe::Vector<TranscoderSlot> Mapped;	///< Fluent map, mux order
			StormByte::Safe::Vector<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::FFmpeg>> Analytics;	///< Global analytics
			StormByte::Safe::Vector<StormByte::Safe::Pair<StormByte::Safe::String, StormByte::Multimedia::Pipeline::Filter::Report>> Reports;	///< Snapshots at Done

		private:
			/**
			 * @brief Builds the Plan, wires the tube and watches the job.
			 * @param job Public facade.
			 * @param token Stop token of the worker.
			 */
			void Run(StormByte::Multimedia::Pipeline::Transcoder& job) noexcept;

			/**
			 * @brief Forwards analytics idle and fires measure / analytics / progress hooks once.
			 * @param job Public facade.
			 * @param graph Wired filters.
			 */
			void TickHooks(StormByte::Multimedia::Pipeline::Transcoder& job,
				StormByte::Multimedia::Pipeline::Filters& graph) noexcept;

			StormByte::Safe::Thread m_worker;		///< Coordinator thread
			bool m_measureHook = false;					///< OnMeasureDone already fired
			bool m_analyticsHook = false;				///< OnAnalyticsDone already fired
	};
}
