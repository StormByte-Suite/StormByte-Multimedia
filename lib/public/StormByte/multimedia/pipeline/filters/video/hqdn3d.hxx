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
#include <StormByte/multimedia/visibility.h>

#include <memory>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Hqdn3d
	 * @brief High-quality 3D denoise. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Hqdn3d>(log).
	 *
	 * @par What it is for
	 * Everyday denoise: light analog grain, DVD/broadcast
	 * hiss, web rips. Fast, one output per input. Default
	 * pick when you do not want BM3D/FFT cost. Too strong
	 * and it melts texture into plastic.
	 *
	 * @par Do not stack
	 * Exclusive with @ref Bm3d, @ref NlMeans,
	 * @ref Fftdnoiz, @ref VagueDenoiser and @ref Atadenoise
	 * (it already has a temporal term). Fine before
	 * @ref Deband and @ref Cas.
	 *
	 * @par Temporal behavior
	 * The first frame receives spatial denoising only; subsequent
	 * frames also receive temporal denoising. No initial Hold.
	 *
	 * @par Strength
	 * Four coefficients in 8-bit units, same layout as
	 * FFmpeg hqdn3d. 0 on a slot uses the built-in default
	 * (4 / 3 / 6 / 4.5).
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of a new
	 * @ref StormByte::Multimedia::FFmpeg::AVFrame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Hqdn3d: public Filter::Process {
		public:
			/**
			 * @brief hqdn3d with FFmpeg-style strengths.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param lumaSpatial Luma spatial strength. 0 → 4.
			 * @param chromaSpatial Chroma spatial strength. 0 → 3.
			 * @param lumaTemporal Luma temporal strength. 0 → 6.
			 * @param chromaTemporal Chroma temporal strength. 0 → 4.5
			 *        stored as 45 tenths; pass 0 for the default.
			 */
			Hqdn3d(std::shared_ptr<StormByte::Logger::Log> log,
				double lumaSpatial = 0.0, double chromaSpatial = 0.0,
				double lumaTemporal = 0.0, double chromaTemporal = 0.0) noexcept;

			Hqdn3d(const Hqdn3d& other) = delete;
			Hqdn3d(Hqdn3d&& other) noexcept = delete;
			~Hqdn3d() noexcept override;
			Hqdn3d& operator=(const Hqdn3d& other) = delete;
			Hqdn3d& operator=(Hqdn3d&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Resets temporal denoising history.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Calls @ref Clean.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Denoises the current video unit and Save.
			 * @param frame Video unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			double m_ls;	///< Luma spatial
			double m_cs;	///< Chroma spatial
			double m_lt;	///< Luma temporal
			double m_ct;	///< Chroma temporal
			StormByte::Multimedia::FFmpeg::AVFrame m_prev;	///< Last filtered look
	};
}
