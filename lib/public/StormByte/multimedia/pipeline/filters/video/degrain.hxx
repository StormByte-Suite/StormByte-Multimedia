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
#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {

	/**
	 * @class Degrain
	 * @brief Two-pass, regional film-grain reducer built on libavfilter
	 *        @c fftdnoiz. ProcessTwoPasses leaf. Not a wrapper and not a
	 *        copy of @ref Fftdnoiz.
	 *
	 * Attach with @c job.Video(in).Filter<Degrain>(log) or
	 * @c Filter<Degrain>(log, cap) to clamp every filtering strength.
	 *
	 * @par What it is for
	 * Experimental spatial regional multi-sigma denoising for film grain.
	 * The first pass measures regions across every frame. Flat noisy areas can receive
	 * stronger filtering than texture and skin-like colours in the same frame.
	 *
	 * Use it when you can see grain in mid-tones and crushed night, and
	 * you are willing to spend a full decode before encode. It is not
	 * a live filter and not a substitute for
	 * @ref StormByte::Multimedia::Pipeline::Filter::Audio::Afftdn (audio) or
	 * @ref Deband (contouring).
	 *
	 * @par What it is not
	 * Not @ref Fftdnoiz with a different name. @ref Fftdnoiz is one
	 * sigma for every frame. Degrain blends the original with bracketing
	 * filtered levels using continuous bilinear regional targets.
	 *
	 * @par When to attach
	 * After decode, before @ref Cas / @ref Scale / @ref Deband.
	 * After @ref Fieldmatch / @ref Decimate if the source was
	 * telecined. Do not put it on animation, screen content, or a
	 * master that is already clean. Darkness alone never forces filtering.
	 *
	 * @par Do not stack
	 * Exclusive with @ref Fftdnoiz, @ref Bm3d, @ref NlMeans,
	 * @ref VagueDenoiser, @ref Hqdn3d and a temporal
	 * @ref Atadenoise. Two grain leaves on the same video is how
	 * faces turn to clay. @ref Deband after Degrain is fine
	 * (bands, not grain).
	 *
	 * @par Two passes
	 * @ref Measure assesses grain across neighbouring pictures.
	 * Brightness discontinuities isolate measurement groups, including flashes.
	 * @ref Process applies per-frame regional strengths during the encode pass.
	 * Clean or inconclusive frames remain unchanged. Samples
	 * that cannot be converted for measurement are omitted.
	 * Missing or duplicate measurement timestamps always pass through.
	 *
	 * @par Strength selection
	 * Lighting-compensated high-pass temporal differences estimate residual
	 * noise robustly, rather than selecting the smallest neighbour difference.
	 * Insufficient evidence, motion and clean regions select zero. Texture
	 * and skin hints suppress strength; the caller cap is never a floor.
	 * Detail detection is not face recognition or motion compensation.
	 * Abrupt brightness changes are detected heuristically; equal-brightness
	 * cuts and correlated or compressed grain can remain ambiguous.
	 * Targets are expressed in 8-bit-equivalent sigma units; the default ceiling
	 * is 4.0. A zero ceiling disables filtering. Negative finite ceilings are
	 * treated as zero, and finite ceilings above 100 are limited to 100.
	 * Non-finite ceilings fail the filter rather than selecting a strength.
	 *
	 * @par Temporal baseline
	 * Denoising uses the previous real input and the current input, with a
	 * duplicate of current in place of an unavailable future picture. At detected
	 * discontinuities it uses current alone. This asymmetric temporal baseline
	 * preserves one output per input without delaying frames, but does not offer
	 * symmetric temporal denoising or motion compensation.
	 *
	 * @par Formats and preservation
	 * Processing supports software planar integer YUV and gray at 8, 10, 12 and
	 * 16 bits, in either byte order. Other layouts pass through unchanged.
	 * Output retains the original geometry, pixel format, timestamp, metadata
	 * and alpha. A frame whose targets are all zero is an exact passthrough.
	 *
	 * @par Cost
	 * Measurement requires a full decode and memory for at most
	 * 11 measurement frames plus compact per-frame measurements.
	 * The second pass evaluates multiple denoising strengths for each frame
	 * and blends them according to regional measurements. Processing is
	 * computationally expensive, especially at 4K. Each input frame produces
	 * one output frame with the same presentation timestamp.
	 * It is experimental: visual quality and scene detection are not guaranteed.
	 *
	 * @par Failure
	 * No confident regional evidence means Warning and passthrough. Graph /
	 * Filter errors Fail the leaf. Uncertainty in auto detect is
	 * a skip, never a blind sigma.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Fftdnoiz
	 * @see StormByte::Multimedia::Pipeline::Filter::ProcessTwoPasses
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Degrain: public Filter::ProcessTwoPasses {
		public:
			/**
			 * @brief Regional grain reduction with a conservative temporal baseline.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param sigmaCap Optional finite ceiling in 8-bit-equivalent sigma units.
			 *        Empty uses 4.0; zero disables filtering. Non-finite values fail
			 *        at setup/measurement. Finite values are clamped to [0, 100].
			 */
			Degrain(std::shared_ptr<StormByte::Logger::Log> log,
				std::optional<double> sigmaCap = {}) noexcept;

			Degrain(const Degrain& other) = delete;
			Degrain(Degrain&& other) noexcept = delete;

			/**
			 * @brief Releases measurement and processing resources.
			 */
			~Degrain() noexcept override;

			Degrain& operator=(const Degrain& other) = delete;
			Degrain& operator=(Degrain&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Discards measurements and resets denoising state.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Prepares denoising for a new pass.
			 */
			void Setup() noexcept override;

			/**
			 * @brief First pass: measures grain in the current picture.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Measure(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Applies regional denoising strengths, or leaves the frame unchanged.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Finalizes measurement; processing has no delayed outputs.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Reports regional-target status, frame counts, filtering and strength.
			 * @return Measurement and processing summary.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			/** @cond INTERNAL */
			/**
			 * @brief Horizontal region count.
			 */
			static constexpr int GridWidth = 12;

			/**
			 * @brief Vertical region count.
			 */
			static constexpr int GridHeight = 8;

			/**
			 * @brief Compact per-frame regional targets in 8-bit-equivalent sigma units.
			 */
			using RegionMap = std::array<float, GridWidth * GridHeight>;

			/**
			 * @brief Compact measurement retained after its pixels leave the ring.
			 */
			struct Row {
				int64_t pts = 0; ///< Original presentation timestamp.
				RegionMap regions{}; ///< Continuous regional targets; zero is no-op.
				std::size_t group = 0; ///< Brightness/geometry/timestamp continuity group.
				int width = 0; ///< Original measurement width.
				int height = 0; ///< Original measurement height.
				int format = 0; ///< Original pixel format.
				bool valid = true; ///< False for all occurrences of an ambiguous PTS.
			};

			/**
			 * @brief One of at most eleven measurement pictures.
			 */
			struct Slot {
				std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFrame> pic; ///< Software YUV420P measurement copy.
				RegionMap means{}; ///< Regional luma averages for lighting compensation.
				std::size_t row = 0; ///< Constant-time index into the compact rows.
				bool scored = false; ///< Prevents rescoring during EOF/boundary draining.
			};

			/**
			 * @brief Convert @p src to YUV420P, isolate boundaries and score the ring centre.
			 * @param src Live decoder frame. ScaleTo failure drops the sample.
			 */
			void PushFrame(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Fill one @ref Row from ring slot @p idx using up to five neighbours each side.
			 * @param idx Index in @ref m_ring. No-op if already scored or tiny.
			 */
			void ScoreCenter(std::size_t idx) noexcept;

			/**
			 * @brief Score every remaining slot so the last 5 frames are not silent.
			 */
			void FlushRing() noexcept;

			/**
			 * @brief Finalize targets, smooth only positive evidence within groups and report strengths.
			 */
			void Decide() noexcept;

			/**
			 * @brief Runs and validates a complete three-input temporal window.
			 * @param previous Previous real input, or current at a boundary.
			 * @param current Current real input.
			 * @param sigma Filter strength.
			 * @param output Receives the unique centre output.
			 * @return False on graph error, missing/duplicate PTS or geometry mismatch.
			 */
			bool FilterWindow(const StormByte::Multimedia::FFmpeg::AVFrame& previous,
				const StormByte::Multimedia::FFmpeg::AVFrame& current, double sigma,
				StormByte::Multimedia::FFmpeg::AVFrame& output) noexcept;

			/**
			 * @brief Processes one real input and remembers its unfiltered reference.
			 * @param source Current decoded frame.
			 * @param output Receives independently allocated blended planes and source properties.
			 * @return True only when output should replace source; false is passthrough or failure.
			 */
			bool Apply(const StormByte::Multimedia::FFmpeg::AVFrame& source,
				StormByte::Multimedia::FFmpeg::AVFrame& output) noexcept;

			/**
			 * @brief Interpolates targets at normalized plane coordinates.
			 * @param regions Region-centred target map.
			 * @param horizontal Horizontal position in the unit interval.
			 * @param vertical Vertical position in the unit interval.
			 * @return Clamped bilinear target in 8-bit-equivalent sigma units.
			 */
			static double RegionTarget(const RegionMap& regions, double horizontal,
				double vertical) noexcept;

			std::optional<double> m_capIn; ///< Caller ceiling, or empty for 4.0.
			std::deque<Slot> m_ring; ///< Bounded measurement pixel storage.
			std::vector<Row> m_row; ///< One compact row per unique measured PTS.
			std::unordered_map<int64_t, std::size_t> m_lookup; ///< Constant-time PTS-to-row lookup.
			std::unordered_set<int64_t> m_processed; ///< Encode timestamps already encountered.
			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFrame> m_previous; ///< Previous real, unfiltered encode input.
			std::optional<std::size_t> m_previousRow; ///< Previous encode row, absent after a discontinuity.
			std::optional<int64_t> m_lastPts; ///< Last measurement PTS, absent after a gap.
			std::size_t m_group = 0; ///< Current continuity group.
			unsigned m_frames = 0; ///< Measurement inputs seen, including rejected inputs.
			bool m_voted = false; ///< Measurement finalized.
			double m_sigmaMin = 0.0; ///< Minimum nonzero regional target.
			double m_sigmaP50 = 0.0; ///< Median nonzero regional target.
			double m_sigmaMax = 0.0; ///< Maximum regional target.
			unsigned m_ran = 0; ///< Frames with positive measured targets.
			unsigned m_skipped = 0; ///< Measured frames with zero/invalid targets.
			unsigned m_applied = 0; ///< Successfully blended encode frames.
			unsigned m_unsupported = 0; ///< Unsupported encode formats passed through.
			unsigned m_rejected = 0; ///< Missing/duplicate/unconvertible measurement inputs.
			/** @endcond */
	};
}
