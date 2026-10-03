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

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {

	/**
	 * @class Degrain
	 * @brief Two-pass, scene-aware film-grain reducer built on libavfilter
	 *        @c fftdnoiz. ProcessTwoPasses leaf. Not a wrapper and not a
	 *        copy of @ref Fftdnoiz.
	 *
	 * Attach with @c job.Video(in).Filter<Degrain>(log) or
	 * @c Filter<Degrain>(log, cap) to clamp the strongest stretch.
	 *
	 * @par What it is for
	 * Hollywood film grain that survived the remaster (roughly pre-2015:
	 * Harry Potter, similar 35 mm scans). One pass of @ref Fftdnoiz at
	 * a fixed sigma either leaves grain on walls / night or plastics
	 * faces. Degrain walks the picture once, scores every frame, splits
	 * the timeline into stretches, and only then picks a sigma per
	 * stretch. The second pass runs @c fftdnoiz with that map.
	 *
	 * Use it when you can see grain in mid-tones and crushed night, and
	 * you are willing to spend a full decode before encode. It is not
	 * a live filter and not a substitute for
	 * @ref StormByte::Multimedia::Pipeline::Filter::Audio::Afftdn (audio) or
	 * @ref Deband (contouring).
	 *
	 * @par What it is not
	 * Not @ref Fftdnoiz with a different name. @ref Fftdnoiz is one
	 * sigma for every frame. Degrain chooses a strength for each
	 * measured stretch and leaves stretches unchanged when denoising
	 * is unnecessary or the measurement is inconclusive.
	 *
	 * @par When to attach
	 * After decode, before @ref Cas / @ref Scale / @ref Deband.
	 * After @ref Fieldmatch / @ref Decimate if the source was
	 * telecined. Do not put it on animation, screen content, or a
	 * master that is already clean — the night floor will still fire
	 * on dark flats.
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
	 * At the end of measurement, changes in brightness and grain
	 * determine stretch boundaries and a denoising strength for each.
	 * @ref Process applies that strength during the encode pass.
	 * Skipped or inconclusive stretches remain unchanged. Samples
	 * that cannot be converted for measurement are omitted.
	 *
	 * @par Strength selection
	 * Dark stretches receive a minimum denoising strength even when
	 * detail detection is inconclusive. Flat areas follow the measured
	 * grain strength up to a ceiling; motion, skin-like colours and
	 * mixed detail influence the selected strength. Clean mid-tone or
	 * bright stretches may be skipped. The caller's cap is an extra
	 * ceiling, not a floor. Detail detection is not face recognition.
	 *
	 * @par Cost
	 * Measurement requires a full decode and memory for at most
	 * 11 measurement frames plus compact per-frame measurements.
	 * The second pass performs temporal FFT denoising, which can be
	 * expensive on 4K. Full pictures are not retained for the whole job.
	 *
	 * @par Failure
	 * No stable stretch → Warning and passthrough. Graph Ensure /
	 * Filter errors Fail the leaf. Uncertainty in auto detect is
	 * a skip, never a blind sigma.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Fftdnoiz
	 * @see StormByte::Multimedia::Pipeline::Filter::ProcessTwoPasses
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Degrain: public Filter::ProcessTwoPasses {
		public:
			/**
			 * @brief Scene-aware grain reduce.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param sigmaCap Optional hard ceiling for every stretch.
			 *        Empty uses the default maximum strength.
			 */
			Degrain(std::shared_ptr<StormByte::Logger::Log> log,
				std::optional<double> sigmaCap = {}) noexcept;

			Degrain(const Degrain& other) = delete;
			Degrain(Degrain&& other) noexcept = delete;
			~Degrain() noexcept override = default;
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
			 * @brief Second pass: @c fftdnoiz at the voted sigma, or no-op.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Finalizes strength selection or emits remaining filtered output.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Reports stretch status, frame counts, applied/skipped filtering and strength.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			/**
			 * @struct Slot
			 * @brief One picture in the temporal ring.
			 *
			 * Only the ring holds pixels, and only ≤ RingMax of them.
			 * @c mean is a cheap Y average used to reject a neighbour
			 * whose lighting already changed (cut / flash).
			 */
			struct Slot {
				std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFrame> pic; ///< YUV420P copy
				int64_t pts = 0;   ///< Source PTS
				double mean = 0.0; ///< Full-frame luma mean
			};

			/**
			 * @struct Row
			 * @brief Per-frame numbers kept after the pixels are gone.
			 *
			 * This is what Decide votes on. Adding a column is cheap;
			 * adding another picture is not.
			 */
			struct Row {
				int64_t pts = 0;    ///< Source PTS
				double mean = 0.0;  ///< Luma mean
				double p10 = 0.0;   ///< 10th percentile luma (reserved)
				double p90 = 0.0;   ///< 90th percentile luma (reserved)
				double mid = 0.0;   ///< RMS residual in the mid band
				double dark = 0.0;  ///< RMS residual in the dark band
				double fLive = 0.0; ///< Fraction of samples that look “structured”
				double fSkin = 0.0; ///< Fraction of samples in the UV skin box
				double fFlat = 0.0; ///< Fraction of low-bandpass samples
				double mot = 0.0;   ///< Fraction that found no stable neighbour
				double raw = 0.0;   ///< Weighted dark+mid residual → sigma hint
				bool flash = false; ///< Isolated mean spike vs local median
			};

			/**
			 * @struct Segment
			 * @brief One voted stretch of the timeline.
			 *
			 * @c skip defaults true so a half-built segment cannot
			 * leak a graph. @ref Decide sets it false before the vote.
			 */
			struct Segment {
				int64_t firstPts = 0; ///< Inclusive
				int64_t lastPts = 0;  ///< Inclusive
				double sigma = 0.0;   ///< fftdnoiz sigma, 0 if skip
				bool skip = true;     ///< No-op this stretch
			};

			/**
			 * @brief Scale @p src to YUV420P, push the ring, score the centre.
			 * @param src Live decoder frame. ScaleTo failure drops the sample.
			 */
			void PushFrame(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Fill one @ref Row from ring slot @p idx using ±RingSpan neighbours.
			 * @param idx Index in @ref m_ring. No-op if already scored or tiny.
			 */
			void ScoreCenter(std::size_t idx) noexcept;

			/**
			 * @brief Score every remaining slot so the last 5 frames are not silent.
			 */
			void FlushRing() noexcept;

			/**
			 * @brief Cut the row list into stretches and pick sigma / skip.
			 *
			 * Called once from measure @ref Eof. After it returns,
			 * pictures are gone and only @ref m_seg remains.
			 */
			void Decide() noexcept;

			/**
			 * @brief Stretch that owns @p pts, or nullptr.
			 */
			const Segment* Find(int64_t pts) const noexcept;

			/**
			 * @brief avfilter chain for the current @ref m_sigma.
			 * @return @c fftdnoiz=sigma=…:prev=1:next=1
			 */
			std::string Chain() const noexcept;

			std::optional<double> m_capIn; ///< Caller ceiling, or empty
			std::deque<Slot> m_ring;       ///< ≤ RingMax YUV420P pictures
			std::vector<Row> m_row;        ///< One row per scored frame
			std::vector<Segment> m_seg;    ///< Voted map
			unsigned m_frames;             ///< Pictures seen in Measure
			bool m_voted;                  ///< Decide already ran
			double m_sigma;                ///< Graph currently open at
			double m_sigmaMin;             ///< Report
			double m_sigmaP50;             ///< Report
			double m_sigmaMax;             ///< Report
			int m_ran;                     ///< Stretches that will filter
			int m_skipped;                 ///< Stretches left untouched
			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph; ///< Second-pass graph
	};
}
