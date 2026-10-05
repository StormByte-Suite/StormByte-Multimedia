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
	 * @class Adeclick
	 * @brief Impulsive click / pop removal via `adeclick`. Process leaf.
	 *
	 * Attach with @c job.Audio(in).Filter<Adeclick>(log).
	 *
	 * @par What it is for
	 * Vinyl ticks, edit splices, a single sample spike.
	 * Autoregressive fill of samples tagged as impulse.
	 * Broadband hiss is @ref Afftdn, not this leaf.
	 *
	 * @par Do not stack
	 * One Adeclick. Do not run it “to be safe” on a clean
	 * master: a low threshold eats transients. Hardware N/A.
	 *
	 * @par Algorithm
	 * Bare @c adeclick uses FFmpeg defaults (window 55 ms,
	 * overlap 75 %, threshold 2). A supplied threshold is
	 * clamped to 1–100. EAGAIN = wait. @ref Eof flushes
	 * the analysis window.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the abuffersink frame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Audio::Afftdn
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Adeclick: public Filter::Process {
		public:
			/**
			 * @brief Click / pop removal (`adeclick`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param threshold Impulse threshold 1–100. Empty → FFmpeg default (2).
			 */
			Adeclick(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> threshold = {}) noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source filter.
			 */
			Adeclick(const Adeclick& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source filter.
			 */
			Adeclick(Adeclick&& other) noexcept = delete;

			/**
			 * @brief Releases the filter resources.
			 */
			~Adeclick() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Adeclick& operator=(const Adeclick& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Adeclick& operator=(Adeclick&& other) noexcept = delete;

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
			 * @brief Pushes one audio frame through `adeclick` and Save.
			 * @param frame Current pipeline unit. Non-audio is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Drains the analysis window.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `adeclick` or `adeclick=threshold=` .
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_thrIn;	///< Caller threshold, or empty for the FFmpeg default.

			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Graph allocated and released only by the provider.
	};
}

/**
 * @brief Requires compatible C++ ABI and loaded Multimedia, Base and Logger providers.
 * @note Private graph ownership is allocated and released only by Multimedia.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Audio::Adeclick);
