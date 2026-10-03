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

#include <StormByte/binary_data.hxx>
#include <StormByte/buffer/generic.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/Sws.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/property/point.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video
 * @brief Video process filters.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video {
	/**
	 * @enum Anchor
	 * @brief Logo placement relative to the active picture.
	 *
	 * Top / Bottom / Left / Right are edges of the measured
	 * letterbox / pillarbox rectangle, not of the full frame.
	 */
	enum class STORMBYTE_MULTIMEDIA_PUBLIC Anchor {
	    TopLeft,
	    TopCenter,
	    TopRight,
	    CenterLeft,
	    Center,
	    CenterRight,
	    BottomLeft,
	    BottomCenter,
	    BottomRight
	};

	/**
	 * @class Watermark
	 * @brief Overlays a still image on decoded video. Real Hold example.
	 *
	 * Attach with
	 * @c job.Video(in, out).Filter<Watermark>(log, path, Anchor::BottomRight).
	 *
	 * @par What it is for
	 * Station / disc / review logo on the **finished**
	 * picture. Last video leaf: after denoise, deband,
	 * CAS and Scale (unless the PNG is authored for the
	 * pre-scale size). Opacity 0 is a no-op. A missing or
	 * broken logo logs a Warning and becomes passthrough;
	 * it does not Fail the job.
	 *
	 * @par Do not stack
	 * One Watermark. Do not Hold-probe twice. Absolute
	 * @ref Property::Point skips Hold (frame pixels).
	 * Anchor placement Holds to find letterbox bars.
	 *
	 * @par Hold (anchor placement only)
	 * Opens @ref FFmpeg::Hold(@ref ProbeMax) on the first
	 * video unit. @ref Process only probes while Held.
	 * @ref FFmpeg::Release replays parked units into
	 * @ref Paint so the logo is on from frame one.
	 * Release early when bars are stable (`m_stable >= 8`).
	 * @ref LastChance must Release or the job Fails.
	 *
	 * Bars are measured on a GRAY8 plane from ScaleTo, not
	 * on plane 0 of HDR. Overlay is RGBA then back to the
	 * source format.
	 *
	 * @see StormByte::Multimedia::Pipeline::Filter::FFmpeg::Hold
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Watermark: public Process {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Logo at an anchor on the active picture.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param logo Path to a still image (png, jpeg, webp, bmp).
			 * @param anchor Placement relative to measured bars.
			 * @param opacity 0–100. 0 = no-op.
			 * @param margin Pixels from the anchored active edge.
			 */
			Watermark(std::shared_ptr<StormByte::Logger::Log> log,
				const std::filesystem::path& logo, Anchor anchor,
				unsigned opacity = 100, int margin = 0) noexcept;

			/**
			 * @brief Logo at an absolute top-left. No Hold.
			 * @param log Shared logger. Empty pointer means no log.
			 * @param logo Path to a still image (png, jpeg, webp, bmp).
			 * @param position Top-left of the logo in frame pixels.
			 * @param opacity 0–100. 0 = no-op.
			 */
			Watermark(std::shared_ptr<StormByte::Logger::Log> log,
				const std::filesystem::path& logo,
				StormByte::Multimedia::Property::Point position,
				unsigned opacity = 100) noexcept;

			/**
			 * @brief Copy constructor (deleted). Filters are unique in the tube.
			 */
			Watermark(const Watermark& other) = delete;

			/**
			 * @brief Move constructor (deleted). Filters are unique in the tube.
			 */
			Watermark(Watermark&& other) noexcept = delete;

			/**
			 * @brief Destructor.
			 */
			~Watermark() noexcept override;

			/**
			 * @brief Copy assignment (deleted). Filters are unique in the tube.
			 * @return *this.
			 */
			Watermark& operator=(const Watermark& other) = delete;

			/**
			 * @brief Move assignment (deleted). Filters are unique in the tube.
			 * @return *this.
			 */
			Watermark& operator=(Watermark&& other) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @name Identity
			 * @{
			 */

			/**
			 * @brief Media this filter handles.
			 * @return Video. Other kinds pass through Gate.
			 */
			enum StormByte::Multimedia::Type Media() const noexcept override;

			/**
			 * @}
			 */

		protected:
			/**
			 * @name Run
			 * @{
			 */

			/**
			 * @brief Drops decoded logo and bar state from a previous run.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Reads the logo file for the coming run.
			 *
			 * A missing file logs a Warning and disables the overlay.
			 * It does not Fail the step.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Probes bars while Held; paints after Release replay.
			 * @param frame Video unit. Borrow
			 *        @ref Filter::FFmpeg::AVFrame for the live picture.
			 *
			 * Must not @ref Paint while @ref FFmpeg::Held is true.
			 */
			void Process(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Hold ceiling: keep measured bars or zero them, then Release.
			 * @param frame Last unit that would overflow the Hold queue.
			 *
			 * Called by FFmpeg when HeldFor() would exceed Hold() and
			 * again on Hold+EOF. Must Release() or the job Fails.
			 */
			void LastChance(const Pipeline::Frame& frame) noexcept override;

			/**
			 * @}
			 */

		private:
			static constexpr std::uint8_t ProbeMax = 200;	///< Hold ceiling in units

			/**
			 * @brief Turns the overlay off without failing the tube.
			 * @param why Warning text after @c Video/watermark disabled:.
			 *
			 * Sets @ref m_opacity to 0 and drops logo buffers. Later
			 * @ref Process / @ref Paint become no-ops. Frames still
			 * leave the filter.
			 */
			void DisableLogo(std::string_view why) noexcept;

			/**
			 * @brief Reads @ref m_path into @ref m_bytes.
			 * @return false if the logo was disabled.
			 */
			bool LoadFile() noexcept;

			/**
			 * @brief Decodes @ref m_bytes into @ref m_rgba on first use.
			 * @return false if the logo was disabled.
			 */
			bool DecodeLogo() noexcept;

			/**
			 * @brief Builds an 8-bit luma view of @p src for bar sampling.
			 * @param src Live RAII frame.
			 * @return Luma frame, or nullptr on Fail.
			 */
			const StormByte::Multimedia::FFmpeg::AVFrame* Luma(
				const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Samples letterbox / pillarbox on the luma view of @p src.
			 * @param src Live RAII frame.
			 * @return true if this frame updated or confirmed the rectangle.
			 */
			bool ProbeBars(const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Paints the logo and @ref FFmpeg::Save.
			 *
			 * An oversized logo calls @ref DisableLogo, not Fail.
			 */
			void Paint() noexcept;

			/**
			 * @brief Frees cached scale context and luma buffer.
			 */
			void DropScale() noexcept;

			std::filesystem::path m_path;									///< Logo file
			std::optional<Anchor> m_anchor;									///< Relative placement
			std::optional<StormByte::Multimedia::Property::Point> m_point;	///< Absolute placement
			unsigned m_opacity;												///< 0–100
			int m_margin;													///< Anchor margin
			StormByte::BinaryData m_bytes;							///< File bytes
			int m_logoWidth;												///< Decoded logo width
			int m_logoHeight;												///< Decoded logo height
			StormByte::BinaryData m_rgba;								///< Decoded RGBA8888
			bool m_loaded;													///< File read attempted
			bool m_decoded;													///< Decode attempted
			bool m_released;												///< Hold finished; replays may Paint
			int m_barTop;													///< Letterbox top
			int m_barBottom;												///< Letterbox bottom
			int m_barLeft;													///< Pillarbox left
			int m_barRight;													///< Pillarbox right
			int m_stable;													///< Consecutive unchanged letterbox probes
			int m_lumaW;													///< Cached luma width
			int m_lumaH;													///< Cached luma height
			int m_lumaFmt;													///< Cached source pixel format
			std::unique_ptr<StormByte::Multimedia::FFmpeg::Sws> m_swsLuma;	///< Cached src → gray
			std::unique_ptr<StormByte::Multimedia::FFmpeg::AVFrame> m_luma;	///< Cached GRAY8 view
	};
}
