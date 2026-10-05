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

#include <StormByte/multimedia/ffmpeg/AVEncoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/backend/pipeline/encoder.hxx>
#include <StormByte/multimedia/ocr/engine.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Encoder
 * @brief Per-media encode backends.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Encoder {
	using FFmpeg::AVRational;
	/**
	 * @class Subtitle
	 * @brief Subtitle encode backend for one public Encoder.
	 *
	 * Text, ASS or OCR. No SendFrame. A PGS show packet with no
	 * duration is held until the next cue or Flush.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Subtitle final: public StormByte::Multimedia::Backend::Pipeline::Encoder {
		public:
			/**
			 * @brief Default constructor.
			 */
			Subtitle() noexcept;

			/**
			 * @brief Destructor.
			 */
			~Subtitle() noexcept override;

			/**
			 * @brief Copy constructor.
			 * @param other Source backend.
			 */
			Subtitle(const Subtitle& other) = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Source backend.
			 * @return *this.
			 */
			Subtitle& operator=(const Subtitle& other) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Backend to take.
			 */
			Subtitle(Subtitle&& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Backend to take.
			 * @return *this.
			 */
			Subtitle& operator=(Subtitle&& other) noexcept;

			/**
			 * @brief Whether the FFmpeg encoder is open.
			 * @return true after a successful Open().
			 */
			bool IsOpen() const noexcept override;

			/**
			 * @brief Opens the subtitle encoder from the first frame.
			 * @param owner Public encoder.
			 * @param frame First subtitle frame.
			 * @return false if owner.Fail() was called.
			 */
			bool Open(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Multimedia::Pipeline::Frame& frame) noexcept override;

			/**
			 * @brief Encodes one subtitle cue. Opens lazily on first call.
			 * @param owner Public encoder.
			 * @param frame Decoded subtitle frame.
			 * @return true if the cue was accepted (or skipped empty).
			 */
			bool Push(StormByte::Multimedia::Pipeline::Encoder& owner,
				const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>& frame) noexcept override;

			/**
			 * @brief Emits any held show packet, then marks flushed.
			 * @param owner Public encoder.
			 */
			void Flush(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept override;

			/**
			 * @brief Pops one encoded subtitle packet.
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
			 * @return Rational. {1, AV_TIME_BASE} before a successful Open().
			 */
			AVRational TimeBase() const noexcept override;

		private:
			/**
			 * @brief Writes the held show cue ending at @p endNs.
			 * @param owner Public encoder.
			 * @param endNs Cue end in nanoseconds.
			 */
			void EmitHeld(StormByte::Multimedia::Pipeline::Encoder& owner,
				std::int64_t endNs) noexcept;

			std::optional<StormByte::Multimedia::FFmpeg::AVEncoder> m_encoder;		///< Opened encoder
			StormByte::Multimedia::FFmpeg::AVPacket m_scratch;						///< Encode scratch
			std::deque<StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>> m_pending;	///< Packets waiting for Mux
			AVRational m_timeBase;															///< Encoder time base
			StormByte::Multimedia::OCR::Engine m_ocr;										///< Bitmap OCR
			int m_index;																	///< Encoder::Index after Open
			bool m_flushed;																	///< Flush already called
			std::string m_heldText;															///< OCR/text of the open PGS show packet
			std::int64_t m_heldStartNs;														///< Start of m_heldText in nanoseconds
			std::int64_t m_heldPts;															///< PTS ticks of m_heldText in AV_TIME_BASE
	};
}
