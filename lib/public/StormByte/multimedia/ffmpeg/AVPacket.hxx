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

#include <StormByte/multimedia/ffmpeg/AVPointer.hxx>
#include <StormByte/multimedia/ffmpeg/fwd.hxx>

#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter
 * @brief Forward so this RAII type can friend the filter base.
 */
namespace StormByte::Multimedia::Pipeline::Filter {
	class FFmpeg;
}

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline
 * @brief Multimedia-owned pipeline stages and unit holders.
 */
namespace StormByte::Multimedia::Backend::Pipeline {
	class Packet;
}

/**
 * @namespace StormByte::Multimedia::FFmpeg
 * @brief Private RAII wrappers over libav*.
 */
namespace StormByte::Multimedia::FFmpeg {
	class AVBSF;
	class AVDecoder;
	class AVEncoder;
	class AVFormatContext;

	/**
	 * @class AVPacket
	 * @brief RAII owner of a libav AVPacket.
	 *
	 * Copy and @ref Ref share compressed buffers (`av_packet_ref`).
	 * They are not a deep copy of payload. Each wrapper still owns
	 * its own @c AVPacket struct. @c Get() and @c Detach() are not
	 * public.
	 *
	 * @ingroup multimedia_pipeline
	 */
	class STORMBYTE_MULTIMEDIA_PUBLIC AVPacket: public AVPointer<::AVPacket> {
		friend class AVBSF;
		friend class AVDecoder;
		friend class AVEncoder;
		friend class AVFormatContext;
		friend class StormByte::Multimedia::Backend::Pipeline::Packet;
		friend class StormByte::Multimedia::Pipeline::Filter::FFmpeg;
		public:
			/**
			 * @brief Allocates an empty packet.
			 */
			AVPacket() noexcept;

			/**
			 * @brief Copy. New struct; payload buffers are referenced (`av_packet_ref`).
			 * @param other Source packet.
			 *
			 * Not a deep copy of compressed bytes.
			 */
			AVPacket(const AVPacket& other) noexcept;

			/**
			 * @brief Move constructor. Transfers the libav pointer.
			 * @param other Source packet; left empty.
			 */
			AVPacket(AVPacket&& other) noexcept = default;

			/**
			 * @brief Destructor. Unreferences buffers and frees the struct.
			 */
			~AVPacket() noexcept override;

			/**
			 * @brief Copy assignment. Same as the copy constructor.
			 * @param other Source packet.
			 * @return *this.
			 */
			AVPacket& operator=(const AVPacket& other) noexcept;

			/**
			 * @brief Move assignment. Frees *this, then takes @p other.
			 * @param other Source packet; left empty.
			 * @return *this.
			 */
			AVPacket& operator=(AVPacket&& other) noexcept = default;

			/**
			 * @brief New packet referencing the same data (`av_packet_ref`).
			 * @return Referenced packet.
			 *
			 * Same as the copy constructor.
			 */
			FFmpeg::AVPacket Ref() const noexcept;

			/**
			 * @brief Unreferences packet data (`av_packet_unref`).
			 */
			void Unref() noexcept;

			/**
			 * @brief Replaces payload for avcodec_send_packet.
			 * @param data Compressed bytes.
			 * @param size Byte count.
			 * @param stream_index Stream index.
			 * @param key_frame Sets AV_PKT_FLAG_KEY when true.
			 * @return false if allocation failed.
			 */
			bool Load(const std::uint8_t* data, int size, int stream_index, bool key_frame) noexcept;

			/**
			 * @brief Sets timestamps in stream time base.
			 * @param pts Presentation timestamp, or AV_NOPTS_VALUE.
			 * @param dts Decode timestamp, or AV_NOPTS_VALUE.
			 * @param duration Duration ticks, or 0.
			 */
			void Timestamps(std::int64_t pts, std::int64_t dts, std::int64_t duration) noexcept;

			/**
			 * @brief Packet stream index.
			 * @return stream_index, or -1 if empty.
			 */
			int StreamIndex() const noexcept;

			/**
			 * @brief Presentation timestamp in stream time base.
			 * @return PTS, or AV_NOPTS_VALUE.
			 */
			std::int64_t Pts() const noexcept;

			/**
			 * @brief Decode timestamp in stream time base.
			 * @return DTS, or AV_NOPTS_VALUE.
			 */
			std::int64_t Dts() const noexcept;

			/**
			 * @brief Packet duration in stream time base.
			 * @return Duration ticks, or 0.
			 */
			std::int64_t Duration() const noexcept;

			/**
			 * @brief Packet flags (AV_PKT_FLAG_*).
			 * @return Flags, or 0 if empty.
			 */
			int Flags() const noexcept;

			/**
			 * @brief Compressed payload pointer.
			 * @return data, or nullptr if empty.
			 */
			const std::uint8_t* Data() const noexcept;

			/**
			 * @brief Compressed payload size in bytes.
			 * @return size, or 0 if empty.
			 */
			int Size() const noexcept;

			/**
			 * @brief Byte position of this packet in its input source.
			 * @return Source offset, or -1 when unavailable.
			 */
			std::int64_t Position() const noexcept;

			/**
			 * @brief Whether a packet struct is owned.
			 * @return true if the wrapper holds a packet.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Number of side-data entries.
			 * @return Count, or 0.
			 */
			int SideDataCount() const noexcept;

			/**
			 * @brief Side-data type at @p index.
			 * @param index Entry index.
			 * @return `AVPacketSideDataType` as int, or -1.
			 */
			int SideDataType(int index) const noexcept;

			/**
			 * @brief Side-data payload at @p index.
			 * @param index Entry index.
			 * @param size Set to payload size on success.
			 * @return Pointer, or nullptr.
			 */
			const std::uint8_t* SideData(int index, int& size) const noexcept;

			/**
			 * @brief Adopts @p raw. Previous packet is freed.
			 * @param raw libav packet, or nullptr.
			 */
			void Reset(::AVPacket* raw) noexcept;

		private:
			/**
			 * @brief Yields the raw pointer and leaves this wrapper empty.
			 * @return Previous libav packet, or nullptr. Does not free.
			 */
			::AVPacket* Detach() noexcept;

			/**
			 * @brief Frees the packet (`av_packet_free`).
			 */
			void Free() noexcept override;

			using AVPointer<::AVPacket>::Get;
			using AVPointer<::AVPacket>::Detach;
	};

	extern template class STORMBYTE_MULTIMEDIA_PUBLIC AVPointer<::AVPacket>;
}
