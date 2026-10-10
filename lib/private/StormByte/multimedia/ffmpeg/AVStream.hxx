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

#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/type_traits/safe.hxx>

#include <chrono>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVCodecParameters;

	/**
	 * @class AVStream
	 * @brief Non-owning view of an ::AVStream (owned by AVFormatContext).
		 * @note Copies borrow the same stream. The owning format context must outlive
		 * every view. Providers must remain loaded and use a compatible C++ and FFmpeg ABI.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE AVStream {
		public:
						/**
						 * @brief Constructs an empty non-owning view.
						 */
			AVStream() noexcept = default;

			/**
			 * @brief Binds a raw stream pointer (not owned).
			 * @param stream Raw stream pointer.
			 */
			explicit AVStream(::AVStream* stream) noexcept;

			/**
						 * @brief Copies the non-owning stream binding.
						 * @param other Source view; ownership is not transferred.
			 */
			AVStream(const AVStream& other) noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param other Source view.
			 * @note Both views retain the borrowed binding; no resource ownership moves.
			 */
			AVStream(AVStream&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~AVStream() noexcept = default;

			/**
						 * @brief Copies the non-owning stream binding.
						 * @param other Source view; ownership is not transferred.
			 * @return *this.
			 */
			AVStream& operator=(const AVStream& other) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param other Source view.
			 * @return *this.
			 * @note Both views retain the borrowed binding; no resource ownership moves.
			 */
			AVStream& operator=(AVStream&& other) noexcept = default;

			/**
			 * @brief Orders by stream index.
			 * @param other Other stream.
			 * @return true if this index is less.
			 */
			bool operator<(const AVStream& other) const noexcept;

			/**
			 * @brief Stream index.
			 * @return Index, or -1.
			 */
			int Index() const noexcept;

			/**
			 * @brief Stream media type.
			 * @return AVMediaType as int.
			 */
			int Type() const noexcept;

			/**
			 * @brief Copy of codec parameters.
			 * @return Parameters wrapper.
			 */
			AVCodecParameters CodecParameters() const noexcept;

			/**
			 * @brief Stream time base.
			 * @return Time base.
			 */
			AVRational TimeBase() const noexcept;

			/**
			 * @brief Stream duration in nanoseconds.
			 * @return Duration, or empty if unknown.
			 */
			Safe::Optional<Property::Duration> Duration() const noexcept;

			/**
			 * @brief Estimated FPS (avg or r_frame_rate).
			 * @return FPS, or 0.
			 */
			double FrameRate() const noexcept;

			/**
			 * @brief Estimated FPS as a rational (avg_frame_rate, else r_frame_rate).
			 * @return `{num, den}` with den &gt; 0. `{0, 1}` if unknown.
			 */
			AVRational FrameRateRational() const noexcept;

			/**
			 * @brief Pixel aspect ratio (`codecpar`, else stream `sample_aspect_ratio`).
			 * @return `{num, den}` with den > 0. `{0, 1}` if unknown.
			 */
			AVRational SampleAspectRatio() const noexcept;

			/**
			 * @brief Looks up a stream metadata tag.
			 * @param key Dictionary key (e.g. `"language"`).
			 * @return Owned value, or empty if missing; independent of the stream lifetime.
			 */
			Safe::Optional<Safe::String> Tag(const char* key) const noexcept;

			/**
			 * @brief Raw FFmpeg disposition bits.
			 * @return `AV_DISPOSITION_*` mask, or 0.
			 */
			int Disposition() const noexcept;

			/**
			 * @brief Raw FFmpeg stream (non-owning).
			 * @return Pointer, or nullptr.
			 */
			::AVStream* Raw() const noexcept;

		private:
			::AVStream* m_stream = nullptr;	///< Non-owning
	};
}

/**
 * @brief Conditional borrowed-view contract: copying and movement never own stream resources.
 * @note The format context must outlive every view, and providers must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVStream);
