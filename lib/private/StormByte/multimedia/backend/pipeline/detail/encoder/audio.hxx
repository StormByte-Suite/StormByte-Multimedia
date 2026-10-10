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

#include <StormByte/multimedia/ffmpeg/AVEncoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AudioFifo.hxx>
#include <StormByte/multimedia/ffmpeg/Swr.hxx>
#include <StormByte/multimedia/backend/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>
#include <StormByte/safe/deque.hxx>
#include <StormByte/safe/optional.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Encoder
 * @brief Per-media encode backends.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Encoder {
	using FFmpeg::AVRational;
	/**
	 * @class Audio
	 * @brief Audio encode backend for one public Encoder.
	 *
	 * Converts sample format and, when the encoder rejects >5.1,
	 * downmixes to 5.1. Same sample rate. Buffers to encoder frame_size.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Audio final: public StormByte::Multimedia::Backend::Pipeline::Encoder {
		public:
			/**
			 * @brief Default constructor.
			 */
			Audio() noexcept;

			/**
			 * @brief Destructor. Frees swr and the sample fifo.
			 */
			~Audio() noexcept override;

			/**
			 * @brief Copy constructor.
			 * @param other Source backend.
			 */
			Audio(const Audio& other) = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Source backend.
			 * @return *this.
			 */
			Audio& operator=(const Audio& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Backend to take.
			 */
			Audio(Audio&& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Backend to take.
			 * @return *this.
			 */
			Audio& operator=(Audio&& other) noexcept;

			/**
			 * @brief Whether the FFmpeg encoder is open.
			 * @return true after a successful Open().
			 */
			bool IsOpen() const noexcept override;

			/**
			 * @brief Opens the audio encoder from the first frame.
			 * @param owner Public encoder.
			 * @param frame First audio frame.
			 * @return false if owner.Fail() was called.
			 */
			bool Open(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Multimedia::Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Encodes one audio frame. Opens lazily on first call.
			 * @param owner Public encoder.
			 * @param frame Decoded audio frame.
			 * @return true if the frame was ingested.
			 */
			bool Push(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>& frame) noexcept override;

			/**
			 * @brief Signals EOF, flushes the fifo and drains.
			 * @param owner Public encoder.
			 */
			void Flush(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept override;

			/**
			 * @brief Receives one packet from libav if needed, then pops the queue.
			 * @return Packet with Producer::Encoder, or empty if none ready.
			 */
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> Take() noexcept override;

			/**
			 * @brief Opened AVCodecContext, if any.
			 * @return Context, or nullptr before Open().
			 */
			const AVCodecContext* Context() const noexcept override;

			/**
			 * @brief Encoder time base after Open().
			 * @return Rational. {0,1} before Open().
			 */
			AVRational TimeBase() const noexcept override;

		private:
			/**
			 * @brief Receives one backend packet into @ref m_pending.
			 * @param owner Public encoder.
			 * @return true if a packet was queued.
			 */
			bool DrainOne(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept;

			/**
			 * @brief Builds swr and the sample fifo.
			 * @param owner Public encoder.
			 * @param src First decoded frame.
			 * @return false if owner.Fail() was called.
			 */
			bool PrepareConvert(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Converts @p src and writes samples into the fifo.
			 * @param owner Public encoder.
			 * @param src Decoded frame.
			 * @return false if owner.Fail() was called.
			 */
			bool Ingest(StormByte::Multimedia::Pipeline::Encoder& owner,
				StormByte::Multimedia::FFmpeg::AVFrame& src) noexcept;

			/**
			 * @brief Sends encoder-sized frames from the fifo.
			 * @param owner Public encoder.
			 * @param last true to send a short tail frame.
			 * @return false if owner.Fail() was called.
			 */
			bool Emit(StormByte::Multimedia::Pipeline::Encoder& owner, bool last) noexcept;

			/**
			 * @brief Fills packet timestamps when libav omits them.
			 */
			void StampOutgoing() noexcept;

			StormByte::Safe::Optional<StormByte::Multimedia::FFmpeg::AVEncoder> m_encoder;	///< Opened encoder.
			StormByte::Multimedia::FFmpeg::AVPacket m_scratch;					///< Receive scratch
			StormByte::Multimedia::FFmpeg::AVFrame m_converted;				///< Encoder-sized frame
			StormByte::Safe::Deque<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>> m_pending;	///< Base-owned packets waiting for Mux.
			StormByte::Multimedia::Pipeline::Encoder* m_owner;							///< Owner for Take/Wrap
			AVRational m_timeBase;														///< Encoder time base
			StormByte::Safe::Optional<StormByte::Multimedia::FFmpeg::Swr> m_swr;	///< Format / layout converter.
			StormByte::Safe::Optional<StormByte::Multimedia::FFmpeg::AudioFifo> m_fifo;	///< Samples waiting for frame_size.
			int m_inFormat;																///< Decoded sample format
			int m_outFormat;															///< Encoder sample format
			int m_frameSize;															///< Encoder frame_size
			int m_channels;																///< Encoder channel count
			int m_index;																///< Encoder::Index after Open
			std::int64_t m_nextPts;														///< Next encoder PTS in samples
			bool m_flushed;																///< EOF already signalled
			std::int64_t m_pktPts;														///< Next packet PTS if libav omits it
	};
}
