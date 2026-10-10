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
 * A written StormByte commercial agreement may license this original source
 * on terms other than the LGPL, including specific use, distribution or
 * linking arrangements such as static linking, as stated in that agreement.
 * It does not grant rights to dependencies or waive their license conditions.
 * Enabling WITH_GPL or WITH_NONFREE may include components with separate
 * obligations for modification, linking (static or dynamic), redistribution
 * or works that incorporate them. The person modifying, linking, packaging or
 * distributing the resulting work is responsible for determining and meeting
 * all applicable requirements, including any needed patent permissions.
 * A StormByte commercial agreement does not provide those rights for GPL or
 * nonfree components.
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

#include <cstdint>
#include <memory>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Audio
 * @brief Audio process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Audio {
	/**
	 * @class Resample
	 * @brief Change sample rate via libavfilter `aresample`. Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Resample>(log) or
	 * @c Filter<Resample>(log, hz).
	 *
	 * @par What it is for
	 * Delivery sample rate (default 48000). Film/TV out,
	 * or match an encoder that will not take 44.1. It does
	 * **not** change pitch or duration — that is
	 * @ref StormByte::Multimedia::Pipeline::Filter::Audio::Rubberband. Layout stays put.
	 *
	 * @par Do not stack
	 * One Resample. Same rate as the source is a no-op.
	 * Do not set `resampler=soxr`. Rate ≤ 0 Fails.
	 *
	 * @par Algorithm
	 * `aresample=osr=<rate>` only. FFmpeg default swr.
	 *
	 * @par Mutation
	 * Save + BindProperties. EAGAIN = wait. @ref StormByte::Multimedia::Pipeline::Filter::Audio::Resample::Eof flushes.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Resample: public Filter::Process {
		public:
			/**
			 * @brief Resample to @p rate Hz (`aresample=osr=`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param rate Output sample rate in Hz. Empty → 48000.
			 */
			Resample(Safe::Shared<StormByte::Logger::Log> log,
					Safe::Optional<int> rate = {}) noexcept;

			/**
			 * @brief Copy is not allowed. The graph is bound to one tube.
			 * @param other Source filter.
			 */
			Resample(const Resample& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Resample(Resample&& other) noexcept = delete;

			/**
			 * @brief Drops the cached graph.
			 */
			~Resample() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Resample& operator=(const Resample& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Resample& operator=(Resample&& other) noexcept = delete;

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
			 * @brief Pushes one audio frame through `aresample` and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains frames still held by the graph, if any.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Resolved output rate in Hz.
			 * @return @ref StormByte::Multimedia::Pipeline::Filter::Audio::Resample::m_rateIn or 48000.
			 */
			int Rate() const noexcept;

			/**
			 * @brief Builds the avfilter chain.
			 * @return `aresample=osr=…` for @ref StormByte::Multimedia::FFmpeg::AVFilterGraph::Ensure.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<int> m_rateIn;	///< Caller rate, or empty
			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< FFmpeg graph context.
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Resample);
