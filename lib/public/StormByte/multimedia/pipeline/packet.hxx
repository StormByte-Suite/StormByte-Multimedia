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

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

/**
 * @namespace StormByte::Multimedia::Pipeline
 * @brief Demux / decode / filter / encode / mux types.
 *
 * @ingroup multimedia_pipeline
 */
namespace StormByte::Multimedia::Pipeline {
	/**
	 * @class Packet
	 * @brief One compressed access unit.
	 *
	 * Compressed bytes for one picture, audio block or subtitle event.
	 * Packet filters, @ref Remuxer and @ref Muxer can inspect the
	 * origin track, timestamps, payload, side data and the Registry
	 * @ref StormByte::Multimedia::Codec of that AU. The Packet does
	 * not open a codec.
	 *
	 * Copies and @ref Clone share media buffers rather than duplicating
	 * compressed bytes.
	 *
	 * @see Item
	 * @see Frame
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC Packet: public Item {
		friend class Backend::Pipeline::Packet;
		friend class Decoder;
		friend class Demuxer;
		friend class Encoder;
		friend class Filter::FFmpeg;
		friend class Muxer;

		public:
			using PointerType = StormByte::Safe::Shared<Packet>;

			/**
			 * @name Construction
			 * @{
			 */

			/**
			 * @brief Empty packet (no payload, track -1, type Unknown).
			 */
			Packet() noexcept;

			/**
			 * @brief Builds a packet.
			 * @param track Origin container stream index.
			 * @param type Media of this access unit.
			 * @param producer Step that created this unit.
			 * @param payload Owned compressed bytes.
			 * @param pts Presentation timestamp, if known.
			 * @param dts Decode timestamp, if known.
			 * @param duration Packet duration, if known.
			 * @param key_frame Whether this is a key frame.
			 * @param attachments Side-data blobs that must reach the muxer.
			 * @param codec Registry codec of this AU, or nullptr.
			 * @param serial Lineage id born at the demuxer for this origin track.
			 * @param part Sub-id inside @p serial. Zero when the demuxed unit did not split.
			 *
			 * Encoder stamps the destination codec. Demuxer stamps
			 * the source stream codec. There is no public setter.
			 */
			Packet(int track, enum Type type, enum Producer producer,
				StormByte::Buffer::FIFO payload,
				std::optional<Property::Duration> pts,
				std::optional<Property::Duration> dts,
				std::optional<Property::Duration> duration,
				bool key_frame,
				std::vector<SideData> attachments,
				const StormByte::Multimedia::Codec* codec,
				std::uint64_t serial,
				std::uint64_t part) noexcept;

			/**
			 * @brief Copy. Metadata and FIFO handle are copied; the
			 *        libav packet is referenced (`av_packet_ref`).
			 * @param other Source packet.
			 *
			 * Not a deep copy of compressed bytes. @ref Clone uses
			 * this constructor.
			 */
			Packet(const Packet& other) noexcept;

			/**
			 * @brief Move constructor.
			 * @param other Packet to take.
			 *
			 * @p other becomes the empty sentinel.
			 */
			Packet(Packet&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			~Packet() noexcept override;

			/**
			 * @brief Copy assignment. Same as the copy constructor.
			 * @param other Source packet.
			 * @return *this.
			 */
			Packet& operator=(const Packet& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Packet to take.
			 * @return *this.
			 *
			 * @p other becomes the empty sentinel.
			 */
			Packet& operator=(Packet&& other) noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Timing
			 * @{
			 */

			/**
			 * @brief Presentation timestamp on the stream clock.
			 * @return Pts, or empty.
			 */
			inline const std::optional<Property::Duration>& Pts() const noexcept {
				return m_pts;
			}

			/**
			 * @brief Decode timestamp on the stream clock.
			 * @return Dts, or empty.
			 */
			inline const std::optional<Property::Duration>& Dts() const noexcept {
				return m_dts;
			}

			/**
			 * @brief Packet duration on the stream clock.
			 * @return Duration, or empty.
			 */
			inline const std::optional<Property::Duration>& Duration() const noexcept {
				return m_duration;
			}

			/**
			 * @brief Whether this is a key frame / key packet.
			 * @return true if marked as a key frame.
			 */
			inline bool KeyFrame() const noexcept {
				return m_keyFrame;
			}

			/**
			 * @}
			 */

			/**
			 * @name Lineage
			 * @{
			 */

			/**
			 * @brief Monotonic lineage id assigned when the unit enters the pipe.
			 * @return Serial born at the demuxer for this origin track; empty on the sentinel.
			 *
			 * This is not a decoded-frame count and not an FFmpeg @c nb_frames /
			 * @c nb_read_frames figure. It identifies one demuxed access unit as it
			 * travels Demuxer → … → Muxer. Downstream stages copy it.
			 * @ref Part distinguishes several frames born from the same serial.
			 */
			inline const std::optional<std::uint64_t>& Serial() const noexcept {
				return m_serial;
			}

			/**
			 * @brief Sub-id inside @ref Serial when one demuxed unit yields several frames.
			 * @return Zero when the unit did not split; 0, 1, … after a split.
			 *
			 * Downstream stages copy this value. It is not a pipe-wide counter.
			 */
			inline std::uint64_t Part() const noexcept {
				return m_part;
			}

			/**
			 * @}
			 */

			/**
			 * @name Codec
			 * @{
			 */

			/**
			 * @brief Registry codec of this access unit.
			 * @return Pointer owned by Registry, or nullptr on the sentinel.
			 */
			inline const StormByte::Multimedia::Codec* Codec() const noexcept {
				return m_codec;
			}

			/**
			 * @}
			 */

			/**
			 * @name Payload
			 * @{
			 */

			/**
			 * @brief Owned compressed payload.
			 * @return FIFO (not thread-safe).
			 */
			inline const StormByte::Buffer::FIFO& Payload() const noexcept {
				return m_payload;
			}

			/**
			 * @brief Owned compressed payload (mutable).
			 * @return FIFO (not thread-safe).
			 */
			inline StormByte::Buffer::FIFO& Payload() noexcept {
				return m_payload;
			}

			/**
			 * @brief Side-data blobs bound to this access unit.
			 * @return Blobs (HdrPlus, captions, …). Empty when none.
			 */
			inline const std::vector<SideData>& Attachments() const noexcept {
				return m_attachments;
			}

			/**
			 * @brief Side-data blobs bound to this access unit (mutable).
			 * @return Blobs.
			 */
			inline std::vector<SideData>& Attachments() noexcept {
				return m_attachments;
			}

			/**
			 * @}
			 */

		private:
			/**
			 * @brief Adopts a backend packet.
			 * @param backend Backend holder.
			 */
			void Bind(std::unique_ptr<Backend::Pipeline::Packet> backend) noexcept;

			/**
			 * @brief Turns this unit into the empty sentinel.
			 */
			void BecomeEmpty() noexcept;

			/**
			 * @brief Fork for hopper taps (`Clonable::Clone`).
			 * @return New unit sharing libav packet buffers (`av_packet_ref`).
			 *
			 * Not a public API. Uses the copy constructor. Not a
			 * deep copy of payload.
			 */
			Item::PointerType Clone() const override;

			/**
			 * @brief Moves this unit into a new owning pointer.
			 * @return Pointer to the relocated unit; *this becomes the empty sentinel.
			 *
			 * Not a public API. Uses the move constructor.
			 * @ref Step::Emit does not call this.
			 */
			Item::PointerType Move() override;

			StormByte::Buffer::FIFO m_payload;								///< Compressed bytes
			std::optional<Property::Duration> m_pts;						///< Presentation timestamp
			std::optional<Property::Duration> m_dts;						///< Decode timestamp
			std::optional<Property::Duration> m_duration;					///< Packet duration
			bool m_keyFrame;												///< Key frame
			std::vector<SideData> m_attachments;							///< Packet side data
			const StormByte::Multimedia::Codec* m_codec;					///< Registry codec, or nullptr
			std::optional<std::uint64_t> m_serial;							///< Lineage id born at demux
			std::uint64_t m_part;											///< Sub-id inside serial
			std::unique_ptr<Backend::Pipeline::Packet> m_backend;			///< Backend holder
	};
}
