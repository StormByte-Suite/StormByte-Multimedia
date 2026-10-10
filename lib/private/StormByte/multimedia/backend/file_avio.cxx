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

#include <StormByte/buffer/io/typedefs.hxx>
#include <StormByte/multimedia/backend/file_avio.hxx>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" {
	#include <libavformat/avio.h>
	#include <libavutil/error.h>
	#include <libavutil/mem.h>
}

using StormByte::Buffer::IO::BufferedLocationReader;
using StormByte::Buffer::IO::BufferedLocationWriter;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::Position;

namespace StormByte::Multimedia::Backend {
	FileAvio::FileAvio(BufferedLocationReader& reader) noexcept
	: m_reader(&reader), m_writer(nullptr), m_avio(nullptr) {}

	FileAvio::FileAvio(BufferedLocationWriter& writer) noexcept
	: m_reader(nullptr), m_writer(&writer), m_avio(nullptr) {}

	FileAvio::FileAvio(FileAvio&& other) noexcept
	: m_reader(other.m_reader), m_writer(other.m_writer), m_avio(other.m_avio) {
		other.m_reader = nullptr;
		other.m_writer = nullptr;
		other.m_avio = nullptr;
	}

	FileAvio::~FileAvio() noexcept {
		Free();
	}

	FileAvio& FileAvio::operator=(FileAvio&& other) noexcept {
		if (this != &other) {
			Free();
			m_reader = other.m_reader;
			m_writer = other.m_writer;
			m_avio = other.m_avio;
			other.m_reader = nullptr;
			other.m_writer = nullptr;
			other.m_avio = nullptr;
		}
		return *this;
	}

	bool FileAvio::IsWriter() const noexcept {
		return m_writer != nullptr;
	}

	StormByte::Safe::Optional<StormByte::ByteSize> FileAvio::LeafSize() const noexcept {
		if (m_reader)
			return m_reader->Size();
		if (m_writer)
			return m_writer->Size();
		return std::nullopt;
	}

	bool FileAvio::Arm() noexcept {
		if (m_avio)
			return true;

		constexpr int ioSize = 64 * 1024;
		auto* ioBuf = static_cast<unsigned char*>(av_malloc(ioSize));
		if (!ioBuf)
			return false;

		if (IsWriter()) {
			m_avio = avio_alloc_context(ioBuf, ioSize, 1, this, nullptr, &Write, &Seek);
			if (!m_avio) {
				av_free(ioBuf);
				return false;
			}
			m_avio->seekable = AVIO_SEEKABLE_NORMAL;
			return true;
		}

		m_avio = avio_alloc_context(ioBuf, ioSize, 0, this, &Read, nullptr, &Seek);
		if (!m_avio) {
			av_free(ioBuf);
			return false;
		}
		m_avio->seekable = AVIO_SEEKABLE_NORMAL;
		return true;
	}

	AVIOContext* FileAvio::Context() const noexcept {
		return m_avio;
	}

	void FileAvio::Free() noexcept {
		if (!m_avio)
			return;
		if (IsWriter() && m_avio->buffer)
			avio_flush(m_avio);
		av_free(m_avio->buffer);
		avio_context_free(&m_avio);
	}

	int FileAvio::Read(void* opaque, std::uint8_t* buf, int bufSize) noexcept {
		auto* self = static_cast<FileAvio*>(opaque);
		if (!self || bufSize <= 0)
			return AVERROR(EINVAL);
		if (!self->m_reader)
			return AVERROR(EINVAL);

		const auto got = self->m_reader->Read(std::span<std::byte>(
			reinterpret_cast<std::byte*>(buf), static_cast<std::size_t>(bufSize)));
		if (got.status == Status::Error || got.status == Status::Failed)
			return AVERROR(EIO);
		if (got.count == 0)
			return AVERROR_EOF;
		return static_cast<int>(got.count);
	}

	int FileAvio::Write(void* opaque, const std::uint8_t* buf, int bufSize) noexcept {
		auto* self = static_cast<FileAvio*>(opaque);
		if (!self || !buf || bufSize <= 0)
			return AVERROR(EINVAL);
		if (!self->m_writer)
			return AVERROR(EINVAL);

		const auto put = self->m_writer->Write(std::span<const std::byte>(
			reinterpret_cast<const std::byte*>(buf), static_cast<std::size_t>(bufSize)));
		if (put.status == Status::Error || put.status == Status::Failed)
			return AVERROR(EIO);
		if (put.count == 0)
			return AVERROR(EIO);
		return static_cast<int>(put.count);
	}

	std::int64_t FileAvio::Seek(void* opaque, std::int64_t offset, int whence) noexcept {
		auto* self = static_cast<FileAvio*>(opaque);
		if (!self)
			return AVERROR(EINVAL);

		if (whence == AVSEEK_SIZE) {
			const auto size = self->LeafSize();
			if (!size.has_value())
				return AVERROR(ESPIPE);
			return static_cast<std::int64_t>(*size);
		}

		int mode = whence & ~AVSEEK_FORCE;
		std::ptrdiff_t target = static_cast<std::ptrdiff_t>(offset);
		Position pos = Position::Absolute;
		if (mode == SEEK_SET) {
			pos = Position::Absolute;
		} else if (mode == SEEK_CUR) {
			pos = Position::Relative;
		} else if (mode == SEEK_END) {
			const auto size = self->LeafSize();
			if (!size.has_value())
				return AVERROR(ESPIPE);
			target = static_cast<std::ptrdiff_t>(*size + offset);
			pos = Position::Absolute;
		} else {
			return AVERROR(EINVAL);
		}

		if (self->m_reader) {
			const auto seek = self->m_reader->Seek(target, pos);
			if (seek.status != Status::Ok)
				return AVERROR(EIO);
			return static_cast<std::int64_t>(self->m_reader->Tell());
		}
		if (self->m_writer) {
			const auto seek = self->m_writer->Seek(target, pos);
			if (seek.status != Status::Ok)
				return AVERROR(EIO);
			return static_cast<std::int64_t>(self->m_writer->Tell());
		}
		return AVERROR(EINVAL);
	}
}
