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
	 * @class Limiter
	 * @brief Peak ceiling via libavfilter `alimiter`. Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Limiter>(log).
	 *
	 * @par What it is for
	 * Stop inter-sample / digital overs before the encode.
	 * A ceiling, not a loudness match — that is @ref Loudnorm.
	 * Put it last on the audio stretch, or after Loudnorm if
	 * a second safety rail is wanted. It does not compress
	 * LRA and does not change layout or rate.
	 *
	 * @par Do not stack
	 * One Limiter. FFmpeg’s `alimiter` auto-level is **off**
	 * (`level=0`): makeup to 0 dBFS is not this leaf.
	 *
	 * @par Algorithm
	 * @c alimiter=limit=&lt;linear&gt;:level=0:latency=1.
	 * Empty ceiling is −1.5 dBTP (same default as Loudnorm).
	 * `limit` is 10^(dB/20), clamped to 0.0625–1. Lookahead
	 * delay is compensated (`latency=1`). Hardware N/A.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the abuffersink frame.
	 * EAGAIN = wait. @ref Eof flushes the lookahead.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Audio::Loudnorm
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Limiter: public Filter::Process {
		public:
			/**
			 * @brief Peak limiter (`alimiter`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param ceiling True-peak ceiling in dBTP. Empty → −1.5.
			 */
			Limiter(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> ceiling = {}) noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source filter.
			 */
			Limiter(const Limiter& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source filter.
			 */
			Limiter(Limiter&& other) noexcept = delete;

			/**
			 * @brief Releases the provider-owned graph.
			 */
			~Limiter() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Limiter& operator=(const Limiter& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Limiter& operator=(Limiter&& other) noexcept = delete;

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
			 * @brief Pushes one audio frame through `alimiter` and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains the lookahead buffer.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `alimiter=…` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_ceilIn;	///< Caller dBTP, or empty

			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< FFmpeg graph context.
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Limiter);
