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
	 * @class VagueDenoiser
	 * @brief Wavelet denoise via libavfilter `vaguedenoiser`. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<VagueDenoiser>(log).
	 *
	 * @par What it is for
	 * Fine residual grain after a temporal pass
	 * (@ref Atadenoise), or a cheap spatial-only clean on
	 * 4K where Bm3d/NlMeans/Fftdnoiz would stall. It will
	 * not remove large blotches or mosquito.
	 *
	 * @par Do not stack
	 * Exclusive with @ref Bm3d, @ref NlMeans,
	 * @ref Fftdnoiz and @ref Hqdn3d. After Atadenoise is
	 * the intended pairing.
	 *
	 * @par Algorithm
	 * 2-D wavelet shrink. Spatial only; no Hold.
	 *
	 * @par Defaults
	 * Empty arguments use FFmpeg's: threshold=2, nsteps=6,
	 * percent=85, method=garrote.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 *
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC VagueDenoiser: public Filter::Process {
		public:
			/**
			 * @brief Wavelet denoise (`vaguedenoiser`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param threshold Coefficient floor. Empty → 2.
			 * @param steps Wavelet steps. Empty → 6.
			 * @param percent Shrink percent. Empty → 85.
			 */
			VagueDenoiser(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				StormByte::Safe::Optional<double> threshold = {},
				StormByte::Safe::Optional<unsigned> steps = {},
				StormByte::Safe::Optional<double> percent = {}) noexcept;

			/**
			 * @brief Copy construction is unavailable.
			 * @param other Filter that cannot be copied.
			 */
			VagueDenoiser(const VagueDenoiser& other) = delete;
			/**
			 * @brief Move construction is unavailable.
			 * @param other Filter that cannot be moved.
			 */
			VagueDenoiser(VagueDenoiser&& other) noexcept = delete;
			/**
			 * @brief Releases owned options and the cached graph in the provider module.
			 */
			~VagueDenoiser() noexcept override;
			/**
			 * @brief Copy assignment is unavailable.
			 * @param other Filter that cannot be copied.
			 * @return No value; this operation is deleted.
			 */
			VagueDenoiser& operator=(const VagueDenoiser& other) = delete;
			/**
			 * @brief Move assignment is unavailable.
			 * @param other Filter that cannot be moved.
			 * @return No value; this operation is deleted.
			 */
			VagueDenoiser& operator=(VagueDenoiser&& other) noexcept = delete;

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
			 * @brief Pushes one video frame through `vaguedenoiser` and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			/**
			 * @brief Builds the avfilter chain.
			 * @return `vaguedenoiser=...` for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			StormByte::Safe::String Chain() const noexcept;

			StormByte::Safe::Optional<double> m_thrIn;		///< Caller threshold, or empty
			StormByte::Safe::Optional<unsigned> m_stepsIn;	///< Caller nsteps, or empty
			StormByte::Safe::Optional<double> m_pctIn;		///< Caller percent, or empty
			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Cached graph with provider-managed FFmpeg resources
	};
}

/**
 * @brief Conditional boundary safety requires compatible ABI and provider-managed FFmpeg lifetimes.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::VagueDenoiser);
