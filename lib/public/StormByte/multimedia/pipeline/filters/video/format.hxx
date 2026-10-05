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
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Format
	 * @brief Convert pixel format, same geometry. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Format>(log, pix_fmt).
	 *
	 * @par What it is for
	 * Hand the encoder a layout it accepts: 10-bit → 8-bit
	 * `yuv420p`, packed RGB → planar YUV, etc. Geometry does
	 * not change; that is @ref Scale. Colour tags are copied,
	 * not remapped — HDR→SDR is @ref Tonemap.
	 *
	 * @par Do not stack
	 * One Format per tube unless two encodes need two
	 * layouts. Same format as the source is a no-op (no Save).
	 * @c AV_PIX_FMT_NONE Fails. Hardware frames Fail.
	 *
	 * @par Engine
	 * @ref FFmpeg::AVFrame::ScaleTo at source width/height.
	 * Packed RGB / @c VideoLayout::Unknown uses
	 * @c Scaler::Sws; planar YUV/gray uses @c Scaler::Zimg.
	 *
	 * @par Mutation
	 * @ref Filter::FFmpeg::Save of the converted frame.
	 *
	 * @see StormByte::Multimedia::FFmpeg::AVFrame::ScaleTo
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Format: public Filter::Process {
		public:
			/**
			 * @brief Convert to @p pixFmt (`AVPixelFormat` as int).
			 * @param log Shared logger. Empty pointer means no log.
			 * @param pixFmt Destination pixel format.
			 */
			Format(Safe::Shared<StormByte::Logger::Log> log, int pixFmt) noexcept;

			/**
			 * @brief Copy construction is disabled.
			 * @param other Source filter.
			 */
			Format(const Format& other) = delete;

			/**
			 * @brief Move construction is disabled.
			 * @param other Source filter.
			 */
			Format(Format&& other) noexcept = delete;

			/**
			 * @brief Releases the filter in its owning module.
			 */
			~Format() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Format& operator=(const Format& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Source filter.
			 * @return This filter.
			 */
			Format& operator=(Format&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Nothing to drop between runs.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Nothing to acquire beyond construction.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Converts the current video unit and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

		private:
			int m_pixFmt;	///< Destination AVPixelFormat as an integer.
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Format);
