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

#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

/** @brief Internal pipeline coordinator types. */
namespace StormByte::Multimedia::Backend::Pipeline {
	/** @brief Internal job coordinator sharing the progress clock. */
	class Transcoder;
}

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	class Demuxer;
	class Filters;
	class Muxer;
	class Transcoder;

	/**
	 * @class Progress
	 * @brief Job clock for one tube.
	 *
	 * Tracks duration resolution, measurement, analytics and output completion
	 * for one job. Standalone Demuxers also provide a progress handle.
	 * @ref Transcoder::Progress and @ref Demuxer::Progress return
	 * @ref Pointer (shared, const). The user may keep that pointer
	 * after the tube dies. There are no public setters.
	 *
	 * Duration calculation is an exclusive preliminary phase. Preparation
	 * exposes activity without a percentage; packet scanning adds its own
	 * processed-packet position estimate. It is not part of the weighted processing score.
	 * Snapshot captures a consistent phase and values for GUI consumers.
	 * Public processing axes are measure (optional 2-pass) and analytics
	 * (optional taps). Ordinary Process has no public name; its
	 * score only enters @ref All.
	 *
	 * @ref All is a weighted sum of the mounted axes. Measure, when
	 * present, is 5 percent. Analytics, when present, is 10 percent
	 * and runs in parallel with Process. Process/mux takes the rest
	 * (100, 95, 90 or 85 percent). Missing axes are not in the mix.
	 * 100.00 only when the Muxer finished and every mounted phase
	 * is closed. Measurement progress reflects completed frame measurement,
	 * not merely reading the source packets.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Progress {
		friend class Demuxer;
		friend class Filters;
		friend class Muxer;
		friend class Transcoder;
		friend class StormByte::Multimedia::Backend::Pipeline::Transcoder;

		public:
			/** @brief Exclusive phase currently presented to the user. */
			enum class Phase {
				CalculatingDuration, ///< Scanning source packet timestamps.
				Measure, ///< Running the optional first pass.
				Processing, ///< Processing, muxing, or draining analytics.
				Complete ///< Muxing and all mounted axes have finished.
			};

			/** @brief Consistent values captured under one clock lock. */
			struct Values {
				Phase Current; ///< Exclusive display phase.
				std::optional<double> Duration; ///< Duration scan percent; empty during CPU-only preparation.
				std::optional<double> Measure; ///< Mounted measure percent, hidden during duration.
				std::optional<double> Analytics; ///< Mounted analytics percent, hidden during duration.
				double Processing; ///< Ordinary processing percent; zero during duration calculation.
				double All; ///< Processing percent; zero during duration calculation.
				bool MeasureComplete; ///< Whether the optional measure axis is closed.
				bool AnalyticsComplete; ///< Whether the optional analytics axis is closed.
				bool ProcessingComplete; ///< Whether the source processing pass has reached EOF.
				bool MuxComplete; ///< Whether the destination trailer has been written.
			};

			/** @brief Capture phase and values atomically. @return Consistent clock values. */
			Values Snapshot() const noexcept;

			/** @brief Duration scan percent. @return 0..100 during byte scanning; empty during preparation or when inactive. */
			std::optional<double> DurationCalculation() const noexcept;

			/** @brief Whether the duration scan owns the status line. @return true while scanning. */
			bool CalculatingDuration() const noexcept;

			/** @brief Ordinary processing percent, independent of weighted All. @return 0..100; zero during duration. */
			double Processing() const noexcept;

			/** @brief Whether source processing reached EOF. @return true after PassDone. */
			bool ProcessingComplete() const noexcept;

			/** @brief Whether destination writing finished. @return true after MuxDone. */
			bool MuxComplete() const noexcept;

			/**
			 * @brief User-facing handle. Const, shared. Not a raw pointer.
			 */
			using Pointer = std::shared_ptr<const Progress>;

			/**
			 * @brief Empty clock. No measure, no analytics.
			 */
			Progress() noexcept = default;

			/**
			 * @brief Copy constructor.
			 * @param other Source clock.
			 */
			Progress(const Progress& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Clock to take.
			 */
			Progress(Progress&& other) noexcept = delete;

			/**
			 * @brief Destructor.
			 */
			~Progress() noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @param other Source clock.
			 * @return *this.
			 */
			Progress& operator=(const Progress& other) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Clock to take.
			 * @return *this.
			 */
			Progress& operator=(Progress&& other) noexcept = delete;

			/**
			 * @brief First-pass percent, if the tube mounted 2-pass.
			 * @return 0..100, or empty if unmounted or duration calculation is active.
			 */
			std::optional<double> Measure() const noexcept;

			/**
			 * @brief Dest-look percent, if analytics taps exist.
			 * @return 0..100, or empty if unmounted or duration calculation is active.
			 */
			std::optional<double> Analytics() const noexcept;

			/**
			 * @brief Whether this tube mounted a measure pass.
			 * @return true after Demuxer::Measure with a non-empty track list.
			 */
			bool HasMeasure() const noexcept;

			/**
			 * @brief Whether this tube mounted analytics taps.
			 * @return true after Filters wired a dest look.
			 */
			bool HasAnalytics() const noexcept;

			/**
			 * @brief Whether the measure pass is finished.
			 * @return true if there is no measure pass, or it has closed.
			 */
			bool MeasureComplete() const noexcept;

			/**
			 * @brief Whether analytics is finished.
			 * @return true if there are no taps, or they are idle.
			 */
			bool AnalyticsComplete() const noexcept;

			/**
			 * @brief Combined job percent.
			 *
			 * Weighted sum of mounted axes: measure 5 percent if
			 * the tube has a 2-pass leaf, analytics 10 percent if
			 * dest-look taps exist, Process/mux the remainder.
			 * Analytics stays in the sum while measure is live;
			 * the status line only hides the analytics label then.
			 * All is monotone and stays below 100 until the Muxer
			 * finished and every mounted phase is closed.
			 *
			 * @return 0..100; zero while duration calculation is active.
			 */
			double All() const noexcept;

			/**
			 * @brief One CR-safe line: live axes plus @ref All.
			 *
			 * An axis is omitted when it was not mounted or already
			 * finished. While measure is live the analytics label
			 * is omitted even if taps exist. No newline.
			 * Duration preparation shows its label and activity indicator. Once
			 * packet scanning starts, the indicator freezes and the percentage is added.
			 *
			 * @return Single line, no trailing newline.
			 */
			operator std::string() const noexcept;

		private:
			/** @brief Begin duration preparation before measurable packet scanning starts. */
			void BeginDurationCalculation() noexcept;

			/** @brief Update the exclusive duration phase. @param percent Scan estimate; empty closes the phase. */
			void SetDurationCalculation(std::optional<double> percent) noexcept;

			mutable std::recursive_mutex m_lock; ///< Protects all clock values and snapshots.
			bool m_calculatingDuration = false; ///< Duration preparation or byte scanning is active.
			mutable char m_durationIndicator = '|'; ///< Last preparation symbol, frozen while scanning.
			std::optional<double> m_durationCalculation; ///< Active monotone duration scan estimate.

			/**
			 * @brief Records that this tube has a measure pass.
			 * @param on true when Demuxer::Measure was called.
			 */
			void HasMeasure(bool on) noexcept;

			/**
			 * @brief Records that this tube has analytics taps.
			 * @param on true when Filters wired a dest look.
			 */
			void HasAnalytics(bool on) noexcept;

			/**
			 * @brief Origin duration used as ceiling for every axis.
			 * @param ns Duration in nanoseconds. Ignored if not > 0.
			 */
			void SetDurationNs(std::int64_t ns) noexcept;

			/**
			 * @brief Measure-pass position. Monotone, clamped to duration.
			 * @param ns Last measured frame Pts.
			 */
			void SetMeasureNs(std::int64_t ns) noexcept;

			/**
			 * @brief Ordinary Process-pass position. Monotone, clamped to duration.
			 * @param ns Last packet Pts while not measuring.
			 */
			void SetPassNs(std::int64_t ns) noexcept;

			/**
			 * @brief Analytics position. Monotone, clamped to duration.
			 * @param ns Dest-look Pts.
			 */
			void SetAnalyticsNs(std::int64_t ns) noexcept;

			/**
			 * @brief Measure pass closed. Freezes measure at duration.
			 */
			void MeasureDone() noexcept;

			/**
			 * @brief Ordinary Process pass hit source EoF. Freezes that Pts at duration.
			 */
			void PassDone() noexcept;

			/**
			 * @brief Muxer finished writing the destination.
			 */
			void MuxDone() noexcept;

			/**
			 * @brief Analytics taps idle. Freezes analytics at duration.
			 */
			void AnalyticsDone() noexcept;

			/**
			 * @brief Maps a position to 0..100 against @p dur.
			 * @param pos Position in nanoseconds.
			 * @param dur Duration in nanoseconds.
			 * @return 0 if unusable; 100 if @p pos >= @p dur.
			 */
			static double Axis(std::int64_t pos, std::int64_t dur) noexcept;

			bool m_hasMeasure = false;			///< Tube mounted a 2-pass leaf
			bool m_hasAnalytics = false;		///< Tube mounted dest-look taps
			bool m_measureDone = false;			///< Measure pass closed
			bool m_passDone = false;			///< Ordinary Process pass at source EoF
			bool m_muxDone = false;				///< Muxer wrote trailer
			bool m_analyticsDone = false;		///< Analytics taps idle
			std::int64_t m_durationNs = 0;		///< Origin duration (ns)
			std::int64_t m_measureNs = 0;		///< Measure position (ns)
			std::int64_t m_passNs = 0;			///< Ordinary Process position (ns)
			std::int64_t m_analyticsNs = 0;		///< Analytics position (ns)
			std::int64_t m_analyticsAtMux = 0;	///< Analytics ns when MuxDone ran
			mutable double m_all = 0.0;			///< Last published All (monotone)
	};
}
