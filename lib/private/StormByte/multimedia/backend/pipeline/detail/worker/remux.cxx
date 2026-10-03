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

#include <StormByte/multimedia/backend/pipeline/detail/worker/remux.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/property/duration.hxx>

#include <format>
#include <string>

namespace {
	std::string Ns(const std::optional<StormByte::Multimedia::Property::Duration>& value) noexcept {
		if (!value)
			return "-";
		return std::format("{}", value->Nanoseconds().count());
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	using StormByte::Multimedia::Pipeline::Item;
	using StormByte::Multimedia::Pipeline::Packet;
	using StormByte::Multimedia::Pipeline::Remuxer;
	using StormByte::Logger::Level;

	Remux::Remux(Remuxer& owner) noexcept
	:	StormByte::Multimedia::Backend::Pipeline::Worker(owner.Face()),
		m_owner(owner) {}

	void Remux::Setup() noexcept {
		if (m_owner.m_index < 0) {
			Fail("remuxer origin is negative");
			return;
		}

		Log(Level::Notice, std::format("open t={}", m_owner.m_index));
	}

	void Remux::Process(Item::PointerType item) noexcept {
		if (!item) {
			Flush();
			return;
		}

		NameThread("STMM:Remuxer:" + std::to_string(m_owner.m_index));
		if (m_owner.Failed())
			return;
		auto packet = StormByte::Safe::DynamicPointerCast<Packet>(item);
		if (!packet) {
			Fail("remuxer expected a packet");
			return;
		}

		if (packet->Track() != m_owner.m_index)
			return;
		if (!packet->Serial()) {
			Fail("packet has no serial");
			return;
		}

		Log(Level::LowLevel, std::format("fwd t={} {}:{} pts={} dts={}",
			packet->Track(), *packet->Serial(), packet->Part(),
			Ns(packet->Pts()), Ns(packet->Dts())));
		m_owner.Emit(std::move(packet));
	}

	void Remux::Flush() noexcept {}
}
