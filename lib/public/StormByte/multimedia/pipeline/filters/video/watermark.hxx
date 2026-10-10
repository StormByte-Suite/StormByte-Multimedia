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
#include <StormByte/multimedia/property/point.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/binary.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <cstdint>
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
	 * @brief Overlays a still image on decoded video from a path or encoded bytes.
	 *
	 * Supply either an owned UTF-8 file path or an owned @ref StormByte::Safe::Binary
	 * containing an encoded PNG, JPEG, WebP or BMP image. File contents are read once
	 * into the filter's provider-owned binary storage; binary input is copied there
	 * when the filter is constructed. The stored source bytes survive @ref Clean so
	 * the filter can be replayed without reopening or retaining the caller's buffer.
	 *
	 * Attach with a path or an encoded byte buffer:
	 * @code
	 * job.Video(in, out).Filter<Watermark>(log, path, Anchor::BottomRight);
	 * job.Video(in, out).Filter<Watermark>(log, logoBytes, Anchor::BottomRight);
	 * @endcode
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
	 * Delays at most 200 video units while detecting letterbox bars.
	 * Output starts earlier when the bars are stable for eight frames.
	 * The logo appears from the first output frame, including those
	 * delayed during detection. @ref LastChance ends detection at
	 * the limit or at end-of-input. The source pixel format is preserved.
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
			 * @param log Safe shared logger retained by the filter. Empty means no log.
			 * @param logo UTF-8 path to a still image (png, jpeg, webp, bmp), read once during construction.
			 * @param anchor Placement relative to measured bars.
			 * @param opacity 0–100. 0 = no-op.
			 * @param margin Pixels from the anchored active edge.
			 * @note File bytes are copied into the filter's persistent @ref StormByte::Safe::Binary.
			 * A missing or unreadable file logs a Warning and disables the overlay.
			 * Allocation or path-conversion exceptions terminate because this is noexcept.
			 */
			Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				StormByte::Safe::String logo, Anchor anchor,
				unsigned opacity = 100, int margin = 0) noexcept;

			/**
			 * @brief Logo bytes at an anchor on the active picture.
			 * @param log Safe shared logger retained by the filter. Empty means no log.
			 * @param logo Encoded PNG, JPEG, WebP or BMP bytes; copied into provider-owned storage.
			 * @param anchor Placement relative to measured bars.
			 * @param opacity 0–100. 0 = no-op.
			 * @param margin Pixels from the anchored active edge.
			 * @note The encoded source is copied into Watermark's persistent @ref StormByte::Safe::Binary
			 * and remains there across @ref Clean and filter replays. Allocation failure terminates
			 * because this constructor is noexcept.
			 */
			Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				const StormByte::Safe::Binary& logo, Anchor anchor,
				unsigned opacity = 100, int margin = 0) noexcept;

			/**
			 * @brief Logo at an absolute top-left. No Hold.
			 * @param log Safe shared logger retained by the filter. Empty means no log.
			 * @param logo UTF-8 path to a still image (png, jpeg, webp, bmp), read once during construction.
			 * @param position Top-left of the logo in frame pixels.
			 * @param opacity 0–100. 0 = no-op.
			 * @note File bytes are copied into the filter's persistent @ref StormByte::Safe::Binary.
			 * A missing or unreadable file logs a Warning and disables the overlay.
			 * Allocation or path-conversion exceptions terminate because this is noexcept.
			 */
			Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				StormByte::Safe::String logo,
				StormByte::Multimedia::Property::Point position,
				unsigned opacity = 100) noexcept;

			/**
			 * @brief Logo bytes at an absolute top-left. No Hold.
			 * @param log Safe shared logger retained by the filter. Empty means no log.
			 * @param logo Encoded PNG, JPEG, WebP or BMP bytes; copied into provider-owned storage.
			 * @param position Top-left of the logo in frame pixels.
			 * @param opacity 0–100. 0 = no-op.
			 * @note The encoded source is copied into Watermark's persistent @ref StormByte::Safe::Binary
			 * and remains there across @ref Clean and filter replays. Allocation failure terminates
			 * because this constructor is noexcept.
			 */
			Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				const StormByte::Safe::Binary& logo,
				StormByte::Multimedia::Property::Point position,
				unsigned opacity = 100) noexcept;

			/**
			 * @brief Copy constructor (deleted). Filters are unique in the tube.
			 * @param other Filter that cannot be copied.
			 */
			Watermark(const Watermark& other) = delete;

			/**
			 * @brief Move constructor (deleted). Filters are unique in the tube.
			 * @param other Filter that cannot be moved.
			 */
			Watermark(Watermark&& other) noexcept = delete;

			/**
			 * @brief Destructor.
			 */
			~Watermark() noexcept override;

			/**
			 * @brief Copy assignment (deleted). Filters are unique in the tube.
			 * @param other Filter that cannot be copied.
			 * @return *this.
			 */
			Watermark& operator=(const Watermark& other) = delete;

			/**
			 * @brief Move assignment (deleted). Filters are unique in the tube.
			 * @param other Filter that cannot be moved.
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
			 * @brief Logs overlay configuration before processing video frames.
			 */
			void Setup() noexcept override;

			/**
			 * @brief Detects anchor placement, then overlays the logo on video frames.
			 * @param frame Video unit. Borrow
			 *        @ref Filter::FFmpeg::AVFrame for the live picture.
			 *
			 * Frames delayed during detection also receive the overlay.
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
			/**
			 * @brief Initializes source bytes and placement shared by public constructors.
			 * @param log Safe shared logger retained by the filter.
			 * @param path UTF-8 file path, or empty for direct binary input; consumed immediately.
			 * @param logo Encoded bytes, or empty for file input.
			 * @param anchor Optional bar-relative placement.
			 * @param position Optional absolute frame position.
			 * @param opacity Logo opacity from 0 to 100.
			 * @param margin Anchor margin in pixels.
			 * @param binaryInput Whether @p logo is the caller-provided source.
			 */
			Watermark(StormByte::Safe::Shared<StormByte::Logger::Log> log,
				StormByte::Safe::String path, const StormByte::Safe::Binary& logo,
				StormByte::Safe::Optional<Anchor> anchor,
				StormByte::Safe::Optional<StormByte::Multimedia::Property::Point> position,
				unsigned opacity, int margin, bool binaryInput) noexcept;

			static constexpr std::uint8_t ProbeMax = 200;	///< Hold ceiling in units

			/**
			 * @brief Turns the overlay off without failing the tube.
			 * @param why Borrowed warning text after @c Video/watermark disabled:;
			 * read only during this call and never retained.
			 *
			 * Sets @ref m_opacity to 0 and drops logo buffers. Later
			 * @ref Process / @ref Paint become no-ops. Frames still
			 * leave the filter.
			 */
			void DisableLogo(std::string_view why) noexcept;

			/**
			 * @brief Reads a path into the persistent source buffer during construction.
			 * @param path UTF-8 file path, consumed during this call.
			 * @return false if the file could not be opened or read.
			 */
			bool LoadFile(const StormByte::Safe::String& path) noexcept;

			/**
			 * @brief Decodes the provider-owned bytes into RGBA on first use.
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

			StormByte::Safe::Binary m_logo;									///< Persistent encoded source; retained across Clean and replays.
			StormByte::Safe::Vector<std::uint8_t> m_rgba;						///< Decoded RGBA8888 pixels.
			StormByte::Safe::Unique<StormByte::Multimedia::FFmpeg::AVFrame> m_luma;	///< Cached GRAY8 view.
			StormByte::Safe::Optional<Anchor> m_anchor;						///< Relative placement
			StormByte::Safe::Optional<StormByte::Multimedia::Property::Point> m_point;	///< Absolute placement
			unsigned m_opacity;												///< 0–100
			int m_margin;													///< Anchor margin
			int m_logoWidth;												///< Decoded logo width
			int m_logoHeight;												///< Decoded logo height
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
	};
}

/**
 * @brief Declares Watermark conditionally safe, not universally ABI-compatible.
 *
 * Consumers must use the same compatible C++ class, enum and virtual-dispatch ABI
 * and keep the Multimedia, Logger and Base creators loaded through destruction.
 * Private storage is created and destroyed out-of-line by Multimedia; Safe owners
 * retain their creator's allocation and destruction callbacks. Copy and move are deleted.
 * Owning handles must preserve creator-side destruction and deallocation of the
 * Watermark object itself; foreign-runtime deletion of provider allocations is invalid.
 * The Process parent and its inherited Step state and APIs must first satisfy their
 * Safe ownership contracts; this declaration does not certify or repair those parents.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Pipeline::Filter::Video::Watermark);
