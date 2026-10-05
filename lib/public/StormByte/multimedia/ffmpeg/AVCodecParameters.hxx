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
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/property/hdr10.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	/**
	 * @class AVCodecParameters
	 * @brief Deep-copying RAII wrapper for ::AVCodecParameters.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVCodecParameters: public AVPointer<::AVCodecParameters> {
		friend class AVBSF;
		friend class AVDecoder;
		friend class AVEncoder;
		friend class AVFormatContext;
		public:
			/**
			 * @brief Allocates and optionally copies from @p par.
			 * @param par Source parameters (may be null).
			 */
			explicit AVCodecParameters(::AVCodecParameters* par) noexcept;

			/**
			 * @brief Copy constructor (deep copy).
			 * @param other Source parameters.
			 */
			AVCodecParameters(const AVCodecParameters& other) noexcept;

			/**
			 * @brief Move constructor.
			 * @param other Source parameters.
			 */
			AVCodecParameters(AVCodecParameters&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~AVCodecParameters() noexcept override;

			/**
			 * @brief Copy assignment (deep copy).
			 * @param other Source parameters.
			 * @return *this.
			 */
			AVCodecParameters& operator=(const AVCodecParameters& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Source parameters.
			 * @return *this.
			 */
			AVCodecParameters& operator=(AVCodecParameters&& other) noexcept = default;

			/**
			 * @brief FFmpeg codec id.
			 * @return `AVCodecID` as int, or `AV_CODEC_ID_NONE`.
			 */
			int CodecId() const noexcept;

			/**
			 * @brief Sets the FFmpeg codec id.
			 * @param id `AVCodecID` as int.
			 */
			void CodecId(int id) noexcept;

			/**
			 * @brief Stream bitrate in bits per second.
			 * @return Bitrate, or 0 if unknown.
			 */
			std::int64_t BitRate() const noexcept;

			/**
			 * @brief Sets stream bitrate in bits per second.
			 * @param bit_rate Bitrate. 0 = unknown.
			 */
			void BitRate(std::int64_t bit_rate) noexcept;

			/**
			 * @brief Frame width in pixels.
			 * @return Width, or 0.
			 */
			int Width() const noexcept;

			/**
			 * @brief Sets frame width.
			 * @param width Width in pixels.
			 */
			void Width(int width) noexcept;

			/**
			 * @brief Frame height in pixels.
			 * @return Height, or 0.
			 */
			int Height() const noexcept;

			/**
			 * @brief Sets frame height.
			 * @param height Height in pixels.
			 */
			void Height(int height) noexcept;

			/**
			 * @brief Pixel aspect ratio (`sample_aspect_ratio`).
			 * @return `{num, den}`, or `{0, 1}` if unknown.
			 */
			AVRational SampleAspectRatio() const noexcept;

			/**
			 * @brief Sets pixel aspect ratio.
			 * @param sar `{num, den}` like libav.
			 */
			void SampleAspectRatio(AVRational sar) noexcept;

			/**
			 * @brief Pixel or sample format.
			 * @return Format as int, or `AV_PIX_FMT_NONE`.
			 */
			int Format() const noexcept;

			/**
			 * @brief Sets pixel or sample format.
			 * @param format Format as int.
			 */
			void Format(int format) noexcept;

			/**
			 * @brief Color range.
			 * @return `AVColorRange` as int.
			 */
			int ColorRange() const noexcept;

			/**
			 * @brief Sets color range.
			 * @param range `AVColorRange` as int.
			 */
			void ColorRange(int range) noexcept;

			/**
			 * @brief Matrix coefficients.
			 * @return `AVColorSpace` as int.
			 */
			int ColorSpace() const noexcept;

			/**
			 * @brief Sets matrix coefficients.
			 * @param space `AVColorSpace` as int.
			 */
			void ColorSpace(int space) noexcept;

			/**
			 * @brief Color primaries.
			 * @return `AVColorPrimaries` as int.
			 */
			int ColorPrimaries() const noexcept;

			/**
			 * @brief Sets color primaries.
			 * @param primaries `AVColorPrimaries` as int.
			 */
			void ColorPrimaries(int primaries) noexcept;

			/**
			 * @brief Transfer characteristics.
			 * @return `AVColorTransferCharacteristic` as int.
			 */
			int ColorTransfer() const noexcept;

			/**
			 * @brief Sets transfer characteristics.
			 * @param transfer `AVColorTransferCharacteristic` as int.
			 */
			void ColorTransfer(int transfer) noexcept;

			/**
			 * @brief Audio sample rate in Hz.
			 * @return Sample rate, or 0.
			 */
			int SampleRate() const noexcept;

			/**
			 * @brief Sets audio sample rate.
			 * @param rate Sample rate in Hz.
			 */
			void SampleRate(int rate) noexcept;

			/**
			 * @brief Audio channel count.
			 * @return Channel count, or 0.
			 */
			int Channels() const noexcept;

			/**
			 * @brief Codec profile id.
			 * @return Profile, or `AV_PROFILE_UNKNOWN`.
			 */
			int Profile() const noexcept;

			/**
			 * @brief Sets codec profile id.
			 * @param profile Profile.
			 */
			void Profile(int profile) noexcept;

			/**
			 * @brief Channel layout owned by this wrapper.
			 * @return Layout, or nullptr.
			 */
			AVChannelLayout ChannelLayout() const noexcept;

			/**
			 * @brief Replaces the layout with FFmpeg's default for @p channels.
			 * @param channels Channel count.
			 */
			void DefaultChannelLayout(int channels) noexcept;

			/**
			 * @brief Whether a parameters struct is owned.
			 * @return true if codecpar is allocated.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Replaces the channel layout.
			 * @param layout Source layout.
			 * @return false on failure.
			 */
			bool CopyChannelLayout(const AVChannelLayout& layout) noexcept;

			/**
			 * @brief Copies this into @p dest (`avcodec_parameters_copy`).
			 * @param dest Already allocated codecpar.
			 * @return false on failure.
			 */
			bool Export(::AVCodecParameters* dest) const noexcept;

			/**
			 * @brief Writes mastering display and content light onto coded side data.
			 * @param hdr10 High-level HDR10 bag.
			 */
			void WriteHdr10(const StormByte::Multimedia::Property::HDR10& hdr10) noexcept;

			/**
			 * @brief Media type.
			 * @return `AVMediaType` as int.
			 */
			int CodecType() const noexcept;

			/**
			 * @brief Sets media type.
			 * @param type `AVMediaType` as int.
			 */
			void CodecType(int type) noexcept;

		private:
			/**
			 * @brief Frees parameters (avcodec_parameters_free).
			 */
			void Free() noexcept override;

			using AVPointer<::AVCodecParameters>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::AVCodecParameters>;
}

/**
 * @brief Conditional provider contract: copies and release use FFmpeg out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVCodecParameters);
