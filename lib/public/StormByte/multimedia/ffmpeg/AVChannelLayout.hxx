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

#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/string.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	struct Convert;
	class AVCodecParameters;
	class AVDecoder;
	class AVEncoder;
	class AVFrame;
	class Swr;

	/**
	 * @class AVChannelLayout
	 * @brief RAII `::AVChannelLayout` (`av_channel_layout_copy` / `uninit`).
	 *
	 * Named like the C struct so `AVChannelLayout stereo(2)` reads as
	 * `av_channel_layout_default(&stereo, 2)`. The C type is `::AVChannelLayout`.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVChannelLayout {
		friend struct Convert;
		friend class AVCodecParameters;
		friend class AVDecoder;
		friend class AVEncoder;
		friend class AVFrame;
		friend class Swr;
		public:
			/**
			 * @brief Empty layout.
			 */
			AVChannelLayout() noexcept;

			/**
			 * @brief Default layout for @p channels (`av_channel_layout_default`).
			 * @param channels Channel count.
			 */
			explicit AVChannelLayout(int channels) noexcept;

			/**
			 * @brief Deep copy (`av_channel_layout_copy`).
			 * @param other Source layout.
			 */
			AVChannelLayout(const AVChannelLayout& other) noexcept;

			/**
			 * @brief Move constructor. @p other is left empty.
			 * @param other Source layout.
			 */
			AVChannelLayout(AVChannelLayout&& other) noexcept;

			/**
			 * @brief Destructor. `av_channel_layout_uninit` + free.
			 */
			~AVChannelLayout() noexcept;

			/**
			 * @brief Deep copy assignment.
			 * @param other Source layout.
			 * @return *this.
			 */
			AVChannelLayout& operator=(const AVChannelLayout& other) noexcept;

			/**
			 * @brief Move assignment. @p other is left empty.
			 * @param other Source layout.
			 * @return *this.
			 */
			AVChannelLayout& operator=(AVChannelLayout&& other) noexcept;

			/**
			 * @brief Default layout for @p channels.
			 * @param channels Channel count.
			 * @return Layout, or empty on failure.
			 */
			static AVChannelLayout Default(int channels) noexcept;

			/**
			 * @brief Whether a layout is stored.
			 * @return true after a successful Default / copy.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Channel count (`nb_channels`).
			 * @return Count, or 0.
			 */
			int NbChannels() const noexcept;

			/**
			 * @brief Native bitmask (`u.mask`) when `order` is native.
			 * @return Mask, or 0.
			 */
			std::uint64_t Mask() const noexcept;

			/**
			 * @brief `AVChannelOrder` as int.
			 * @return Order, or 0.
			 */
			int Order() const noexcept;

			/**
			 * @brief FFmpeg layout name (`av_channel_layout_describe`).
			 * @return Name such as @c "stereo", or empty if there is no layout.
			 */
			Safe::String Describe() const noexcept;

			/**
			 * @brief `av_channel_layout_compare` == 0.
			 * @param other Other layout.
			 * @return true if equal.
			 */
			bool operator==(const AVChannelLayout& other) const noexcept;

			/**
			 * @brief Inequality.
			 * @param other Other layout.
			 * @return true if not equal.
			 */
			bool operator!=(const AVChannelLayout& other) const noexcept;

		private:
			::AVChannelLayout* m_raw = nullptr;	///< Owned C layout

			/**
			 * @brief Allocates a zeroed C layout if needed.
			 */
			void Ensure() noexcept;

			/**
			 * @brief Releases the C layout.
			 */
			void Free() noexcept;

			/**
			 * @brief Const C layout.
			 * @return Pointer, or nullptr.
			 */
			const ::AVChannelLayout* Get() const noexcept;

			/**
			 * @brief Mutable C layout.
			 * @return Pointer, or nullptr.
			 */
			::AVChannelLayout* Get() noexcept;
	};
}

/**
 * @brief Conditional provider contract: layout copies and release use FFmpeg out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVChannelLayout);
