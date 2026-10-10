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
 * file. Third-party components remain under their own licenses.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#pragma once

#include <StormByte/buffer/fifo.hxx>

#include <cstddef>
#include <cstdint>
#include <span>

/**
 * @namespace StormByte::Multimedia::Backend::Pipeline::Detail
 * @brief Private pipeline diagnostics shared by packet boundaries.
 */
namespace StormByte::Multimedia::Backend::Pipeline::Detail {
	/**
	 * @brief Computes a stable FNV-1a fingerprint for a packet byte span.
	 * @param bytes Byte sequence to fingerprint.
	 * @return 64-bit digest for matching packet payloads across stages.
	 */
	inline std::uint64_t PacketDigest(std::span<const std::byte> bytes) noexcept {
		std::uint64_t digest = 14695981039346656037ull;
		for (const std::byte byte : bytes) {
			digest ^= std::to_integer<std::uint8_t>(byte);
			digest *= 1099511628211ull;
		}
		return digest;
	}

	/**
	 * @brief Computes a stable FNV-1a fingerprint for the unread FIFO bytes.
	 * @param fifo Packet payload whose unread suffix is fingerprinted.
	 * @return 64-bit digest, or zero if the FIFO cursor is inconsistent.
	 */
	inline std::uint64_t PacketDigest(const StormByte::Buffer::FIFO& fifo) noexcept {
		const auto& stored = fifo.Data();
		const std::size_t available = static_cast<std::size_t>(fifo.Available());
		if (available == 0)
			return PacketDigest(std::span<const std::byte>{});
		if (available > stored.size())
			return 0;
		const auto* first = stored.data() + (stored.size() - available);
		return PacketDigest(std::span<const std::byte>{first, available});
	}
}