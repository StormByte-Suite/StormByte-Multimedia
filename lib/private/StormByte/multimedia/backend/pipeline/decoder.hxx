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

#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/visibility.h>

#include <memory>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	/**
	 * @class Decoder
	 * @brief Decode backend behind one public Decoder.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE Decoder {
		public:
			/**
			 * @brief Destructor.
			 */
			virtual ~Decoder() noexcept = default;

			/**
			 * @brief Copy constructor.
			 * @param other Source backend.
			 */
			Decoder(const Decoder& other) = delete;

			/**
			 * @brief Copy assignment.
			 * @param other Source backend.
			 * @return *this.
			 */
			Decoder& operator=(const Decoder& other) = delete;

			/**
			 * @brief Whether the FFmpeg decoder is open.
			 * @return true after a successful open.
			 */
			virtual bool IsOpen() const noexcept = 0;

			/**
			 * @brief Sends one compressed packet.
			 * @param owner Public decoder.
			 * @param packet Compressed packet. Not empty.
			 * @return true if libav accepted it. false if the codec is full.
			 */
			virtual bool Send(StormByte::Multimedia::Pipeline::Decoder& owner,
				const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept = 0;

			/**
			 * @brief Receives one decoded frame.
			 * @param owner Public decoder.
			 * @return Frame with Producer::Decoder, or empty if none ready.
			 */
			virtual StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame> Receive(
				StormByte::Multimedia::Pipeline::Decoder& owner) noexcept = 0;

			/**
			 * @brief Signals EOF to libavcodec. Drain with Receive afterwards.
			 * @param owner Public decoder.
			 */
			virtual void Flush(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept = 0;

			/**
			 * @brief Drain, then return the codec to a clean open state.
			 * @param owner Public decoder.
			 * @return false if owner.Fail() was called.
			 *
			 * Same backend object. Used after Measure, before ordinary
			 * Process continues on the same tube.
			 */
			virtual bool Reset(StormByte::Multimedia::Pipeline::Decoder& owner) noexcept = 0;

		protected:
			/**
			 * @brief Default constructor.
			 */
			Decoder() noexcept = default;

			/**
			 * @brief Move constructor.
			 * @param other Backend to take.
			 */
			Decoder(Decoder&& other) noexcept = default;

			/**
			 * @brief Move assignment.
			 * @param other Backend to take.
			 * @return *this.
			 */
			Decoder& operator=(Decoder&& other) noexcept = default;

			/**
			 * @brief Binds a decoded handle onto @p frame through the public decoder.
			 * @param owner Public decoder.
			 * @param frame Public unit.
			 * @param holder Backend frame. May be empty.
			 */
			void BindFrame(StormByte::Multimedia::Pipeline::Decoder& owner,
				StormByte::Multimedia::Pipeline::Frame& frame,
				std::unique_ptr<Frame> holder) noexcept;

			/**
			 * @brief Closes a subtitle cue through the public decoder.
			 * @param owner Public decoder.
			 * @param frame Public unit.
			 * @param duration Cue length.
			 */
			void CloseCue(StormByte::Multimedia::Pipeline::Decoder& owner,
				StormByte::Multimedia::Pipeline::Frame& frame,
				StormByte::Multimedia::Property::Duration duration) noexcept;
	};
}
