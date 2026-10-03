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
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/hdr10.hxx>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 */
namespace StormByte::Multimedia::Pipeline {
	/**
	 * @namespace Filter
	 * @brief Frame and packet steps attached to a job or a raw pipeline.
	 */
	namespace Filter {
		class FFmpeg;
	}
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	class Frame;
}

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVDecoder;
	class AVEncoder;
	class AVFilterGraph;
	class Swr;
	class Sws;

	/**
	 * @class AVFrame
	 * @brief RAII owner of a libav AVFrame.
	 *
	 * Filter authors: clone, scale, planes, props, side data,
	 * still-image decode. Do not call av_*. Copy and @ref Clone
	 * share plane buffers (`av_frame_ref`); they are not a deep
	 * copy of planes. @c Get() and @c Detach() are not public.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVFrame: public AVPointer<::AVFrame> {
		friend class AVDecoder;
		friend class AVEncoder;
		friend class AVFilterGraph;
		friend class StormByte::Multimedia::Backend::Pipeline::Frame;
		friend class StormByte::Multimedia::Pipeline::Filter::FFmpeg;
		friend class Swr;
		friend class Sws;
		public:
			/**
			 * @brief Allocates an empty frame (av_frame_alloc).
			 */
			AVFrame() noexcept;

			/**
			 * @brief Copy. New struct; plane buffers are referenced (`av_frame_ref`).
			 * @param other Source frame.
			 *
			 * Not a deep copy of planes.
			 */
			AVFrame(const AVFrame& other) noexcept;

			/**
			 * @brief Move constructor. Transfers the libav pointer.
			 * @param other Source frame; left empty.
			 */
			AVFrame(AVFrame&& other) noexcept = default;

			/**
			 * @brief Destructor. Unreferences buffers and frees the struct.
			 */
			~AVFrame() noexcept override;

			/**
			 * @brief Copy assignment. Same as the copy constructor.
			 * @param other Source frame.
			 * @return *this.
			 */
			AVFrame& operator=(const AVFrame& other) noexcept;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source frame; left empty.
			 * @return *this.
			 */
			AVFrame& operator=(AVFrame&& other) noexcept = default;

			/**
			 * @brief True when a libav struct is owned. Plane buffers may still be empty.
			 * @return true if the wrapper holds a frame.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief New frame referencing the same planes (`av_frame_ref`).
			 * @return Referenced frame.
			 *
			 * Same as the copy constructor. Not a deep copy of planes.
			 */
			AVFrame Clone() const noexcept;

			/**
			 * @brief Shares buffers with @p other (`av_frame_ref`).
			 * @param other Source frame.
			 * @return false on failure.
			 */
			bool Ref(const AVFrame& other) noexcept;

			/**
			 * @brief Copies samples/planes from @p other (`av_frame_copy`).
			 * @param other Source with matching geometry.
			 * @return false on failure.
			 */
			bool Copy(const AVFrame& other) noexcept;

			/**
			 * @brief Copies metadata from @p other (`av_frame_copy_props`).
			 * @param other Source frame.
			 * @return false on failure.
			 */
			bool CopyProps(const AVFrame& other) noexcept;

			/**
			 * @brief Allocates plane buffers (`av_frame_get_buffer`).
			 * @param align Alignment in bytes. 0 = FFmpeg default.
			 * @return false on failure.
			 */
			bool GetBuffer(int align = 0) noexcept;

			/**
			 * @brief Sets video geometry and format, then allocates planes.
			 * @param width Width in pixels.
			 * @param height Height in pixels.
			 * @param format `AVPixelFormat` as int.
			 * @param align Alignment in bytes. 0 = FFmpeg default.
			 * @return false on failure.
			 */
			bool AllocVideo(int width, int height, int format, int align = 0) noexcept;

			/**
			 * @brief Sets audio samples, format and layout, then allocates planes.
			 * @param nb_samples Sample count.
			 * @param format `AVSampleFormat` as int.
			 * @param layout Channel layout.
			 * @param sample_rate Sample rate in Hz.
			 * @return false on failure.
			 */
			bool AllocAudio(int nb_samples, int format, const AVChannelLayout& layout, int sample_rate) noexcept;

			/**
			 * @brief Unreferences frame buffers (`av_frame_unref`).
			 */
			void Unref() noexcept;

			/**
			 * @brief Frame width in pixels.
			 * @return Width, or 0.
			 */
			int Width() const noexcept;

			/**
			 * @brief Sets frame width. Does not reallocate planes.
			 * @param width Width in pixels.
			 */
			void Width(int width) noexcept;

			/**
			 * @brief Frame height in pixels.
			 * @return Height, or 0.
			 */
			int Height() const noexcept;

			/**
			 * @brief Sets frame height. Does not reallocate planes.
			 * @param height Height in pixels.
			 */
			void Height(int height) noexcept;

			/**
			 * @brief Pixel or sample format.
			 * @return Format as int, or `AV_PIX_FMT_NONE`.
			 */
			int Format() const noexcept;

			/**
			 * @brief Sets pixel or sample format. Does not reallocate planes.
			 * @param format Format as int.
			 */
			void Format(int format) noexcept;

			/**
			 * @brief Whether the pixel format is hardware-backed.
			 * @return true if planes are not CPU-readable.
			 */
			bool Hardware() const noexcept;

			/**
			 * @brief `AV_PIX_FMT_NONE` as int.
			 * @return Format token.
			 */
			static int FormatNone() noexcept;

			/**
			 * @brief `AV_PIX_FMT_GRAY8` as int.
			 * @return Format token.
			 */
			static int FormatGray8() noexcept;

			/**
			 * @brief `AV_PIX_FMT_RGBA` as int.
			 * @return Format token.
			 */
			static int FormatRgba() noexcept;

			/**
			 * @brief `AV_PIX_FMT_YUV420P` as int.
			 * @return Format token.
			 */
			static int FormatYUV420P() noexcept;

			/**
			 * @brief Planar video family, ignoring bit depth.
			 *
			 * Packed RGB/YUV and hardware formats are @c Unknown.
			 * Gray is single-component (`YUV400` for libvmaf).
			 */
			enum class VideoLayout: std::uint8_t {
				Unknown,	///< Not a software planar YUV/gray frame.
				Gray,		///< One component (GRAY8 / GRAY10 / …).
				Yuv420,		///< 4:2:0 (8 / 10 / 12).
				Yuv422,		///< 4:2:2 (8 / 10).
				Yuv444		///< 4:4:4 (8 / 10).
			};

			/**
			 * @brief Resampling backend for @ref ScaleTo.
			 *
			 * @c Zimg is the default. @c Sws is libswscale and is
			 * required for packed RGB.
			 */
			enum class Scaler: std::uint8_t {
				Zimg,		///< zimg graph (default).
				Sws			///< libswscale.
			};

			/**
			 * @brief Resample kernel for @ref ScaleTo.
			 *
			 * Every value maps to both zimg and libswscale.
			 * @c Default is the backend native default (zimg:
			 * bicubic luma / bilinear chroma; Sws: bilinear).
			 */
			enum class Resample: std::uint8_t {
				Default,	///< Backend default.
				Point,		///< Nearest neighbour.
				Bilinear,	///< Bilinear.
				Bicubic,	///< Bicubic.
				Spline,		///< Spline (zimg spline36 / SWS_SPLINE).
				Lanczos		///< Lanczos.
			};

			/**
			 * @brief Planar layout of this frame.
			 * @return Family, or @c Unknown if empty / packed / hw.
			 */
			VideoLayout Layout() const noexcept;

			/**
			 * @brief Decodes one still image from a compressed buffer.
			 * @param data File bytes (PNG / JPEG / WebP / BMP).
			 * @param size Byte count.
			 * @param hint Path or extension (`.png`, `logo.jpg`). Empty tries JPEG.
			 * @return Frame with planes, or an empty wrapper (`!frame`) on failure.
			 *
			 * Opens a decoder, sends one packet, receives one frame.
			 * Does not convert pixel format; call @ref ScaleTo for RGBA / GRAY8.
			 * @p data must remain valid for the duration of the call.
			 */
			static AVFrame DecodeImage(const std::uint8_t* data, std::size_t size,
				std::string_view hint = {}) noexcept;

			/**
			 * @brief Presentation timestamp in stream ticks.
			 * @return pts, or AV_NOPTS_VALUE.
			 */
			std::int64_t Pts() const noexcept;

			/**
			 * @brief Sets presentation timestamp in stream ticks.
			 * @param pts Presentation timestamp, or AV_NOPTS_VALUE.
			 */
			void Pts(std::int64_t pts) noexcept;

			/**
			 * @brief Best-effort timestamp (`best_effort_timestamp`).
			 * @return Timestamp, or AV_NOPTS_VALUE.
			 */
			std::int64_t BestEffortTimestamp() const noexcept;

			/**
			 * @brief Sets the best-effort timestamp.
			 * @param ts Timestamp, or AV_NOPTS_VALUE.
			 */
			void BestEffortTimestamp(std::int64_t ts) noexcept;

			/**
			 * @brief Duration in stream ticks.
			 * @return Duration, or 0.
			 */
			std::int64_t DurationTicks() const noexcept;

			/**
			 * @brief Sets duration in stream ticks.
			 * @param duration Duration, or 0.
			 */
			void DurationTicks(std::int64_t duration) noexcept;

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
			 * @brief Chroma sample location.
			 * @return `AVChromaLocation` as int.
			 */
			int ChromaLocation() const noexcept;

			/**
			 * @brief Sets chroma sample location.
			 * @param location `AVChromaLocation` as int.
			 */
			void ChromaLocation(int location) noexcept;

			/**
			 * @brief Sample aspect ratio.
			 * @return SAR, or `{0, 1}` if unknown.
			 */
			AVRational SampleAspectRatio() const noexcept;

			/**
			 * @brief Sets sample aspect ratio.
			 * @param sar Pixel aspect ratio.
			 */
			void SampleAspectRatio(AVRational sar) noexcept;

			/**
			 * @brief Picture type (`I`/`P`/`B`/…).
			 * @return `AVPictureType` as int.
			 */
			int PictType() const noexcept;

			/**
			 * @brief Sets picture type.
			 * @param type `AVPictureType` as int.
			 */
			void PictType(int type) noexcept;

			/**
			 * @brief Whether this is a key frame.
			 * @return true for IDR / key frames.
			 */
			bool KeyFrame() const noexcept;

			/**
			 * @brief Marks the frame as a key frame or not.
			 * @param key true for a key frame.
			 */
			void KeyFrame(bool key) noexcept;

			/**
			 * @brief Extra delay in field durations (`repeat_pict`). Yadif / bwdif.
			 * @return Repeat count, or 0.
			 */
			int RepeatPict() const noexcept;

			/**
			 * @brief Sets `repeat_pict`.
			 * @param repeat Extra field delays.
			 */
			void RepeatPict(int repeat) noexcept;

			/**
			 * @brief Whether the frame is interlaced.
			 * @return true for interlaced content.
			 */
			bool Interlaced() const noexcept;

			/**
			 * @brief Marks the frame as interlaced or progressive.
			 * @param interlaced true for interlaced.
			 * @param top_first true if the top field is first.
			 */
			void Interlaced(bool interlaced, bool top_first) noexcept;

			/**
			 * @brief Whether the top field is first (interlaced).
			 * @return true if top field first.
			 */
			bool TopFieldFirst() const noexcept;

			/**
			 * @brief Left crop in pixels.
			 * @return Crop, or 0.
			 */
			int CropLeft() const noexcept;

			/**
			 * @brief Right crop in pixels.
			 * @return Crop, or 0.
			 */
			int CropRight() const noexcept;

			/**
			 * @brief Top crop in pixels.
			 * @return Crop, or 0.
			 */
			int CropTop() const noexcept;

			/**
			 * @brief Bottom crop in pixels.
			 * @return Crop, or 0.
			 */
			int CropBottom() const noexcept;

			/**
			 * @brief Sets crop window. Does not apply it.
			 * @param left Left crop in pixels.
			 * @param right Right crop in pixels.
			 * @param top Top crop in pixels.
			 * @param bottom Bottom crop in pixels.
			 */
			void Crop(int left, int right, int top, int bottom) noexcept;

			/**
			 * @brief Applies the crop window (`av_frame_apply_cropping`).
			 * @param flags `AV_FRAME_CROP_*` flags.
			 * @return false on failure.
			 */
			bool ApplyCropping(int flags = 0) noexcept;

			/**
			 * @brief Number of video components or audio channels.
			 * @return Plane/component count, or 0.
			 */
			int PlaneCount() const noexcept;

			/**
			 * @brief Width of @p plane accounting for chroma subsampling.
			 * @param plane Plane index. 0 is luma / packed.
			 * @return Width in samples.
			 */
			int PlaneWidth(int plane) const noexcept;

			/**
			 * @brief Height of @p plane accounting for chroma subsampling.
			 * @param plane Plane index. 0 is luma / packed.
			 * @return Height in samples.
			 */
			int PlaneHeight(int plane) const noexcept;

			/**
			 * @brief Bits per component from the pixel descriptor.
			 * @return Depth, or 8 if unknown.
			 */
			int BitsPerComponent() const noexcept;

			/**
			 * @brief Pixel or sample format name.
			 * @return FFmpeg name, or `"?"`.
			 */
			const char* FormatName() const noexcept;

			/**
			 * @brief Const pointer to plane @p plane.
			 * @param plane Plane index.
			 * @return Pointer, or nullptr.
			 */
			const uint8_t* Data(int plane) const noexcept;

			/**
			 * @brief Mutable pointer to plane @p plane.
			 * @param plane Plane index.
			 * @return Pointer, or nullptr.
			 */
			uint8_t* Data(int plane) noexcept;

			/**
			 * @brief Bytes per row for @p plane, including padding.
			 * @param plane Plane index.
			 * @return Linesize, or 0.
			 */
			int Linesize(int plane) const noexcept;

			/**
			 * @brief Audio sample count.
			 * @return Samples, or 0.
			 */
			int NbSamples() const noexcept;

			/**
			 * @brief Sets audio sample count. Does not reallocate.
			 * @param samples Sample count.
			 */
			void NbSamples(int samples) noexcept;

			/**
			 * @brief Audio sample rate in Hz.
			 * @return Rate, or 0.
			 */
			int SampleRate() const noexcept;

			/**
			 * @brief Sets audio sample rate.
			 * @param rate Sample rate in Hz.
			 */
			void SampleRate(int rate) noexcept;

			/**
			 * @brief Audio channel count.
			 * @return Channels, or 0.
			 */
			int Channels() const noexcept;

			/**
			 * @brief Channel layout owned by this frame.
			 * @return Layout, or nullptr.
			 */
			AVChannelLayout ChannelLayout() const noexcept;

			/**
			 * @brief Replaces the channel layout.
			 * @param layout Source layout.
			 * @return false on failure.
			 */
			bool CopyChannelLayout(const AVChannelLayout& layout) noexcept;

			/**
			 * @brief Planar audio / video data pointers (`extended_data`).
			 * @return Pointer array, or nullptr.
			 */
			uint8_t** ExtendedData() noexcept;

			/**
			 * @brief Const planar data pointers (`extended_data`).
			 * @return Pointer array, or nullptr.
			 */
			const uint8_t* const* ExtendedData() const noexcept;

			/**
			 * @brief Looks up side data.
			 * @param type `AVFrameSideDataType` value.
			 * @return Side data pointer, or nullptr.
			 */
			const AVFrameSideData* SideData(int type) const noexcept;

			/**
			 * @brief Looks up mutable side data.
			 * @param type `AVFrameSideDataType` value.
			 * @return Side data pointer, or nullptr.
			 */
			AVFrameSideData* SideData(int type) noexcept;

			/**
			 * @brief Removes side data of @p type.
			 * @param type `AVFrameSideDataType` value.
			 */
			void RemoveSideData(int type) noexcept;

			/**
			 * @brief Number of side-data entries.
			 * @return Count, or 0.
			 */
			int SideDataCount() const noexcept;

			/**
			 * @brief Side data at @p index.
			 * @param index Entry index.
			 * @return Pointer, or nullptr.
			 */
			const AVFrameSideData* SideDataAt(int index) const noexcept;

			/**
			 * @brief Allocates side data of @p type (`av_frame_new_side_data`).
			 * @param type `AVFrameSideDataType` value.
			 * @param size Payload size in bytes.
			 * @return Payload pointer, or nullptr on failure.
			 */
			uint8_t* NewSideData(int type, int size) noexcept;

			/**
			 * @brief Packs planar video/audio into @p out without linesize padding.
			 * @param out Destination (cleared first).
			 */
			void CopyPrimaryBuffer(StormByte::BinaryData& out) const noexcept;

			/**
			 * @brief Writes mastering display and content light from @p hdr10.
			 * @param hdr10 High-level HDR10 bag.
			 */
			void WriteHdr10(const StormByte::Multimedia::Property::HDR10& hdr10) noexcept;

			/**
			 * @brief Copies raw SEI/side-data blobs onto the frame.
			 * @param attachments High-level side data.
			 */
			void WriteSideData(const std::vector<StormByte::Multimedia::Pipeline::SideData>& attachments) noexcept;

			/**
			 * @brief Scales this frame into @p dst (allocates @p dst if empty).
			 * @param dst Destination. Pixel format is taken from @p dst if set,
			 *        otherwise from this frame.
			 * @param dst_w Destination width.
			 * @param dst_h Destination height.
			 * @param filter Resample kernel. @c Default is the backend default.
			 * @param scaler Backend. @c Zimg is the default. Packed RGB
			 *        requires @c Sws.
			 * @return false on failure. @c Zimg does not fall back to @c Sws.
			 */
			bool ScaleTo(AVFrame& dst, int dst_w, int dst_h,
				Resample filter = Resample::Default,
				Scaler scaler = Scaler::Zimg) const noexcept;

			/**
			 * @brief Adopts @p raw. Previous frame is freed.
			 * @param raw libav frame, or nullptr.
			 */
			void Reset(::AVFrame* raw) noexcept;

		private:
			/**
			 * @brief Frees the frame (`av_frame_free`).
			 */
			void Free() noexcept override;

			using AVPointer<::AVFrame>::Get;
			using AVPointer<::AVFrame>::Detach;
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::AVFrame>;
}
