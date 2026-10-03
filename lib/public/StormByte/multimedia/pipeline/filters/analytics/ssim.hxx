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

#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video leaves (Process and Analytics).
 *
 * Inherit @ref Filter::Process to rewrite frames, or
 * @ref Filter::Analytics to observe them. Do not inherit
 * @ref Filter::FFmpeg. Attach with @c job.Filter<SSIM>(log).
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class SSIM
	 * @brief Full-reference SSIM. Analytics leaf.
	 *
	 * @par What an Analytics leaf is
	 * Analytics is not on the encode path. @ref Process sees
	 * only @ref Pipeline::Frame. Reference
	 * @ref Item::Producer is @ref Producer::Decoder.
	 * Distorted Producer is @ref Producer::Encoder or
	 * @ref Producer::Remuxer. There is no Packet and no
	 * @ref FFmpeg::Save.
	 *
	 * Implement @ref Setup, @ref Process, @ref Eof, @ref Clean,
	 * @ref Report, @ref Media.
	 *
	 * @par Pairing
	 * Frames are paired in presentation order per
	 * @ref Pipeline::Frame::Track, not by Serial or PTS. The nth
	 * reference frame is compared with the nth distorted frame
	 * of that track. There is no pooled mean across tracks.
	 *
	 * @par Metric
	 * 8×8 windows, Wang et al. constants scaled to the latched
	 * peak (@c (1 << bpc) - 1). Every plane the layout exposes
	 * is scored. @ref Report publishes @c ssim_y / @c ssim_u /
	 * @c ssim_v when that plane exists, plus @c ssim_mean
	 * (sample-weighted over scored planes) and @c ssim_min
	 * (minimum per-frame average). Range is [0, 1].
	 *
	 * @par Geometry
	 * Latched on the first valid reference. Distorted looks
	 * of another size are scaled to the reference dimensions.
	 * A later reference that changes
	 * width/height is skipped with a Warning. Scale is not
	 * @ref Report::Failed.
	 *
	 * @par Memory
	 * Unpaired frames may be retained to accommodate encoder delay.
	 * @ref Eof warns about unmatched frames. @ref InputCeiling (512)
	 * limits pending input, not frames awaiting a matching look.
	 *
	 * @par When to read the report
	 * Wait until the job is Done. A low score is not Fail.
	 * @ref Report::Failed only when no pair was scored. One
	 * track keeps flat keys (`ssim_mean`). Several tracks
	 * prefix with the origin index (`0.ssim_mean`).
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Analytics
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::PSNR
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC SSIM: public Filter::Analytics {
		public:
			/**
			 * @brief SSIM analytics leaf.
			 * @param log Shared logger. Empty pointer means no log.
			 */
			explicit SSIM(std::shared_ptr<StormByte::Logger::Log> log) noexcept;

			/**
			 * @brief Copy is not allowed. Each leaf owns parked looks.
			 */
			SSIM(const SSIM& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 */
			SSIM(SSIM&& other) noexcept = delete;

			/**
			 * @brief Destructor. Drops parked looks.
			 */
			~SSIM() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 */
			SSIM& operator=(const SSIM& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 */
			SSIM& operator=(SSIM&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video. Other kinds are ignored in Process.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Ceiling of the analytics input hopper.
			 * @return Max items in the hopper. Never 0.
			 */
			std::size_t InputCeiling() const noexcept override {
				return Ceiling;
			}

			/**
			 * @brief Drops per-run state. First-run no-op is allowed.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Acquires per-run state. Calls @ref Clean first.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Parks a look and scores when a pair is ready.
			 * @param frame Unit in the tube. Never null.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Scores leftover pairs and freezes @ref Report.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Pooled SSIM after EoF.
			 * @return Ok with mean/min/planes/frames, or Failed.
			 *
			 * One scored track: `ssim_mean` / `ssim_min` /
			 * `ssim_y` / `ssim_u` / `ssim_v` / `frames`.
			 * Several: `0.ssim_mean` (origin index).
			 * Missing chroma planes omit `ssim_u` / `ssim_v`.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			/**
			 * @brief Running SSIM for one plane of one track.
			 */
			struct Plane {
				double sum = 0.0;			///< Sum of per-frame SSIM
				unsigned frames = 0;		///< Frames that contributed
			};

			/**
			 * @brief Per-track presentation park and accumulators.
			 */
			struct Lane {
				std::deque<StormByte::Multimedia::FFmpeg::AVFrame> ref;		///< Decoder looks
				std::deque<StormByte::Multimedia::FFmpeg::AVFrame> dist;	///< Dest looks
				int width = 0;												///< Latched width
				int height = 0;												///< Latched height
				int bpc = 0;												///< Latched bits per component
				unsigned scored = 0;										///< Accepted pairs
				std::size_t peakRef = 0;									///< Peak parked refs
				std::size_t peakDist = 0;									///< Peak parked dists
				Plane y;													///< Luma
				Plane u;													///< Cb, if the layout has it
				Plane v;													///< Cr, if the layout has it
				std::optional<double> mean;									///< Sample-weighted mean
				std::optional<double> min;									///< Minimum per-frame average
				bool failed = false;										///< No pair scored at EoF
			};

			/**
			 * @brief Scores while both presentation FIFOs of @p lane have a frame.
			 * @param lane Track context.
			 */
			void Drain(Lane& lane) noexcept;

			/**
			 * @brief Accumulates SSIM for one presentation pair.
			 * @param lane Track context.
			 * @param ref Decoder tap look.
			 * @param dist Dest look, already scaled to the latch if needed.
			 */
			void Score(Lane& lane,
				const StormByte::Multimedia::FFmpeg::AVFrame& ref,
				const StormByte::Multimedia::FFmpeg::AVFrame& dist) noexcept;

			/**
			 * @brief Drops parked RAII clones of @p lane.
			 * @param lane Track context.
			 */
			void DropParked(Lane& lane) noexcept;

			/**
			 * @brief Drops parked looks of every lane.
			 */
			void DropAll() noexcept;

			static constexpr std::size_t Ceiling = 512;	///< Analytics hopper
			static constexpr int Window = 8;			///< SSIM window edge
			std::map<int, Lane> m_lanes;				///< One park per Frame::Track
	};
}
