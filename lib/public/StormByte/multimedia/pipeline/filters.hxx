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
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pair.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @brief Backend coordinator for transcoding jobs.
	 */
	class Transcoder;
}

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Pipeline Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @brief Decoding stage for compressed access units.
			 */
			class Decoder;

			/**
			 * @brief Demultiplexing stage for input containers.
			 */
			class Demuxer;

			/**
			 * @brief Connection between pipeline stages.
			 */
			class Route;
		}
	}
}

/**
 * @namespace StormByte::Multimedia::Pipeline::Detail
 * @brief Internal route and filter binding records.
 */
namespace StormByte::Multimedia::Pipeline::Detail {
	/**
	 * @struct FiltersStretch
	 * @brief Provider-owned route binding stored by the Filters facade.
	 */
	struct FiltersStretch {
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Step> Origin;		///< Decoder / Demuxer / Encoder.
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Step> Destination;	///< Encoder / Remuxer / Muxer.
		int Track = -1;														///< Hopper key.
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Route> Lane;		///< Wired chain.
		StormByte::Safe::Optional<int> Scope;									///< Optional track scope.
	};

	/**
	 * @struct FiltersAttached
	 * @brief Provider-owned filter and optional track binding.
	 */
	struct FiltersAttached {
		StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::FFmpeg> Filter;	///< Leaf.
		StormByte::Safe::Optional<int> Track;										///< Stretch track, or none if global.
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Detail::FiltersStretch);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Detail::FiltersAttached);

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Pipeline
		 * @brief Pipeline Demux / decode / filter / encode / mux types.
		 *
		 * @ingroup multimedia_pipeline
		 */
		namespace Pipeline {
			/**
			 * @class Filters
			 * @brief Optional facade: Between stretches, Add filters, Close.
			 *
			 * Use @ref Between to select connected stages, @ref Handle::Add
			 * to add filters to that connection, and @ref Close to complete
			 * configuration. Pipelines without filters do not need this facade.
			 *
			 * Global @ref Add applies one analytics filter to every matching
			 * connection. Global and per-connection filters may coexist;
			 * duplicate additions are not removed.
			 *
			 * @ref Filter::ProcessTwoPasses filters measure the required source
			 * tracks before the output pass. Two-pass processing is not supported
			 * on remux connections. The source is reread after measurement.
			 *
			 * Analytics can inspect decoded destination pictures as well as
			 * source pictures. The shared job progress includes measurement
			 * and analytics completion. @ref Reports returns analytics snapshots.
			 *
			 * @ingroup multimedia_pipeline
			 */
			class STORMBYTE_MULTIMEDIA_PUBLIC Filters {
				public:
					/**
					 * @brief Per-stretch filter attachment handle.
					 */
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
					 * The stages must identify the track being connected. An
					 * unidentifiable track causes the destination stage to fail.
					 */
					Handle Between(StormByte::Safe::Shared<Step> origin,
						StormByte::Safe::Shared<Step> destination) noexcept;

					/**
					 * @brief Global analytics. One node, every matching stretch.
					 * @param filter Leaf to attach. Must be Analytics.
					 * @return This facade.
					 *
					 * Process / Packet / ProcessTwoPasses leaves Fail: they
					 * go on @ref Handle::Add.
					 */
					Filters& Add(StormByte::Safe::Shared<Filter::FFmpeg> filter) noexcept;

					/**
					 * @brief Constructs a global analytics leaf and attaches it.
					 * @tparam T Analytics type.
					 * @tparam Args Constructor arguments after the implicit new.
					 * @param args Forwarded to T.
					 * @return This facade.
					 */
					template<typename T, typename... Args>
					Filters& Add(Args&&... args) noexcept {
						return Add(StormByte::Safe::Shared<Filter::FFmpeg>::MakePointer<T>(
							std::forward<Args>(args)...));
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
					StormByte::Safe::Vector<StormByte::Safe::Pair<StormByte::Safe::String, Filter::Report>>
						Reports() const noexcept;

					/**
					 * @brief Telemetry handles for every attached filter.
					 * @return Named stage metrics in mount order.
					 */
					StormByte::Safe::Vector<TelemetryStage> StageTelemetries() const noexcept;

					/**
					 * @class Handle
					 * @brief Per-stretch Add returned by @ref Between.
					 *
					 * Borrows its facade, which must outlive the handle and all
					 * calls through it. Copying a handle does not extend that lifetime.
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
							Handle& Add(StormByte::Safe::Shared<Filter::FFmpeg> filter) noexcept;

							/**
							 * @brief Constructs a leaf and mounts it on this stretch.
							 * @tparam T Filter type.
							 * @tparam Args Constructor arguments after the implicit new.
							 * @param args Forwarded to T.
							 * @return This handle.
							 */
							template<typename T, typename... Args>
							Handle& Add(Args&&... args) noexcept {
								return Add(StormByte::Safe::Shared<Filter::FFmpeg>::MakePointer<T>(
									std::forward<Args>(args)...));
							}

						private:
							/**
							 * @brief Allows the facade to construct handles for its stretches.
							 */
							friend class Filters;

							/**
							 * @brief Bound to @p owner stretch @p index.
							 * @param owner Facade that created this handle.
							 * @param index Index into m_stretches.
							 */
							Handle(Filters& owner, StormByte::Size index) noexcept;

							Filters* m_owner;	///< Borrowed facade; must outlive this handle.
							StormByte::Size m_index;	///< Stretch index; owns no storage.
					};

				private:
					/**
					 * @brief Allows the decoder to report measurement and analytics progress.
					 */
					friend class Decoder;

					/**
					 * @brief Allows the demuxer to notify the facade of measure-source completion.
					 */
					friend class Demuxer;

					/**
					 * @brief Allows filter workers to coordinate measurement completion.
					 */
					friend class Filter::FFmpeg;

					/**
					 * @brief Allows two-pass leaves to report measurement completion.
					 */
					friend class Filter::ProcessTwoPasses;

					/**
					 * @brief Allows the backend coordinator to update analytics completion.
					 */
					friend class StormByte::Multimedia::Backend::Pipeline::Transcoder;

					/**
					 * @typedef Stretch
					 * @brief Route-binding record stored in the facade.
					 */
					using Stretch = Detail::FiltersStretch;

					/**
					 * @typedef Attached
					 * @brief Analytics/report binding stored in the facade.
					 */
					using Attached = Detail::FiltersAttached;

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

					StormByte::Safe::Vector<Stretch> m_stretches;		///< Between() order.
					StormByte::Safe::Vector<Attached> m_globals;		///< Global analytics.
					StormByte::Safe::Vector<Attached> m_reports;		///< Leaves that may Report().
					StormByte::Safe::Vector<int> m_measureTracks;		///< Tracks given to Demuxer::Measure.
					StormByte::Safe::Vector<int> m_measureDrained;		///< Tracks that finished DrainMeasure
					StormByte::Size m_measureFilterCount = 0;			///< ProcessTwoPasses leaves in this pass
					StormByte::Size m_measureFiltersDrained = 0;			///< Those leaves that finished Measure
					bool m_measuring = false;						///< After Close, before FinishMeasure
					bool m_hasAnalytics = false;						///< At least one Analytics leaf
					StormByte::Safe::Shared<class Progress> m_progress;	///< Shared tube clock
			};
		}
	}
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filters::Handle);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filters);
