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

#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVFrame;

	/**
	 * @class Swr
	 * @brief RAII `SwrContext` for audio convert / resample.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Swr: public AVPointer<::SwrContext> {
		public:
			/**
			 * @brief Move constructor. Transfers the resampler.
			 * @param other Source resampler; left empty.
			 */
			Swr(Swr&& other) noexcept = default;

			/**
			 * @brief Destructor. Frees the `SwrContext`.
			 */
			~Swr() noexcept override;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source resampler; left empty.
			 * @return *this.
			 */
			Swr& operator=(Swr&& other) noexcept = default;

			/**
			 * @brief Whether a resampler context is open.
			 * @return true if `Convert` can run.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Opens a resampler. Empty wrapper on failure.
			 * @param out_layout Destination channel layout.
			 * @param out_fmt Destination `AVSampleFormat` as int.
			 * @param out_rate Destination sample rate in Hz.
			 * @param in_layout Source channel layout.
			 * @param in_fmt Source `AVSampleFormat` as int.
			 * @param in_rate Source sample rate in Hz.
			 * @return Open resampler, or empty on failure.
			 */
			static Swr Open(const AVChannelLayout& out_layout, int out_fmt, int out_rate,
				const AVChannelLayout& in_layout, int in_fmt, int in_rate) noexcept;

			/**
			 * @brief Converts @p src into @p dst (`swr_convert_frame`).
			 * @param src Source frame.
			 * @param dst Destination frame (geometry already set).
			 * @return false on failure.
			 */
			bool Convert(const AVFrame& src, AVFrame& dst) const noexcept;

			/**
			 * @brief Flushes delayed samples into @p dst (`swr_convert_frame` with a null source).
			 * @param dst Destination frame (geometry already set).
			 * @return false on failure.
			 */
			bool Drain(AVFrame& dst) const noexcept;

			/**
			 * @brief Buffered delay in units of 1/@p base seconds (`swr_get_delay`).
			 * @param base Unit. Pass the input sample rate to get samples.
			 * @return Delay, or 0.
			 */
			std::int64_t Delay(std::int64_t base) const noexcept;

		private:
			/**
			 * @brief Adopts an opened `SwrContext`.
			 * @param ctx libswresample context, or nullptr.
			 */
			explicit Swr(::SwrContext* ctx) noexcept;

			/**
			 * @brief Deleted. Use @ref Open; an empty resampler is not useful.
			 */
			Swr() = delete;

			/**
			 * @brief Frees the resampler (`swr_free`).
			 */
			void Free() noexcept override;

			using AVPointer<::SwrContext>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::SwrContext>;
}

/**
 * @brief Conditional provider contract: resampler storage is released through FFmpeg out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::Swr);
