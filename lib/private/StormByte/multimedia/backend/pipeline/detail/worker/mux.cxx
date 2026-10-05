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

#include <StormByte/multimedia/backend/pipeline/detail/worker/mux.hxx>
#include <StormByte/multimedia/backend/pipeline/muxer.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/type.hxx>

#include <format>
#include <string>

namespace {
	std::string Ns(const StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration>& value) noexcept {
		if (!value)
			return "-";
		return std::format("{}", value->Nanoseconds().count());
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	using StormByte::Multimedia::Pipeline::Item;
	using StormByte::Multimedia::Pipeline::Muxer;
	using StormByte::Multimedia::Pipeline::Packet;
	using StormByte::Multimedia::Pipeline::State;
	using StormByte::Multimedia::Type;
	using StormByte::Logger::Level;

	Mux::Mux(Muxer& owner) noexcept
	:	StormByte::Multimedia::Backend::Pipeline::Worker(owner.Face()),
		m_owner(owner) {}

	void Mux::Setup() noexcept {
		if (!m_owner.m_backend)
			Fail("muxer has no backend");
	}

	void Mux::Process(Item::PointerType item) noexcept {
		if (!item) {
			Flush();
			return;
		}

		NameThread("STMM:Muxer");

		if (m_owner.Failed() || !m_owner.m_backend)
			return;

		if (!m_owner.Armed())
			m_owner.WaitArmed();

		if (m_owner.Failed() || m_owner.Status() == State::Stopping || !m_owner.Armed())
			return;

		auto packet = StormByte::Safe::DynamicPointerCast<Packet>(item);
		if (!packet) {
			Fail("muxer expected a packet");
			return;
		}

		while (!m_owner.m_backend->Push(m_owner, packet)) {
			if (m_owner.Failed())
				return;
			Wait();
		}

		const int track = packet->Track();
		const auto type = packet->Type();
		if (type == Type::Video || type == Type::Audio) {
			if (const auto& pts = packet->Pts(); pts) {
				auto ns = pts->Nanoseconds().count();
				if (const auto& dur = packet->Duration(); dur)
					ns += dur->Nanoseconds().count();
				if (type == Type::Video)
					m_owner.m_positionNs.store(ns, std::memory_order_release);
				else {
					const std::int64_t current = m_owner.m_positionNs.load(std::memory_order_acquire);
					if (current < 0)
						m_owner.m_positionNs.store(ns, std::memory_order_release);
				}
			}
		}

		const std::int64_t pos = m_owner.m_positionNs.load(std::memory_order_acquire);
		if (pos >= 0)
			m_owner.ClockPass(pos);

		Log(Level::LowLevel, std::format("written t={} {}:{} pts={} dts={} pos={}",
			track, packet->Serial().value_or(0), packet->Part(),
			Ns(packet->Pts()), Ns(packet->Dts()),
			pos));
	}

	void Mux::Flush() noexcept {
		if (m_owner.m_backend && !m_owner.Failed())
			m_owner.m_backend->Flush(m_owner);
		m_owner.FlushOctets();
		m_owner.m_closed.store(true, std::memory_order_release);
		m_owner.ClockMuxDone();
		Log(Level::Notice, "closed");
	}
}
