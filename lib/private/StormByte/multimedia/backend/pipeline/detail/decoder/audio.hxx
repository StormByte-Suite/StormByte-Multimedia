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

#include <StormByte/multimedia/ffmpeg/AVDecoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/backend/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/property/audio.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/visibility.h>

#include <memory>
#include <optional>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail::Decoder
 * @brief Per-media decode backends.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail::Decoder {
	using FFmpeg::AVRational;
	/**
	 * @class Audio
	 * @brief Audio decode backend for one public Decoder.
	 *
	 * Sample-format conversion stays in this leaf when it is needed.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Audio final: public StormByte::Multimedia::Backend::Pipeline::Decoder {
		public:
			/**
			 * @brief Adopts an opened audio decoder.
			 * @param decoder Opened AVDecoder.
			 * @param timeBase Stream time base.
			 * @param audio Stream audio properties.
			 */
			Audio(StormByte::Multimedia::FFmpeg::AVDecoder decoder,
				AVRational timeBase,
				std::optional<StormByte::Multimedia::Property::Audio> audio) noexcept;

			/**
			 * @brief Destructor.
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
			 * @brief Whether the FFmpeg decoder is open.
			 * @return true.
			 */
			bool IsOpen() const noexcept override;

			/**
			 * @brief Sends one audio packet.
			 * @param owner Public decoder.
			 * @param packet Compressed packet.
			 * @return true if libav accepted it.
			 */
			bool Send(StormByte::Multimedia::Pipeline::Decoder& owner,
				const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept override;

			/**
			 * @brief Receives one audio frame.
			 * @param owner Public decoder.
			 * @return Frame with Producer::Decoder, or empty if none ready.
			 */
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame> Receive(
				StormByte::Multimedia::Pipeline::Decoder& owner) noexcept override;

			/**
			 * @brief Signals EOF and keeps delayed frames available to Receive.
			 * @param owner Public decoder.
			 */
			void Flush(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept override;

			/**
			 * @brief Drain leftover frames, then clear codec state.
			 * @param owner Public decoder.
			 * @return false if owner.Fail() was called.
			 */
			bool Reset(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept override;

		private:
			StormByte::Multimedia::FFmpeg::AVDecoder m_decoder;				///< Opened decoder
			StormByte::Multimedia::FFmpeg::AVFrame m_scratch;				///< Receive scratch
			std::optional<StormByte::Multimedia::Property::Audio> m_audio;	///< Stream audio properties
			AVRational m_timeBase;											///< Stream time base
			bool m_flushed;													///< EOF already signalled
	};
}
