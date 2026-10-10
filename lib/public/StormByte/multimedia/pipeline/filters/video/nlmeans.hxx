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
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class NlMeans
	 * @brief Spatial non-local means denoise. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<NlMeans>(log).
	 *
	 * @par What it is for
	 * Moderate spatial grain when BM3D is too expensive and
	 * hqdn3d is too blunt. Cartoon / anime edges survive
	 * better than with a box blur. Still one frame, no
	 * temporal help. Not the first choice on 4K.
	 *
	 * @par Do not stack
	 * Exclusive with @ref Bm3d, @ref Fftdnoiz,
	 * @ref VagueDenoiser and @ref Hqdn3d.
	 *
	 * @par Algorithm
	 * One frame. Each sample is a similarity-weighted average
	 * of patches inside a search window. No Hold.
	 * Implemented on the frame, not via avfilter `nlmeans`.
	 *
	 * @par Defaults
	 * Empty arguments pick a profile from the first video frame:
	 * - 4K+: research=2, patch=1, strength=0.8
	 * - below: research=3, patch=2, strength=1.0
	 * Caller values are used as-is.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of a new
	 * @ref StormByte::Multimedia::FFmpeg::AVFrame.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Process
	 * @par ABI contract
	 * Requires a compatible C++ ABI and a Safe-migrated parent base.
	 * Optional storage uses creator-module callbacks; destroy and deallocate
	 * the filter in its creating module while that module remains loaded.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC NlMeans: public Filter::Process {
		public:
			/**
			 * @brief Spatial nlmeans.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param research Search-window radius. Empty → profile default.
			 * @param patch Patch radius. Empty → profile default.
			 * @param strength Filter strength h. Empty → profile default.
			 */
			NlMeans(Safe::Shared<StormByte::Logger::Log> log,
				Safe::Optional<unsigned> research = {},
				Safe::Optional<unsigned> patch = {},
				Safe::Optional<double> strength = {}) noexcept;

			/**
			 * @brief Copying is disabled.
			 * @param other Source filter.
			 */
			NlMeans(const NlMeans& other) = delete;

			/**
			 * @brief Moving is disabled.
			 * @param other Source filter.
			 */
			NlMeans(NlMeans&& other) noexcept = delete;

			/**
			 * @brief Release filter ownership in the provider module.
			 */
			~NlMeans() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			NlMeans& operator=(const NlMeans& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			NlMeans& operator=(NlMeans&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Reset the latched profile.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Prepare the filter for processing.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Denoise a video frame.
			 * @param frame Borrowed input frame, valid for this call.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			/**
			 * @brief Resolves empty arguments from @p width × @p height.
			 * @param width Frame width.
			 * @param height Frame height.
			 */
			void Latch(int width, int height) noexcept;

			Safe::Optional<unsigned> m_researchIn;	///< Caller research, or empty
			Safe::Optional<unsigned> m_patchIn;	///< Caller patch, or empty
			Safe::Optional<double> m_hIn;		///< Caller strength, or empty
			unsigned m_research;			///< Effective research
			unsigned m_patch;			///< Effective patch
			double m_h;				///< Effective strength
			bool m_latched;				///< Profile chosen
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::NlMeans);
