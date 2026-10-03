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
	: m_leaf(&reader), m_avio(nullptr) {}

	FileAvio::FileAvio(BufferedLocationWriter& writer) noexcept
	: m_leaf(&writer), m_avio(nullptr) {}

	FileAvio::FileAvio(FileAvio&& other) noexcept
	: m_leaf(other.m_leaf), m_avio(other.m_avio) {
		other.m_avio = nullptr;
	}

	FileAvio::~FileAvio() noexcept {
		Free();
	}

	FileAvio& FileAvio::operator=(FileAvio&& other) noexcept {
		if (this != &other) {
			Free();
			m_leaf = other.m_leaf;
			m_avio = other.m_avio;
			other.m_avio = nullptr;
		}
		return *this;
	}

	bool FileAvio::IsWriter() const noexcept {
		return std::holds_alternative<BufferedLocationWriter*>(m_leaf);
	}

	std::optional<std::size_t> FileAvio::LeafSize() const noexcept {
		if (auto* reader = std::get_if<BufferedLocationReader*>(&m_leaf); reader && *reader)
			return (*reader)->Size();
		if (auto* writer = std::get_if<BufferedLocationWriter*>(&m_leaf); writer && *writer)
			return (*writer)->Size();
		return std::nullopt;
	}

	bool FileAvio::Arm() noexcept {
		if (m_avio)
			return true;

		constexpr int ioSize = 4096;
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
		auto* reader = std::get_if<BufferedLocationReader*>(&self->m_leaf);
		if (!reader || !*reader)
			return AVERROR(EINVAL);

		const auto got = (*reader)->Read(std::span<std::byte>(
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
		auto* writer = std::get_if<BufferedLocationWriter*>(&self->m_leaf);
		if (!writer || !*writer)
			return AVERROR(EINVAL);

		const auto put = (*writer)->Write(std::span<const std::byte>(
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

		if (auto* reader = std::get_if<BufferedLocationReader*>(&self->m_leaf); reader && *reader) {
			const auto seek = (*reader)->Seek(target, pos);
			if (seek.status != Status::Ok)
				return AVERROR(EIO);
			return static_cast<std::int64_t>((*reader)->Tell());
		}
		if (auto* writer = std::get_if<BufferedLocationWriter*>(&self->m_leaf); writer && *writer) {
			const auto seek = (*writer)->Seek(target, pos);
			if (seek.status != Status::Ok)
				return AVERROR(EIO);
			return static_cast<std::int64_t>((*writer)->Tell());
		}
		return AVERROR(EINVAL);
	}
}
