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

#include <tables/codec/catalog.hxx>
#include <StormByte/safe/string.hxx>

#include <cstdint>

namespace StormByte::Multimedia::Tables::Codec {
	const Catalog& Catalog::Instance() noexcept {
		static Catalog instance;
		return instance;
	}

	Catalog::Catalog() noexcept {
		Initialize();
	}

	void Catalog::Index(Type type, std::span<const CodecDef> table) noexcept {
		for (std::size_t index = 0; index < table.size(); ++index) {
			const auto& row = table[index];
			const std::uint64_t reference = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(type)) << 32)
				| static_cast<std::uint32_t>(index);
			m_byName.emplace(StormByte::Safe::String{row.name}, reference);
			m_kind.emplace(StormByte::Safe::String{row.name}, type);
			for (std::size_t i = 0; i < row.FfmpegIdCount(); ++i)
				m_byName.emplace(StormByte::Safe::String{row.FfmpegId(i)}, reference);
		}
	}

	void Catalog::Initialize() noexcept {
		const auto video		= Identity(Type::Video);
		const auto audio		= Identity(Type::Audio);
		const auto subtitle		= Identity(Type::Subtitle);
		const auto attachment	= Identity(Type::Attachment);
		m_byName.reserve((video.size() + audio.size() + subtitle.size() + attachment.size()) * 2);
		m_kind.reserve(video.size() + audio.size() + subtitle.size() + attachment.size());
		Index(Type::Video, video);
		Index(Type::Audio, audio);
		Index(Type::Subtitle, subtitle);
		Index(Type::Attachment, attachment);
	}

	std::span<const CodecDef> Catalog::All(Type type) const noexcept {
		return Identity(type);
	}

	const CodecDef* Catalog::Find(std::string_view name) const noexcept {
		const auto it = m_byName.find(StormByte::Safe::String{name});
		if (it == m_byName.end())
			return nullptr;
		const auto type = static_cast<Type>(static_cast<std::uint32_t>(it->second >> 32));
		const auto index = static_cast<std::size_t>(it->second & 0xffffffffu);
		const auto rows = Identity(type);
		return index < rows.size() ? &rows[index] : nullptr;
	}

	Type Catalog::Kind(std::string_view name) const noexcept {
		const auto it = m_kind.find(StormByte::Safe::String{name});
		return it == m_kind.end() ? Type::Unknown : it->second;
	}
}
