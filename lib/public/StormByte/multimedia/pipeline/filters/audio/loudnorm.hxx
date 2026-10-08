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
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>

#include <ebur128.h>


/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Audio
 * @brief Audio process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Audio {
	/**
	 * @class Loudnorm
	 * @brief Two-pass EBU R128 loudness (libebur128). ProcessTwoPasses leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Loudnorm>(log).
	 *
	 * @par What it is for
	 * Bring a finished mix to a delivery loudness (broadcast,
	 * VOD, disc) without changing the mix balance. Program
	 * integrated loudness is aimed at @a integrated LUFS;
	 * a linked true-peak ceiling stops inter-sample overs.
	 * It is not a compressor and not FFmpeg Dynamic: LRA is
	 * measured, not squeezed. Do not use it to “fix” a
	 * dialogue/music imbalance.
	 *
	 * @par Measure
	 * One ebur128 state for the whole layout (L/R/C/LFE/Ls/Rs
	 * map). Integrated loudness and LRA are **program** values
	 * (BS.1770). True peak is stored **per channel**. LFE is
	 * @c EBUR128_UNUSED in the loudness sum; it still counts
	 * for TP. Channels are never normalized independently.
	 *
	 * @par Process
	 * Linear gain is always @c I_target − I_measured, same on
	 * every channel. If that gain would push any channel over
	 * @p truePeak, a linked ceiling at @p truePeak runs on the
	 * gained samples. The program gain is not reduced. Same
	 * @ref Process as a one-pass leaf after measure has closed.
	 *
	 * @par Defaults
	 * - I = −23 LUFS (EBU R128)
	 * - TP = −1.5 dBTP
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of a new
	 * @ref StormByte::Multimedia::FFmpeg::AVFrame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::ProcessTwoPasses
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Loudnorm: public Filter::ProcessTwoPasses {
		public:
			/**
			 * @brief Two-pass loudness + linked true-peak ceiling.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param integrated Target integrated loudness in LUFS. Empty → −23.
			 * @param truePeak True-peak ceiling in dBTP. Empty → −1.5.
			 */
			Loudnorm(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> integrated = {},
				Safe::Optional<double> truePeak = {}) noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source filter.
			 */
			Loudnorm(const Loudnorm& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source filter.
			 */
			Loudnorm(Loudnorm&& other) noexcept = delete;

			/**
			 * @brief Releases the provider-owned meter and measurement storage.
			 */
			~Loudnorm() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Loudnorm& operator=(const Loudnorm& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Loudnorm& operator=(Loudnorm&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Audio.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops the ebur128 state. Next run starts clean.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets state before a pass.
			 */
			void Setup() noexcept override;

			/**
			 * @brief First pass: feed samples into ebur128. Does not Save.
			 * @param frame Current audio unit.
			 */
			void Measure(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Program gain, linked TP ceiling, Save.
			 * @param frame Current audio unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Closes the meter and freezes I / LRA / per-channel TP.
			 */
			void Eof() noexcept override;

			/**
			 * @brief Measured I/LRA, per-channel TP, targets and program gain.
			 * @return Ok after a successful measure, else Failed.
			 */
			class Filter::Report Report() const noexcept override;

		private:
			/**
			 * @brief Opens ebur128 for @p channels at @p rate.
			 * @param channels Layout channel count.
			 * @param rate Sample rate in Hz.
			 * @return false if the meter could not be created.
			 */
			bool OpenMeter(int channels, int rate) noexcept;

			/**
			 * @brief Maps channel @p i of a @p channels layout into ebur128.
			 * @param i Zero-based index.
			 * @param channels Layout width.
			 * @return ebur128 channel enum.
			 */
			static int MapChannel(int i, int channels) noexcept;

			/**
			 * @brief Interleaves @p src to float and adds it to the meter.
			 * @param src Audio frame.
			 * @return false on format / add failure.
			 */
			bool Add(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Reads I, LRA and per-channel TP, then computes @c m_gain.
			 */
			void CloseMeter() noexcept;

			/**
			 * @brief Scales @p src by @c m_gain and the linked TP ceiling.
			 * @param src Source audio.
			 * @return New frame, or empty on failure.
			 */
			StormByte::Multimedia::FFmpeg::AVFrame Gain(
				const StormByte::Multimedia::FFmpeg::AVFrame& src) const noexcept;

			double m_targetI;					///< Target integrated LUFS
			double m_targetTp;					///< Target true-peak dBTP
			ebur128_state* m_st;					///< Provider-owned meter, or nullptr
			int m_channels;						///< Latched channel count
			int m_rate;							///< Latched sample rate
			double m_measuredI;					///< Program integrated LUFS
			double m_measuredLra;				///< Program LRA (LU)
			StormByte::Safe::Vector<double> m_tp;	///< Base-owned true peak per channel (linear).
			double m_gain;						///< Linear amplitude gain (I only)
			double m_ceiling;					///< Linear TP ceiling
			bool m_limit;						///< Gain would exceed TP without ceiling
			bool m_ready;						///< Measure closed successfully
			unsigned m_frames;					///< Audio frames seen in Measure
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Base-owned peak storage and the ebur128 meter never leave Multimedia;
 * all allocation, mutation and destruction execute in its out-of-line methods.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Loudnorm);
