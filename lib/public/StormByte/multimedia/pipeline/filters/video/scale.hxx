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
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/property/resolution.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Scale
	 * @brief Scales a decoded video frame. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Scale>(log, w, h).
	 *
	 * @par What it is for
	 * Change geometry once, usually last among picture
	 * filters (after denoise / deband / CAS, before
	 * @ref Watermark if the logo is sized for the
	 * **output**). Upscale does not invent detail; CAS
	 * belongs on the source size, not after a 2× blow-up,
	 * unless you know why.
	 *
	 * @par Do not stack
	 * One Scale per tube. Same size as the source is a
	 * no-op (no Save). Both axes 0 Fail.
	 *
	 * @par Engine
	 * Empty filter/scaler use
	 * @ref StormByte::Multimedia::FFmpeg::AVFrame::ScaleTo
	 * defaults (`Resample::Default`, `Scaler::Zimg`).
	 * Packed RGB is not a zimg layout; pass `Scaler::Sws`.
	 * Width or height 0 keeps aspect ratio.
	 *
	 * @see StormByte::Multimedia::FFmpeg::AVFrame::ScaleTo
	 * @par ABI contract
	 * Requires a compatible C++ ABI and a Safe-migrated parent base.
	 * Optional storage uses creator-module callbacks; destroy and deallocate
	 * the filter in its creating module while that module remains loaded.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Scale: public Filter::Process {
		public:
			/**
			 * @brief Exact destination size.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param resolution Target resolution.
			 * @param filter Resample kernel. Empty → ScaleTo default.
			 * @param scaler Backend. Empty → ScaleTo default.
			 */
			Scale(Safe::Shared<StormByte::Logger::Log> log,
				const StormByte::Multimedia::Property::Resolution& resolution,
				Safe::Optional<StormByte::Multimedia::FFmpeg::AVFrame::Resample> filter = {},
				Safe::Optional<StormByte::Multimedia::FFmpeg::AVFrame::Scaler> scaler = {}) noexcept;

			/**
			 * @brief Destination size. 0 on one axis keeps aspect ratio.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param width Target width, or 0.
			 * @param height Target height, or 0.
			 * @param filter Resample kernel. Empty → ScaleTo default.
			 * @param scaler Backend. Empty → ScaleTo default.
			 */
			Scale(Safe::Shared<StormByte::Logger::Log> log,
				std::uint32_t width, std::uint32_t height,
				Safe::Optional<StormByte::Multimedia::FFmpeg::AVFrame::Resample> filter = {},
				Safe::Optional<StormByte::Multimedia::FFmpeg::AVFrame::Scaler> scaler = {}) noexcept;

			/**
			 * @brief Copy is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Scale(const Scale& other) = delete;

			/**
			 * @brief Move is not allowed. The tube owns the mounted leaf.
			 * @param other Source filter.
			 */
			Scale(Scale&& other) noexcept = delete;

			/**
			 * @brief Destructor.
			 */
			~Scale() noexcept override;

			/**
			 * @brief Copy assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Scale& operator=(const Scale& other) = delete;

			/**
			 * @brief Move assignment is not allowed.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Scale& operator=(Scale&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video. Other kinds pass through Gate.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Nothing to drop between runs.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Logs the requested target size.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Scales the current video unit and Save.
			 * @param frame Video unit.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			std::uint32_t m_width;	///< Requested width. 0 keeps aspect ratio
			std::uint32_t m_height;	///< Requested height. 0 keeps aspect ratio
			Safe::Optional<StormByte::Multimedia::FFmpeg::AVFrame::Resample> m_filter;	///< Empty → Resample::Default
			Safe::Optional<StormByte::Multimedia::FFmpeg::AVFrame::Scaler> m_scaler;	///< Empty → Scaler::Zimg
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Scale);
