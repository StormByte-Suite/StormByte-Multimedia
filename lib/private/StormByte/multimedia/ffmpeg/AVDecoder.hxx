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
#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/AVRational.hxx>
#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>
#include <StormByte/multimedia/ffmpeg/backend_typedefs.hxx>

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVFormatContext;
	class AVFrame;
	class AVPacket;

	/**
	 * @class AVDecoder
	 * @brief RAII decoder context with optional BSF pipeline.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE AVDecoder: public AVPointer<::AVCodecContext> {
		public:
			/**
			 * @brief Copy constructor (deleted).
			 */
			AVDecoder(const AVDecoder&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source decoder.
			 */
			AVDecoder(AVDecoder&& other) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			~AVDecoder() noexcept override;

			/**
			 * @brief Copy assignment (deleted).
			 * @return *this.
			 */
			AVDecoder& operator=(const AVDecoder&) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Source decoder.
			 * @return *this.
			 */
			AVDecoder& operator=(AVDecoder&& other) noexcept = default;

			/**
			 * @brief Opens a decoder from codec and parameters; may attach mp4→Annex-B BSF.
			 * @param codec Decoder codec.
			 * @param params Stream codec parameters.
			 * @param fmt Parent format context (for BSF decision).
			 * @param stream_index Stream index this decoder serves.
			 * @return Decoder or DecoderError.
			 */
			static ExpectedAVDecoder Open(AVCodec* codec, const AVCodecParameters& params, const AVFormatContext& fmt, int stream_index) noexcept;

			/**
			 * @brief Opens a decoder from codec and parameters. No BSF.
			 * @param codec Decoder codec.
			 * @param params Stream codec parameters.
			 * @param stream_index Stream index this decoder serves
			 *        (must match AVPacket::StreamIndex on SendPacket).
			 * @return Decoder or DecoderError.
			 *
			 * For Analytics encode-look: packets already left the
			 * Encoder, so mp4→Annex-B is not applied. Use the
			 * four-argument Open when the parent format is known.
			 */
			static ExpectedAVDecoder Open(AVCodec* codec, const AVCodecParameters& params, int stream_index) noexcept;

			/**
			 * @brief Sends a packet (after BSF) to the decoder.
			 * @param pkt Packet for this stream.
			 * @return Operation result.
			 */
			FFmpeg::OperationResult SendPacket(AVPacket& pkt) noexcept;

			/**
			 * @brief Receives a decoded frame.
			 * @param frame Destination frame.
			 * @return Operation result.
			 */
			FFmpeg::OperationResult ReceiveFrame(AVFrame& frame) noexcept;

			/**
			 * @brief Stream index this decoder was opened for.
			 * @return Stream index.
			 */
			int StreamIndex() const noexcept;

			/**
			 * @brief Time base used for decoded frame timestamps.
			 * @return `pkt_timebase` if set, otherwise `time_base`.
			 */
			AVRational TimeBase() const noexcept;

			/**
			 * @brief Flushes decoder and BSF buffers (does not signal EOF).
			 */
			void Flush() noexcept;

			/**
			 * @brief Signals EOF (`avcodec_send_packet(nullptr)` + BSF EOF).
			 * @return Success/EndOfFile when the null packet was accepted, TryAgain if frames must be drained first.
			 */
			FFmpeg::OperationResult SetEof() noexcept;

			/**
			 * @brief true if the opened codec is a subtitle decoder.
			 * @return true for subtitle codecs.
			 */
			bool IsSubtitle() const noexcept;

			/**
			 * @brief Decodes one subtitle packet (`avcodec_decode_subtitle2`).
			 * @param pkt Compressed packet.
			 * @param out Filled when a subtitle is produced.
			 * @return Success, TryAgain (no subtitle this packet), EndOfFile or Error.
			 */
			OperationResult DecodeSubtitle(AVPacket& pkt, FFmpeg::AVSubtitle& out) noexcept;

		private:
			int m_stream_index = -1;		///< Bound stream index
			AVBSFPipeline m_bsf_pipeline;	///< Optional BSF chain

			/**
			 * @brief Adopts an opened codec context.
			 * @param ctx Opened codec context.
			 */
			explicit AVDecoder(AVCodecContext* ctx) noexcept;

			/**
			 * @brief Allocates, copies params and avcodec_open2.
			 * @param codec Decoder codec.
			 * @param params Stream codec parameters.
			 * @param stream_index Bound stream index.
			 * @return Decoder or DecoderError. BSF is empty.
			 */
			static ExpectedAVDecoder OpenRaw(AVCodec* codec, const AVCodecParameters& params, int stream_index) noexcept;

			/**
			 * @brief Frees the codec context.
			 */
			void Free() noexcept override;

			using AVPointer<::AVCodecContext>::Get;
	};

	extern template class STORMBYTE_MULTIMEDIA_PRIVATE AVPointer<::AVCodecContext>;
}

/**
 * @brief Conditional provider contract: codec and private filter storage release out-of-line.
 * @note Multimedia, Base and FFmpeg must remain loaded with compatible ABIs.
 */
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::FFmpeg::AVDecoder);
