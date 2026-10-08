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

#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVFrame;

	/**
	 * @class Sws
	 * @brief RAII `SwsContext` for video scale / convert.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Sws: public AVPointer<::SwsContext> {
		friend class AVFrame;

		public:
			/**
			 * @brief Move constructor. Transfers the scaler.
			 * @param other Source scaler; left empty.
			 */
			Sws(Sws&& other) noexcept = default;

			/**
			 * @brief Destructor. Frees the `SwsContext`.
			 */
			~Sws() noexcept override;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source scaler; left empty.
			 * @return *this.
			 */
			Sws& operator=(Sws&& other) noexcept = default;

			/**
			 * @brief Whether a scaler context is open.
			 * @return true if `sws_scale` can run.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief `SWS_BILINEAR`. Default when @ref Open / @ref Ensure get 0.
			 */
			static int Bilinear() noexcept;

			/**
			 * @brief `SWS_BICUBIC`.
			 */
			static int Bicubic() noexcept;

			/**
			 * @brief `SWS_FAST_BILINEAR`.
			 */
			static int FastBilinear() noexcept;

			/**
			 * @brief `SWS_POINT`.
			 */
			static int Point() noexcept;

			/**
			 * @brief `SWS_LANCZOS`.
			 */
			static int Lanczos() noexcept;

			/**
			 * @brief Opens a scaler. Empty wrapper on failure.
			 * @param src_w Source width in pixels.
			 * @param src_h Source height in pixels.
			 * @param src_fmt Source pixel format as int.
			 * @param dst_w Destination width in pixels.
			 * @param dst_h Destination height in pixels.
			 * @param dst_fmt Destination pixel format as int.
			 * @param flags @ref Bilinear, @ref Bicubic, … 0 = @ref Bilinear.
			 * @return Open scaler, or empty on failure.
			 */
			static Sws Open(int src_w, int src_h, int src_fmt,
				int dst_w, int dst_h, int dst_fmt, int flags = 0) noexcept;

			/**
			 * @brief Reopens if geometry, format or flags changed.
			 * @param src_w Source width in pixels.
			 * @param src_h Source height in pixels.
			 * @param src_fmt Source pixel format as int.
			 * @param dst_w Destination width in pixels.
			 * @param dst_h Destination height in pixels.
			 * @param dst_fmt Destination pixel format as int.
			 * @param flags @ref Bilinear, @ref Bicubic, … 0 = @ref Bilinear.
			 * @return false if the scaler could not be (re)opened.
			 */
			bool Ensure(int src_w, int src_h, int src_fmt,
				int dst_w, int dst_h, int dst_fmt, int flags = 0) noexcept;

			/**
			 * @brief Scales @p src into @p dst. Both frames must already have buffers.
			 * @param src Source frame.
			 * @param dst Destination frame.
			 * @return false on failure.
			 */
			bool Scale(const AVFrame& src, AVFrame& dst) const noexcept;

		private:
			/**
			 * @brief Adopts an opened `SwsContext`.
			 * @param ctx libswscale context, or nullptr.
			 */
			explicit Sws(::SwsContext* ctx) noexcept;

			/**
			 * @brief Empty scaler. Used by the per-thread slot in @ref AVFrame::ScaleTo.
			 */
			Sws() noexcept;

			/**
			 * @brief Frees the scaler (`sws_freeContext`).
			 */
			void Free() noexcept override;

			using AVPointer<::SwsContext>::Get;

			int m_srcW = 0;		///< Cached source width
			int m_srcH = 0;		///< Cached source height
			int m_srcFmt = 0;	///< Cached source format
			int m_dstW = 0;		///< Cached destination width
			int m_dstH = 0;		///< Cached destination height
			int m_dstFmt = 0;	///< Cached destination format
			int m_flags = 0;	///< Cached scaler flags
	};

	extern template class STORMBYTE_MULTIMEDIA_PRIVATE AVPointer<::SwsContext>;
}

/**
 * @brief Conditional provider contract: scaler storage is released through FFmpeg out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::Sws);
