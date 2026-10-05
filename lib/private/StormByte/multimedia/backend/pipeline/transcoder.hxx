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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

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
			int In = -1;								///< Origin stream index
			int Out = -1;								///< Mux destination order
			StormByte::Multimedia::Type Kind = StormByte::Multimedia::Type::Unknown;	///< Media kind
			const StormByte::Multimedia::Codec* Source = nullptr;	///< Origin codec from consultation
			StormByte::Safe::Unique<StormByte::Multimedia::Pipeline::Config::Base> Config;	///< Track intention
			std::vector<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::FFmpeg>> Filters;	///< Stretch leaves
			bool Settled = false;						///< OnSettled already fired
	};

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

			/**
			 * @brief Requests stop and joins the worker.
			 */
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

			mutable std::mutex Lock;					///< Status / Error
			std::mutex PauseMutex;						///< PauseCv
			std::condition_variable PauseCv;			///< Pause waiters
			std::atomic<StormByte::Multimedia::Pipeline::Status> Status {
				StormByte::Multimedia::Pipeline::Status::Stopped
			};											///< Public job lifecycle
			std::atomic<bool> Cancel { false };			///< Cancel requested
			std::atomic<bool> Paused { false };			///< Coordinator is paused
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Progress> Clock;	///< Demuxer clock
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::JobTelemetry> Metrics;	///< Retained stage and process metrics
			StormByte::Safe::Optional<StormByte::Safe::String> Error;			///< Failure text
			std::vector<TranscoderSlot> Mapped;			///< Fluent map, mux order
			std::vector<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::FFmpeg>> Analytics;	///< Global analytics
			StormByte::Safe::Vector<StormByte::Safe::Pair<StormByte::Safe::String, StormByte::Multimedia::Pipeline::Filter::Report>> Reports;	///< Snapshots at Done

		private:
			/**
			 * @brief Builds the Plan, wires the tube and watches the job.
			 * @param job Public facade.
			 * @param token Stop token of the worker.
			 */
			void Run(StormByte::Multimedia::Pipeline::Transcoder& job, std::stop_token token) noexcept;

			/**
			 * @brief Forwards analytics idle and fires measure / analytics / progress hooks once.
			 * @param job Public facade.
			 * @param graph Wired filters.
			 */
			void TickHooks(StormByte::Multimedia::Pipeline::Transcoder& job,
				StormByte::Multimedia::Pipeline::Filters& graph) noexcept;

			std::jthread m_worker;						///< Coordinator thread
			bool m_measureHook = false;					///< OnMeasureDone already fired
			bool m_analyticsHook = false;				///< OnAnalyticsDone already fired
	};
}
