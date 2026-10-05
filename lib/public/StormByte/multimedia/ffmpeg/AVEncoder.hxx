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

#include <StormByte/multimedia/ffmpeg/AVBSFPipeline.hxx>
#include <StormByte/multimedia/ffmpeg/AVChannelLayout.hxx>
#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/safe/map.hxx>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVCodecParameters;
	class AVFormatContext;
	class AVFrame;
	class AVPacket;
	class AVSubtitle;

	/**
	 * @class AVEncoder
	 * @brief RAII encoder context with optional BSF pipeline.
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVEncoder: public AVPointer<::AVCodecContext> {
		public:
			/**
			 * @brief Copy constructor (deleted).
			 */
			AVEncoder(const AVEncoder&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source encoder.
			 */
			AVEncoder(AVEncoder&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~AVEncoder() noexcept override;

			/**
			 * @brief Copy assignment (deleted).
			 * @return *this.
			 */
			AVEncoder& operator=(const AVEncoder&) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Source encoder.
			 * @return *this.
			 */
			AVEncoder& operator=(AVEncoder&& other) noexcept;

			/**
			 * @brief Opens an encoder; may attach BSF from @p fmt.
			 * @param codec Encoder codec.
			 * @param params Stream codec parameters.
			 * @param fmt Format context (for BSF decision).
			 * @param stream_index Stream index.
			 * @return Encoder or EncoderError.
			 */
			static ExpectedAVEncoder Open(AVCodec* codec, const AVCodecParameters& params, const AVFormatContext& fmt, int stream_index) noexcept;

			/**
			 * @brief Opens an encoder without a muxer (no BSF).
			 * @param codec Encoder codec.
			 * @param params Stream codec parameters.
			 * @param stream_index Output track index.
			 * @param options av_opt_set pairs applied before avcodec_open2.
			 * @param time_base Encoder time base. Ignored if num/den are not positive.
			 * @return Encoder or EncoderError.
			 */
			static ExpectedAVEncoder Open(AVCodec* codec, const AVCodecParameters& params, int stream_index,
				const Safe::Map<Safe::String, Safe::String>& options, AVRational time_base) noexcept;

			/**
			 * @brief Sends a frame to the encoder.
			 * @param frame Source frame.
			 * @return Operation result.
			 *
			 * Captures HDR10+ (`AV_FRAME_DATA_DYNAMIC_HDR_PLUS`) and keeps the
			 * ITU-T T.35 payload keyed by PTS so ReceivePacket can emit SEI.
			 */
			FFmpeg::OperationResult SendFrame(AVFrame& frame) noexcept;

			/**
			 * @brief Encodes one text subtitle (`avcodec_encode_subtitle`).
			 * @param sub Filled subtitle.
			 * @param pkt Destination packet.
			 * @return Operation result.
			 */
			FFmpeg::OperationResult EncodeSubtitle(AVSubtitle& sub, AVPacket& pkt) noexcept;

			/**
			 * @brief Receives an encoded packet (after BSF).
			 * @param pkt Destination packet.
			 * @return Operation result.
			 *
			 * For HEVC, prepends a prefix SEI NAL with the matching HDR10+ T.35.
			 */
			FFmpeg::OperationResult ReceivePacket(AVPacket& pkt) noexcept;

			/**
			 * @brief Stream index this encoder was opened for.
			 * @return Stream index.
			 */
			int StreamIndex() const noexcept;

			/**
			 * @brief Encoder time base.
			 * @return `time_base`, or 0/1.
			 */
			AVRational TimeBase() const noexcept;

			/**
			 * @brief true if the opened codec is a subtitle encoder.
			 * @return true for subtitle codecs.
			 */
			bool IsSubtitle() const noexcept;

			/**
			 * @brief Flushes encoder and BSF.
			 */
			void Flush() noexcept;

			/**
			 * @brief Signals EOF to encoder and BSF.
			 * @return Operation result.
			 */
			FFmpeg::OperationResult SetEof() noexcept;

			/**
			 * @brief Whether a codec context is open.
			 * @return true after a successful Open().
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Opened codec id.
			 * @return `AVCodecID` as int, or `AV_CODEC_ID_NONE`.
			 */
			int CodecId() const noexcept;

			/**
			 * @brief Media type of the opened codec.
			 * @return `AVMediaType` as int.
			 */
			int CodecType() const noexcept;

			/**
			 * @brief Audio sample format.
			 * @return `AVSampleFormat` as int, or `AV_SAMPLE_FMT_NONE`.
			 */
			int SampleFmt() const noexcept;

			/**
			 * @brief Encoder frame size in samples. 0 means variable.
			 * @return Frame size, or 0.
			 */
			int FrameSize() const noexcept;

			/**
			 * @brief Audio channel count.
			 * @return Channels, or 0.
			 */
			int Channels() const noexcept;

			/**
			 * @brief Audio sample rate in Hz.
			 * @return Rate, or 0.
			 */
			int SampleRate() const noexcept;

			/**
			 * @brief Channel layout owned by the context.
			 * @return Layout, or nullptr.
			 */
			AVChannelLayout ChannelLayout() const noexcept;

			/**
			 * @brief Whether the codec uses B-frames.
			 * @return true if `has_b_frames` is set.
			 */
			bool HasBFrames() const noexcept;

			/**
			 * @brief Video framerate guessed by the encoder.
			 * @return Framerate, or `{0, 1}`.
			 */
			AVRational FrameRate() const noexcept;

			/**
			 * @brief Opened codec context for mux stream bind.
			 * @return Context, or nullptr.
			 */
			const ::AVCodecContext* Context() const noexcept;

		private:
			int m_stream_index = -1;										///< Output track
			FFmpeg::AVBSFPipeline m_bsf_pipeline;							///< Optional annex-B filter
			std::map<std::int64_t, std::vector<std::uint8_t>> m_hdrPlusT35;	///< PTS → ST 2094-40 T.35

			/**
			 * @brief Adopts an opened codec context.
			 * @param ctx Opened codec context.
			 */
			explicit AVEncoder(AVCodecContext* ctx) noexcept;

			/**
			 * @brief Frees the codec context.
			 */
			void Free() noexcept override;

			using AVPointer<::AVCodecContext>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::AVCodecContext>;
}

/**
 * @brief Conditional provider contract: codec, filter and HDR storage lifetimes stay in Multimedia.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVEncoder);
