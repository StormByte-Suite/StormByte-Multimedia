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
#include <StormByte/type_traits/safe.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Bm3d
	 * @brief Spatial BM3D denoise via libavfilter `bm3d` (`estim=basic`). Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Bm3d>(log).
	 *
	 * @par What it is for
	 * Heavy spatial grain when quality matters more than
	 * runtime: stills, 1080p film, a master you will encode
	 * once. Patch-matching keeps structure that a blur would
	 * melt. It is not a temporal filter and will not fix
	 * flicker. Prefer @ref Hqdn3d or @ref Atadenoise on a
	 * long, mildly noisy encode.
	 *
	 * @par Do not stack
	 * Exclusive with @ref NlMeans, @ref Fftdnoiz,
	 * @ref VagueDenoiser and @ref Hqdn3d. Two spatial
	 * denoisers smear detail and pay the cost twice.
	 * @c estim=final is not available (second input pad).
	 *
	 * @par Algorithm
	 * One frame. libavfilter groups similar 2-D patches,
	 * hard-thresholds in the 3-D transform domain and
	 * aggregates. No temporal references, no Hold.
	 *
	 * @par Defaults
	 * Empty arguments pick a profile from the first video frame:
	 * - 4K and above (width ≥ 3840 or height ≥ 2160):
	 *   sigma=2, group=8, range=9, bstep=4
	 * - below that (1080p class):
	 *   sigma=3, group=16, range=9, bstep=4
	 * Patch log2 size is always 4 (16×16). Caller values are
	 * used as-is.
	 *
	 * @par Cost
	 * CPU- and RAM-heavy. 4K with the 1080 profile is a bad
	 * idea; the UHD profile exists so a 4K job stays usable.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the buffersink frame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 * @see StormByte::Multimedia::FFmpeg::AVFilterGraph
	 * @par ABI contract
	 * Requires a compatible C++ ABI and the parent Safe ownership contract.
	 * Create through Base-heap Safe ownership and keep providers loaded until
	 * destruction; graph allocation and destruction remain provider-owned.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Bm3d: public Filter::Process {
		public:
			/**
			 * @brief Spatial BM3D (`estim=basic`).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param sigma Denoise strength. Empty → profile default.
			 * @param group Max similar patches in the 3-D stack. Empty → profile.
			 * @param range Block-matching radius in pixels. Empty → profile.
			 * @param bstep Sliding step between processed blocks. Empty → profile.
			 */
			Bm3d(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<double> sigma = {},
				Safe::Optional<unsigned> group = {},
				Safe::Optional<unsigned> range = {},
				Safe::Optional<unsigned> bstep = {}) noexcept;

			/**
			 * @brief Copy construction is not allowed.
			 * @param other Source filter.
			 */
			Bm3d(const Bm3d& other) = delete;

			/**
			 * @brief Move construction is not allowed.
			 * @param other Source filter.
			 */
			Bm3d(Bm3d&& other) noexcept = delete;

			/**
			 * @brief Releases settings and graph in the provider.
			 */
			~Bm3d() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Bm3d& operator=(const Bm3d& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Bm3d& operator=(Bm3d&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops the cached graph. Next @ref Process latches again.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets latch and graph before the first frame.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Denoises one video frame and @ref Filter::FFmpeg::Save it.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			/**
			 * @brief Resolves empty arguments from @p width × @p height.
			 * @param width Frame width.
			 * @param height Frame height.
			 */
			void Latch(int width, int height) noexcept;

			/**
			 * @brief Builds the avfilter chain from the latched values.
			 * @return `bm3d=...` string for @ref FFmpeg::AVFilterGraph::Ensure.
			 */
			std::string Chain() const noexcept;

			Safe::Optional<double> m_sigmaIn;		///< Caller sigma, or empty
			Safe::Optional<unsigned> m_groupIn;		///< Caller group, or empty
			Safe::Optional<unsigned> m_rangeIn;		///< Caller range, or empty
			Safe::Optional<unsigned> m_bstepIn;		///< Caller bstep, or empty
			double m_sigma;							///< Effective sigma
			unsigned m_group;						///< Effective group
			unsigned m_range;						///< Effective range
			unsigned m_bstep;						///< Effective bstep
			bool m_latched;							///< Profile chosen
			Safe::Unique<StormByte::Multimedia::FFmpeg::AVFilterGraph> m_graph;	///< Reused `buffer → bm3d → buffersink`
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Bm3d);
