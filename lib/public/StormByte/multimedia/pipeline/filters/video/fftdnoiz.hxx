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
#include <StormByte/type_traits/safe.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Fftdnoiz
	 * @brief 3-D FFT denoise via libavfilter `fftdnoiz` (FFmpeg ≥ 9.0.1). Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Fftdnoiz>(log).
	 *
	 * @par What it is for
	 * Film-like grain that lives in both space and time.
	 * With prev/next it is the “expensive grain” leaf;
	 * spatial-only (prev=next=0) is a milder FFT denoise.
	 * Prefer this over stacking Atadenoise+Bm3d.
	 *
	 * @par Do not stack
	 * Exclusive with @ref Bm3d, @ref NlMeans,
	 * @ref VagueDenoiser and @ref Hqdn3d. Do not also run
	 * @ref Atadenoise when prev/next are non-zero.
	 *
	 * @par Algorithm
	 * Frequency-domain denoise on overlapping blocks.
	 * Early frames may not leave the sink (`EAGAIN`);
	 * @ref Process then returns without Save. @ref Eof
	 * drains the window.
	 *
	 * @par Defaults
	 * Empty arguments: sigma=1, prev=1, next=1 (3-D).
	 * FFmpeg’s own prev/next default is 0. `block` is the
	 * FFmpeg 9 size (32). prev/next clamped to 0..1.
	 *
	 * @par Cost
	 * Heavy on 4K, especially with a temporal window.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 *
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 * @note DLL use requires the same compatible C++ ABI and the parent Safe base.
	 *       Allocation and destruction remain creator-owned; graph state stays in Multimedia.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Fftdnoiz: public Filter::Process {
		public:
			/**
			 * @brief 3-D FFT denoise (`fftdnoiz`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param sigma Strength. Empty → 1. Range 0–100.
			 * @param prev Previous frames in the window. Empty → 1. Clamped to 0..1.
			 * @param next Future frames in the window. Empty → 1. Clamped to 0..1.
			 */
			Fftdnoiz(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> sigma = {},
				Safe::Optional<unsigned> prev = {},
				Safe::Optional<unsigned> next = {}) noexcept;

			/**
			 * @brief Copy construction is not allowed.
			 * @param other Source leaf.
			 */
			Fftdnoiz(const Fftdnoiz& other) = delete;

			/**
			 * @brief Move construction is not allowed.
			 * @param other Source leaf.
			 */
			Fftdnoiz(Fftdnoiz&& other) noexcept = delete;

			/**
			 * @brief Release owned state in the creator module.
			 */
			~Fftdnoiz() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source leaf.
			 * @return This leaf.
			 */
			Fftdnoiz& operator=(const Fftdnoiz& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source leaf.
			 * @return This leaf.
			 */
			Fftdnoiz& operator=(Fftdnoiz&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
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
			 * @brief Pushes one video frame through `fftdnoiz` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Closes the filter source and Saves leftover frames.
			 */
			void Eof() noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `format=...,fftdnoiz=...` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			Safe::String Chain() const noexcept;

			Safe::Optional<double> m_sigmaIn;									///< Caller sigma, or empty.

			Safe::Optional<unsigned> m_prevIn;									///< Caller previous-frame count, or empty.

			Safe::Optional<unsigned> m_nextIn;									///< Caller next-frame count, or empty.

			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused graph, allocated and destroyed through creator-owned callbacks.
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Fftdnoiz);
