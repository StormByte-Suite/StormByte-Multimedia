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

#include <StormByte/multimedia/backend/pipeline/detail/worker/encode.hxx>
#include <StormByte/multimedia/backend/pipeline/encoder.hxx>
#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <format>
#include <string>
#include <thread>

namespace {
	std::string Ns(const StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration>& value) noexcept {
		if (!value)
			return "-";
		return std::format("{}", value->Nanoseconds().count());
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	using StormByte::Multimedia::Pipeline::Encoder;
	using StormByte::Multimedia::Pipeline::Frame;
	using StormByte::Multimedia::Pipeline::Item;
	using StormByte::Multimedia::Pipeline::Packet;
	using StormByte::Logger::Level;

	Encode::Encode(Encoder& owner) noexcept
	:	StormByte::Multimedia::Backend::Pipeline::Worker(owner.Face()),
		m_owner(owner) {}

	void Encode::Setup() noexcept {
		if (!m_owner.m_backend)
			Fail("encoder has no backend");
	}

	void Encode::Process(Item::PointerType item) noexcept {
		if (!item) {
			Flush();
			return;
		}

		NameThread("STMM:Encode:" + std::to_string(m_owner.m_index));
		if (m_owner.Failed() || !m_owner.m_backend)
			return;
		auto frame = StormByte::Safe::DynamicPointerCast<Frame>(item);
		if (!frame) {
			Fail("encoder expected a frame");
			return;
		}

		if (!frame->Serial()) {
			Fail("frame has no serial");
			return;
		}

		const bool opening = !m_owner.m_backend->IsOpen();
		if (opening && !m_owner.m_backend->Open(m_owner, *frame))
			return;
		if (opening)
			Log(Level::Notice, std::format("open t={} codec={} impl={}",
				m_owner.m_index, std::string(m_owner.m_codec->Name()),
				std::string_view{m_owner.m_implementation.value_or(StormByte::Safe::String{"auto"})}));

		Log(Level::LowLevel, std::format("in t={} {}:{} pts={} dts={}",
			m_owner.m_index, *frame->Serial(), frame->Part(),
			Ns(frame->Pts()), Ns(frame->Dts())));
		m_owner.m_serial = *frame->Serial();
		m_owner.m_part = frame->Part();

		while (!m_owner.m_backend->Push(m_owner, frame)) {
			if (m_owner.Failed())
				return;
			Packet::PointerType packet = m_owner.m_backend->Take();
			if (!packet) {
				std::this_thread::yield();
				continue;
			}

			m_owner.Emit(std::move(packet));
		}

		for (;;) {
			if (m_owner.Failed())
				return;
			Packet::PointerType packet = m_owner.m_backend->Take();
			if (!packet)
				break;
			m_owner.Emit(std::move(packet));
		}
	}

	void Encode::Flush() noexcept {
		if (!m_owner.Failed() && m_owner.m_backend) {
			m_owner.m_backend->Flush(m_owner);
			for (;;) {
				if (m_owner.Failed())
					break;
				Packet::PointerType packet = m_owner.m_backend->Take();
				if (!packet)
					break;
				m_owner.Emit(std::move(packet));
			}
		}
	}
}
