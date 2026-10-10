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

#include <StormByte/buffer/io/buffered_location_reader.hxx>
#include <StormByte/buffer/io/buffered_location_writer.hxx>
#include <StormByte/multimedia/visibility.h>

#include <cstdint>

struct AVIOContext;

/**
 * @namespace StormByte::Multimedia::Backend
 * @brief Private multimedia helpers.
 */
namespace StormByte::Multimedia::Backend {
	/**
	 * @class FileAvio
	 * @brief Libav AVIO over a File-family reader or writer.
	 *
	 * Does not own the leaf. Reader mode is opaque for
	 * @c avformat_open_input. Writer mode is opaque for
	 * @c avformat_write_header / @c av_write_trailer.
	 * Seek / Size / Tell go to the leaf so a future remote
	 * File-family type can override them. Writer AVIO is
	 * marked @c AVIO_SEEKABLE_NORMAL so Matroska/MP4 can
	 * patch duration and indexes.
	 */
	class STORMBYTE_MULTIMEDIA_PRIVATE FileAvio {
		public:
			/**
			 * @brief Bind @p reader. Does not allocate AVIO yet.
			 * @param reader Live File-family reader.
			 */
			explicit FileAvio(StormByte::Buffer::IO::BufferedLocationReader& reader) noexcept;

			/**
			 * @brief Bind @p writer. Does not allocate AVIO yet.
			 * @param writer Live File-family writer.
			 */
			explicit FileAvio(StormByte::Buffer::IO::BufferedLocationWriter& writer) noexcept;

			FileAvio(const FileAvio&) = delete;
			FileAvio& operator=(const FileAvio&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Context to take.
			 */
			FileAvio(FileAvio&& other) noexcept;

			/**
			 * @brief Frees the AVIO buffer and context.
			 */
			~FileAvio() noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Context to take.
			 * @return *this.
			 */
			FileAvio& operator=(FileAvio&& other) noexcept;

			/**
			 * @brief Allocate the AVIO context.
			 * @return false if allocation failed.
			 *
			 * Reader: read + seek. Writer: write + seek,
			 * @c write_flag = 1 and @c AVIO_SEEKABLE_NORMAL.
			 * Idempotent.
			 */
			bool Arm() noexcept;

			/**
			 * @brief Libav I/O context, or null before @ref Arm.
			 * @return Context.
			 */
			AVIOContext* Context() const noexcept;

		private:
			StormByte::Buffer::IO::BufferedLocationReader* m_reader;	///< Borrowed reader, or null
			StormByte::Buffer::IO::BufferedLocationWriter* m_writer;	///< Borrowed writer, or null
			AVIOContext* m_avio;	///< Custom I/O

			/**
			 * @brief Libav read callback.
			 * @param opaque This object.
			 * @param buf Destination.
			 * @param bufSize Capacity.
			 * @return Bytes copied, or a negative AVERROR.
			 */
			static int Read(void* opaque, std::uint8_t* buf, int bufSize) noexcept;

			/**
			 * @brief Libav write callback.
			 * @param opaque This object.
			 * @param buf Source.
			 * @param bufSize Byte count.
			 * @return Bytes written, or a negative AVERROR.
			 */
			static int Write(void* opaque, const std::uint8_t* buf, int bufSize) noexcept;

			/**
			 * @brief Libav seek callback.
			 * @param opaque This object.
			 * @param offset Byte offset.
			 * @param whence SEEK_SET / SEEK_CUR / SEEK_END / AVSEEK_SIZE.
			 * @return New position, size for AVSEEK_SIZE, or a negative AVERROR.
			 */
			static std::int64_t Seek(void* opaque, std::int64_t offset, int whence) noexcept;

			/**
			 * @brief Flush writer AVIO if needed and free the context.
			 */
			void Free() noexcept;

			/**
			 * @brief Whether this instance is bound to a writer.
			 * @return true in writer mode.
			 */
			bool IsWriter() const noexcept;

			/**
			 * @brief Origin length for AVSEEK_SIZE / SEEK_END.
			 * @return Length, or empty if the reader has no size.
			 */
			StormByte::Safe::Optional<StormByte::ByteSize> LeafSize() const noexcept;
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Multimedia::Backend::FileAvio);
