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
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <memory>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @class Crop
	 * @brief Crop a decoded video frame. Process leaf.
	 *
	 * Attach with @c job.Video(in).Filter<Crop>(log, x, y, w, h)
	 * or @c Filter<Crop>(log) to detect letterbox / pillarbox.
	 *
	 * @par What it is for
	 * Drop black bars or an explicit window before @ref Scale
	 * and @ref Watermark. Auto mode is for stable cinema bars,
	 * not bouncing credits or a fade from black.
	 *
	 * @par Auto detect
	 * Holds like @ref Watermark. Probes a GRAY8 view from
	 * @ref FFmpeg::AVFrame::ScaleTo. Needs a boxed pair
	 * (top+bottom or left+right) stable for 8 frames. If that
	 * never happens, @ref LastChance logs a Warning and the
	 * leaf becomes a no-op (no @ref Filter::FFmpeg::Save).
	 * It does not guess a crop. Hardware frames Fail.
	 *
	 * @par Explicit
	 * @a x,@a y,@a w,@a h in frame pixels. Out of range or an
	 * empty rectangle Fails. Identity window is a no-op.
	 * No Hold.
	 *
	 * @par Do not stack
	 * One Crop. @ref Pad is the opposite leaf (add canvas).
	 *
	 * @par Mutation
	 * Clone, @ref FFmpeg::AVFrame::Crop,
	 * @ref FFmpeg::AVFrame::ApplyCropping, Save.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::Video::Watermark
	 * @par Boundary ownership
	 * Requires a compatible C++ ABI and the Multimedia, Logger and Base providers
	 * to remain loaded. Own the leaf through Base-heap Safe pointers. The private
	 * frame cache is allocated, mutated and destroyed only by out-of-line
	 * Multimedia methods; copying and moving the leaf are disabled.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Crop: public Filter::Process {
		public:
			/**
			 * @brief Detect letterbox / pillarbox. Hold until sure, else no-op.
			 * @param log Shared logger. Empty pointer means no log.
			 */
			explicit Crop(Safe::Shared<StormByte::Logger::Log> log) noexcept;

			/**
			 * @brief Explicit window in frame pixels.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param x Left origin.
			 * @param y Top origin.
			 * @param width Cropped width.
			 * @param height Cropped height.
			 */
			Crop(Safe::Shared<StormByte::Logger::Log> log,
				int x, int y, int width, int height) noexcept;

			/**
			 * @brief Copying the provider-owned leaf is disabled.
			 * @param other Leaf that cannot be copied.
			 */
			Crop(const Crop& other) = delete;

			/**
			 * @brief Moving the mounted leaf is disabled.
			 * @param other Leaf that cannot be moved.
			 */
			Crop(Crop&& other) noexcept = delete;

			/**
			 * @brief Releases the provider-owned probe cache.
			 */
			~Crop() noexcept override;

			/**
			 * @brief Copy assignment is disabled.
			 * @param other Leaf that cannot be copied.
			 * @return Assignment is unavailable.
			 */
			Crop& operator=(const Crop& other) = delete;

			/**
			 * @brief Move assignment is disabled.
			 * @param other Leaf that cannot be moved.
			 * @return Assignment is unavailable.
			 */
			Crop& operator=(Crop&& other) noexcept = delete;

			/**
			 * @brief Media this filter handles.
			 * @return Video.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @brief Drops probe state and luma cache.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Resets probe state.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Probes while Held, or applies the window and Save.
			 * @param frame Current pipeline unit. Non-video is ignored.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Hold ceiling: crop only if the probe was sure, else no-op.
			 * @param frame Last held unit.
			 */
			void LastChance(const Pipeline::Frame& frame) noexcept override;

		private:
			static constexpr std::uint8_t ProbeMax = 200;	///< Maximum probe count

			/**
			 * @brief GRAY8 view of @p src via ScaleTo (Sws).
			 * @param src Live frame.
			 * @return Luma frame, or nullptr on Fail.
			 */
			const StormByte::Multimedia::FFmpeg::AVFrame* Luma(
				const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Samples bars on the luma view of @p src.
			 * @param src Live frame.
			 * @return true if this frame saw a boxed pair.
			 */
			bool ProbeBars(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Boxed pair stable for 8 probes.
			 * @return True when detected bars have stabilized.
			 */
			bool Sure() const noexcept;

			/**
			 * @brief Applies stored bars via Crop / ApplyCropping and Save.
			 */
			void Apply() noexcept;

			/**
			 * @brief Drops cached luma frame.
			 */
			void DropLuma() noexcept;

			bool m_auto;		///< Detect mode
			bool m_released;	///< Hold finished
			bool m_skip;		///< Auto gave up: passthrough
			int m_x;			///< Explicit origin x
			int m_y;			///< Explicit origin y
			int m_w;			///< Explicit width
			int m_h;			///< Explicit height
			int m_left;			///< Detected left bar
			int m_right;		///< Detected right bar
			int m_top;			///< Detected top bar
			int m_bottom;		///< Detected bottom bar
			int m_stable;		///< Consecutive matching probes
			int m_lumaW;		///< Cached source width
			int m_lumaH;		///< Cached source height
			int m_lumaFmt;		///< Cached source pixel format
			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFrame> m_luma;	///< Provider-owned luma cache
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Crop);
