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
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <memory>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Audio
 * @brief Audio process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Audio {
	/**
	 * @class Denoise
	 * @brief Two-pass room-tone hiss reduction. ProcessTwoPasses leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Denoise>(log).
	 *
	 * @par What it is for
	 * Phone / cheap-mic recordings: the constant floor (preamp
	 * hiss, A/C, room tone). Not a neighbour, a slammed door
	 * or a click — those are events. Clicks are @ref Adeclick.
	 * Generic one-pass hiss without a profile is @ref Afftdn.
	 *
	 * @par Measure
	 * Mix-down windows (~350 ms). Keep candidates that look
	 * like floor (not digital mute, not speech, not a transient).
	 * Each candidate has RMS + crest + ZCR + high-pass ratio.
	 * The winner is the signature with the **most neighbours**
	 * inside a distance ball (the repeated room tone), not
	 * the quietest window.
	 *
	 * @par If unsure
	 * Too few candidates, no cluster, or an unstable floor:
	 * Warning and Process is a no-op (no Save). Never subtract
	 * “whatever was left”.
	 *
	 * @par Process
	 * @c afftdn=nr=:nf= from the winner. @c tn off. Conservative
	 * @c nr (12 dB, or caller).
	 *
	 * @par Do not stack
	 * Denoise **or** Afftdn, not both. Hardware N/A.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::ProcessTwoPasses
	 * @see StormByte::Multimedia::Pipeline::Filter::Audio::Afftdn
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Denoise: public Filter::ProcessTwoPasses {
		public:
			/**
			 * @brief Automatic room-tone denoise.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param nr Reduction in dB. Empty → 12. Clamped 1–30.
			 */
			Denoise(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> nr = {}) noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source filter.
			 */
			Denoise(const Denoise& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source filter.
			 */
			Denoise(Denoise&& other) noexcept = delete;

			/**
			 * @brief Releases measurement storage and the graph in Multimedia.
			 */
			~Denoise() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Denoise& operator=(const Denoise& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Denoise& operator=(Denoise&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Audio.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops candidates, vote and graph.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets per-pass accumulators. Keeps the vote.
			 */
			void Setup() noexcept override;

			/**
			 * @brief First pass: collect room-tone candidates. No Save.
			 * @param frame Current audio unit.
			 */
			void Measure(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Second pass: afftdn with the voted floor, or no-op.
			 * @param frame Current audio unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Closes the vote (measure) or flushes the graph (process).
			 */
			void Eof() noexcept override;

			/**
			 * @brief Vote result and afftdn knobs.
			 * @return Ok after measure (including skip), else Failed.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			/**
			 * @brief Provider-local room-tone signature used by the vote.
			 */
			struct Candidate {
				double rmsDb = 0.0;	///< Window RMS in dB
				double crest = 0.0;	///< Peak-to-RMS ratio
				double zcr = 0.0;	///< Zero-crossing ratio
				double hp = 0.0;		///< High-pass energy ratio
			};

			/**
			 * @brief Mixes @p src into the measure window and emits candidates.
			 * @param src Borrowed source frame, valid throughout this call.
			 */
			void Ingest(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Scores one full window and maybe stores a candidate.
			 */
			void EmitWindow() noexcept;

			/**
			 * @brief Picks the signature with the most neighbours, or skip.
			 */
			void Vote() noexcept;

			/**
			 * @brief @c afftdn=nr=:nf= chain for the winner.
			 * @return Base-owned filter chain for the voted floor.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_nrIn;	///< Caller noise reduction, or empty
			std::vector<float> m_acc;		///< Provider-local measurement samples
			std::vector<Candidate> m_cand;	///< Provider-local candidate signatures
			int m_rate;						///< Latched sample rate
			int m_win;						///< Measurement window size
			unsigned m_frames;				///< Measured audio frame count
			bool m_voted;					///< Vote has completed
			bool m_skip;						///< Uncertain floor leaves audio unchanged
			double m_nf;						///< Voted noise floor in dB
			double m_nr;						///< Resolved noise reduction in dB
			int m_matches;					///< Matching room-tone signatures

			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Provider-owned graph
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private STL measurement storage and graph ownership never leave Multimedia;
 * all allocation, mutation and destruction execute in its out-of-line methods.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Denoise);
