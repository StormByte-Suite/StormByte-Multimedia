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

#include <StormByte/safe/map.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <cstddef>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video::Detail::XPSNR
 * @brief Provider-owned XPSNR implementation state.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video::Detail::XPSNR {
	/**
	 * @brief Opaque per-track presentation park and accumulators.
	 */
	struct Lane;
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Filter::Video::Detail::XPSNR::Lane>);

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video leaves (Process and Analytics).
 *
 * Inherit @ref Filter::Process to rewrite frames, or
 * @ref Filter::Analytics to observe them. Do not inherit
 * @ref Filter::FFmpeg. Attach with @c job.Filter<XPSNR>(log).
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class XPSNR
	 * @brief Full-reference XPSNR (activity-weighted PSNR). Analytics leaf.
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
	 * Block MSE weighted by a simple spatial-activity map on the
	 * reference (sum of absolute 1-pixel gradients in 16×16
	 * blocks). High-detail blocks dominate; flat areas do not
	 * inflate the score. Peak is @c (1 << bpc) - 1. Every plane
	 * the layout exposes is scored. @ref Report publishes
	 * @c xpsnr_y / @c xpsnr_u / @c xpsnr_v when that plane exists,
	 * plus @c xpsnr_mean and @c xpsnr_min. Identical frames
	 * report a finite cap of 100.0, not +inf.
	 *
	 * This is the same *class* of metric as FFmpeg's @c xpsnr.
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
	 * track keeps flat keys (`xpsnr_mean`). Several tracks
	 * prefix with the origin index (`0.xpsnr_mean`).
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Analytics
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::PSNR
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC XPSNR: public Filter::Analytics {
		public:
			/**
			 * @brief XPSNR analytics leaf.
			 * @param log Shared logger. Empty pointer means no log.
			 */
			explicit XPSNR(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept;

			/**
			 * @brief Copy is not allowed. Each leaf owns parked looks.
			 */
			XPSNR(const XPSNR& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 */
			XPSNR(XPSNR&& other) noexcept = delete;

			/**
			 * @brief Destructor. Drops parked looks.
			 */
			~XPSNR() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 */
			XPSNR& operator=(const XPSNR& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 */
			XPSNR& operator=(XPSNR&& other) noexcept = delete;

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
			 * @brief Pooled XPSNR after EoF.
			 * @return Ok with mean/min/planes/frames, or Failed.
			 *
			 * One scored track: `xpsnr_mean` / `xpsnr_min` /
			 * `xpsnr_y` / `xpsnr_u` / `xpsnr_v` / `frames`.
			 * Several: `0.xpsnr_mean` (origin index).
			 * Missing chroma planes omit `xpsnr_u` / `xpsnr_v`.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			/**
			 * @brief Per-track presentation park and accumulators.
			 */
			using Lane = Detail::XPSNR::Lane;

			/**
			 * @brief Scores while both presentation FIFOs of @p lane have a frame.
			 * @param lane Track context.
			 */
			void Drain(Lane& lane) noexcept;

			/**
			 * @brief Accumulates XPSNR for one presentation pair.
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
			static constexpr int Block = 16;			///< Activity block edge
			static constexpr double Cap = 100.0;		///< Finite score when MSE is 0
			StormByte::Safe::Map<int, StormByte::Safe::Shared<Lane>> m_lanes;	///< One park per Frame::Track
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::XPSNR);
