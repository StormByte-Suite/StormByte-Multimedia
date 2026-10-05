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
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <memory>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Audio
 * @brief Audio process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Audio {
	/**
	 * @class Afftdn
	 * @brief FFT denoise via libavfilter `afftdn`. Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Afftdn>(log).
	 *
	 * @par What it is for
	 * Steady hiss / air / tape floor on dialogue and
	 * music. Not a click repair, not a reverb killer, not
	 * @ref Loudnorm. If the noise floor moves, pass
	 * `trackNoise=true`. Too much `nr` hollows the top.
	 *
	 * @par Do not stack
	 * One Afftdn. No custom `bn` profile and no
	 * `sample_noise` window in this leaf. Fine before
	 * Downmix/Upmix and before Loudnorm.
	 *
	 * @par Algorithm
	 * `afftdn=nr=:nf=:tn=:nt=w:om=o`. Empty nr=12 dB,
	 * nf=−50 dB, tn off. Graph owns FFT overlap; no Hold.
	 *
	 * @par Mutation
	 * Save. EAGAIN = wait. @ref Eof flushes.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Afftdn: public Filter::Process {
		public:
			/**
			 * @brief FFT denoise (`afftdn`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param nr Noise reduction in dB. Empty → 12.
			 * @param nf Noise floor in dB. Empty → −50.
			 * @param trackNoise Enable floor tracking (`tn`). Empty → false.
			 */
			Afftdn(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> nr = {},
				Safe::Optional<double> nf = {},
				Safe::Optional<bool> trackNoise = {}) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Source filter.
			 */
			Afftdn(const Afftdn& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Afftdn(Afftdn&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Afftdn() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Afftdn& operator=(const Afftdn& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Afftdn& operator=(Afftdn&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Audio.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops the cached graph.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets the graph before the first frame.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Pushes one audio frame through `afftdn` and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the graph, if any.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `afftdn=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_nrIn;		///< Caller nr, or empty
			Safe::Optional<double> m_nfIn;		///< Caller nf, or empty
			Safe::Optional<bool> m_trackIn;		///< Caller tn, or empty

			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused graph
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Afftdn);
