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
 * file. Third-party components remain under their own licenses and are not
 * covered by the commercial grant.
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

#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/system/device.hxx>
#include <StormByte/system/host.hxx>

#include <algorithm>
#include <cstdint>
#include <utility>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Multimedia
	 * @brief Multimedia module of the StormByte suite.
	 */
	namespace Multimedia {
		/**
		 * @namespace StormByte::Multimedia::Backend
		 * @brief Multimedia-owned pipeline stages and unit holders.
		 */
		namespace Backend {
			/**
			 * @brief Builds a local reader with a RAM-aware cache ceiling.
			 * @param path Local file path, already converted to a Safe string.
		 * @return Reader owner. Cache allocation remains lazy and is capped at
			 *         the smaller of 64 MiB or one sixty-fourth of available RAM,
			 *         with a 1 MiB fallback/floor.
			 */
			inline StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader>
			MakeLocalFileReader(StormByte::Safe::String path) {
				constexpr std::uint64_t fallbackBytes = 1024ull * 1024ull;
				constexpr std::uint64_t maximumBytes = 16ull * 1024ull * 1024ull;
				constexpr std::uint64_t maximumReadAheadBytes = 8ull * 1024ull * 1024ull;
				const std::uint64_t availableBytes = static_cast<std::uint64_t>(
					StormByte::System::Host::AvailableMemory());
				const std::uint64_t budget = availableBytes == 0
					? fallbackBytes
					: std::clamp(availableBytes / 64, fallbackBytes, maximumBytes);
				const std::uint64_t readAhead = std::min(budget, maximumReadAheadBytes);
				using Reader = StormByte::Buffer::IO::BufferedFileReader;
				using MaxMemory = StormByte::Buffer::IO::MaxMemory;
				using ReadAhead = StormByte::Buffer::IO::ReadAhead;
				return StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationReader>
					::MakePointer<Reader>(std::move(path), Reader::Parameters{
						ReadAhead{StormByte::ByteSize{readAhead}},
						MaxMemory{StormByte::ByteSize{budget}}});
			}

			/**
			 * @brief Builds a local writer preserving System's transfer window with a RAM-aware dirty-page budget.
			 * @param path Local file path, already converted to a Safe string.
			 * @return Writer owner. Dirty pages are capped at the smaller of 16 MiB
			 *         or one sixty-fourth of available RAM, with a 1 MiB fallback/floor.
			 */
			inline StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter>
			MakeLocalFileWriter(StormByte::Safe::String path) {
				constexpr std::uint64_t fallbackBytes = 1024ull * 1024ull;
				constexpr std::uint64_t maximumBytes = 16ull * 1024ull * 1024ull;
				const std::uint64_t availableBytes = static_cast<std::uint64_t>(
					StormByte::System::Host::AvailableMemory());
				const std::uint64_t budget = availableBytes == 0
					? fallbackBytes
					: std::clamp(availableBytes / 64, fallbackBytes, maximumBytes);
				const StormByte::System::Device device{path};
				const StormByte::ByteSize writeChunk = device
					? device.Window().write : StormByte::ByteSize{0};
				using Writer = StormByte::Buffer::IO::BufferedFileWriter;
				using BackPressure = StormByte::Buffer::IO::BackPressure;
				using MaxMemory = StormByte::Buffer::IO::MaxMemory;
				using WriteChunk = StormByte::Buffer::IO::WriteChunk;
				return StormByte::Safe::Unique<StormByte::Buffer::IO::BufferedLocationWriter>
					::MakePointer<Writer>(std::move(path), Writer::Parameters{
						WriteChunk{writeChunk}, BackPressure{4},
						MaxMemory{StormByte::ByteSize{budget}}});
			}
		}
	}
}
