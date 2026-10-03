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

#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace StormByte::Multimedia::Backend::Pipeline {
	class Transcoder;
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
	class Route;

	/**
	 * @class Filters
	 * @brief Optional facade: Between stretches, Add filters, Close.
	 *
	 * A tube without filters does not need this. Ends are
	 * @c std::shared_ptr<Step>. operator>> still wires Plan and
	 * reserves (demuxer >> decoder, encoder >> muxer). Close
	 * wires each stretch (process chain, CloneTo, dest look).
	 *
	 * Global @ref Add is one analytics node shared by every
	 * matching Between (CloneTo, not N Launch). Per-stretch Add
	 * is @ref Handle::Add. Both are allowed; there is no dedup.
	 *
	 * A @ref Filter::ProcessTwoPasses leaf on a stretch puts that
	 * track on Demuxer::Measure when @ref Close runs. Remux plus
	 * that leaf is Fail on @ref Handle::Add. Demuxer measure EoF
	 * calls @ref CloseMeasureSource. Each measure-track Decoder
	 * drains on its worker, then @ref OnMeasureDrained. Each
	 * ProcessTwoPasses leaf drains on its worker, then
	 * @ref OnMeasureFilterDrained. @ref FinishMeasure runs on the
	 * two-pass filter worker when both sets are drained:
	 * LeaveMeasure and Rewind. After that the tube is ordinary
	 * Process, same as a job that never measured.
	 *
	 * Shares the Demuxer’s @ref Progress. Analytics leaves set
	 * HasAnalytics. Dest-look Decoders feed the analytics axis.
	 * Measure pts is written after ProcessTwoPasses::Measure in
	 * FFmpeg::Work, not when the Demuxer emits. Leaves never see
	 * Progress. Idle after the job sets AnalyticsDone.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Filters {
		friend class Decoder;
		friend class Demuxer;
		friend class Filter::FFmpeg;
		friend class Filter::ProcessTwoPasses;
		friend class StormByte::Multimedia::Backend::Pipeline::Transcoder;

		public:
			class Handle;

			/**
			 * @brief Empty facade. No stretches, no leaves.
			 */
			Filters() noexcept;

			/**
			 * @brief Copy is not allowed. The tube owns the facade.
			 */
			Filters(const Filters&) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the facade.
			 */
			Filters(Filters&&) noexcept = delete;

			/**
			 * @brief Drops stretches and attached leaves. Does not Halt Steps.
			 */
			~Filters() noexcept;

			/**
			 * @brief Copy assignment is not allowed.
			 */
			Filters& operator=(const Filters&) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 */
			Filters& operator=(Filters&&) noexcept = delete;

			/**
			 * @brief Stretch from @p origin to @p destination.
			 * @param origin Decoder / Demuxer / Encoder.
			 * @param destination Encoder / Remuxer / Muxer.
			 * @return @ref Handle for per-stretch Add.
			 *
			 * Hopper key: Decoder::Index, else Remuxer::In / Encoder::Index
			 * of dest, else origin Encoder/Remuxer. Fail dest if unknown.
			 */
			Handle Between(std::shared_ptr<Step> origin,
				std::shared_ptr<Step> destination) noexcept;

			/**
			 * @brief Global analytics. One node, every matching stretch.
			 * @param filter Leaf to attach. Must be Analytics.
			 * @return This facade.
			 *
			 * Process / Packet / ProcessTwoPasses leaves Fail: they
			 * go on @ref Handle::Add.
			 */
			Filters& Add(std::shared_ptr<Filter::FFmpeg> filter) noexcept;

			/**
			 * @brief Constructs a global analytics leaf and attaches it.
			 * @tparam T Analytics type.
			 * @tparam Args Constructor arguments after the implicit new.
			 * @param args Forwarded to T.
			 * @return This facade.
			 */
			template<typename T, typename... Args>
			Filters& Add(Args&&... args) noexcept {
				return Add(std::shared_ptr<Filter::FFmpeg>(
					std::make_shared<T>(std::forward<Args>(args)...)));
			}

			/**
			 * @brief Wires every stretch.
			 *
			 * Process chain, CloneTo, dest look. If any stretch has a
			 * @ref Filter::ProcessTwoPasses leaf, calls EnterMeasure
			 * on those leaves and Demuxer::Measure with their tracks.
			 * Hoppers stay open. Demuxer measure EoF then
			 * CloseMeasureSource; decode and two-pass workers drain;
			 * FinishMeasure Rewinds and Process continues.
			 */
			void Close() noexcept;

			/**
			 * @brief Whether every mounted leaf is idle.
			 * @return true when no leaf is still working.
			 */
			bool Idle() const noexcept;

			/**
			 * @brief Reports from attached leaves, in mount order.
			 * @return Name / report pairs. Empty reports are omitted.
			 */
			std::vector<std::pair<std::string, Filter::Report>> Reports() const noexcept;

			/**
			 * @brief Telemetry handles for every attached filter.
			 * @return Name / metrics pairs in mount order.
			 */
			std::vector<std::pair<std::string, std::shared_ptr<const StageTelemetry>>>
				StageTelemetries() const noexcept;

			/**
			 * @class Handle
			 * @brief Per-stretch Add returned by @ref Between.
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Handle {
				public:
					/**
					 * @brief Mounts a leaf on this stretch only.
					 * @param filter Process, ProcessTwoPasses, Packet or Analytics.
					 * @return This handle.
					 *
					 * ProcessTwoPasses on a remux destination Fails the dest.
					 */
					Handle& Add(std::shared_ptr<Filter::FFmpeg> filter) noexcept;

					/**
					 * @brief Constructs a leaf and mounts it on this stretch.
					 * @tparam T Filter type.
					 * @tparam Args Constructor arguments after the implicit new.
					 * @param args Forwarded to T.
					 * @return This handle.
					 */
					template<typename T, typename... Args>
					Handle& Add(Args&&... args) noexcept {
						return Add(std::shared_ptr<Filter::FFmpeg>(
							std::make_shared<T>(std::forward<Args>(args)...)));
					}

				private:
					friend class Filters;

					/**
					 * @brief Bound to @p owner stretch @p index.
					 * @param owner Facade that created this handle.
					 * @param index Index into m_stretches.
					 */
					Handle(Filters& owner, std::size_t index) noexcept;

					Filters* m_owner;		///< Facade
					std::size_t m_index;	///< Stretch index
			};

		private:
			/**
			 * @brief Whether a measure pass started by @ref Close is active.
			 * @return true until FinishMeasure, otherwise false.
			 */
			bool Measuring() const noexcept;

			/**
			 * @brief Decoders and two-pass leaves have both drained.
			 * @return true when FinishMeasure may run on the filter worker.
			 *
			 * Friend: Filter::FFmpeg::Wait. Leaves never call this.
			 */
			bool MeasureReadyToFinish() const noexcept;

			/**
			 * @brief Measure origin EoF. Friend: Demuxer::ReachedEof.
			 *
			 * Calls Decoder::MeasureSourceClosed on each measure-track
			 * decoder and ProcessTwoPasses::MeasureSourceClosed on each
			 * two-pass leaf. Does not LeaveMeasure.
			 */
			void CloseMeasureSource() noexcept;

			/**
			 * @brief One measure-track decoder finished DrainMeasure.
			 * @param track Origin index of that decoder.
			 *
			 * Friend: Decoder. Records the track and wakes two-pass
			 * leaves. Does not FinishMeasure.
			 */
			void OnMeasureDrained(int track) noexcept;

			/**
			 * @brief One ProcessTwoPasses leaf finished its measure hopper.
			 *
			 * Friend: ProcessTwoPasses. May FinishMeasure on this
			 * filter worker.
			 */
			void OnMeasureFilterDrained() noexcept;

			/**
			 * @brief FinishMeasure when decoders and two-pass leaves are drained.
			 *
			 * Friend: Filter::FFmpeg::Wait. Must run on a two-pass
			 * filter worker, not on a decoder worker.
			 */
			void MaybeFinishMeasure() noexcept;

			/**
			 * @brief Ends the measure pass.
			 *
			 * LeaveMeasure on each ProcessTwoPasses leaf (Eof then
			 * Measured) and Demuxer::Rewind. Process continues on
			 * the same tube. Decoder reset already ran on the
			 * decode worker.
			 */
			void FinishMeasure() noexcept;

			/**
			 * @brief Binds the Demuxer clock if found among stretches.
			 */
			void BindClock() noexcept;

			/**
			 * @brief Marks analytics complete on the shared clock if Idle.
			 *
			 * Friend: Backend::Pipeline::Transcoder. Not a user hook.
			 */
			void ClockAnalytics() noexcept;

			/**
			 * @brief Dest-look pts for the analytics axis.
			 * @param ns Presentation time of a dest-look frame.
			 *
			 * Friend: Decoder. Leaves never call this.
			 */
			void NoteAnalytics(std::int64_t ns) noexcept;

			/**
			 * @brief Pts of a frame that just ran Measure.
			 * @param ns Presentation time of that frame.
			 *
			 * Friend: Filter::FFmpeg::Work. Leaves never call this.
			 */
			void NoteMeasure(std::int64_t ns) noexcept;

			/**
			 * @brief One origin / destination pair and its Route.
			 */
			struct Stretch {
				std::shared_ptr<Step> Origin;			///< Decoder / Demuxer / Encoder
				std::shared_ptr<Step> Destination;		///< Encoder / Remuxer / Muxer
				int Track = -1;							///< Hopper key
				std::unique_ptr<Route> Lane;			///< Wired chain
				std::optional<int> Scope;				///< Optional track scope
			};

			/**
			 * @brief A mounted leaf and the track it is bound to.
			 */
			struct Attached {
				std::shared_ptr<Filter::FFmpeg> Filter;	///< Leaf
				std::optional<int> Track;				///< Stretch track, or none if global
			};

			std::vector<Stretch> m_stretches;			///< Between() order
			std::vector<Attached> m_globals;			///< Global analytics
			std::vector<Attached> m_reports;			///< Leaves that may Report()
			std::vector<int> m_measureTracks;			///< Tracks given to Demuxer::Measure
			std::vector<int> m_measureDrained;			///< Tracks that finished DrainMeasure
			std::size_t m_measureFilterCount = 0;		///< ProcessTwoPasses leaves in this pass
			std::size_t m_measureFiltersDrained = 0;	///< Those leaves that finished Measure
			bool m_measuring = false;					///< After Close, before FinishMeasure
			bool m_hasAnalytics = false;				///< At least one Analytics leaf
			std::shared_ptr<class Progress> m_progress;	///< Shared tube clock
	};
}
