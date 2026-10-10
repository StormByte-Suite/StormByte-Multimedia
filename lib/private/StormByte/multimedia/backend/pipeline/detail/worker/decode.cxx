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

#include <StormByte/multimedia/backend/pipeline/detail/worker/decode.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/packet_diagnostic.hxx>
#include <StormByte/multimedia/backend/pipeline/decoder.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <chrono>
#include <format>
#include <string>
#include <string_view>

namespace {
	std::string Ns(const StormByte::Safe::Optional<StormByte::Multimedia::Property::Duration>& value) noexcept {
		if (!value)
			return "-";
		return std::format("{}", value->Nanoseconds().count());
	}

	std::int64_t ElapsedUs(std::chrono::steady_clock::time_point started) noexcept {
		return std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - started).count();
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	using StormByte::Multimedia::Pipeline::Decoder;
	using StormByte::Multimedia::Pipeline::Frame;
	using StormByte::Multimedia::Pipeline::Item;
	using StormByte::Multimedia::Pipeline::Packet;
	using StormByte::Logger::Level;

	Decode::Decode(Decoder& owner) noexcept
	:	StormByte::Multimedia::Backend::Pipeline::Worker(owner.Face()),
		m_owner(owner) {}

	void Decode::Setup() noexcept {
		NameThread("STMM:Decode:" + std::to_string(m_owner.m_index));
		if (m_owner.m_look) {
			Log(Level::Notice, std::format("look t={}", m_owner.m_index));
			return;
		}

		while (!Stopping() && m_owner.m_origin == nullptr)
			Wait();
		if (Stopping())
			return;
		if (!m_owner.m_origin) {
			Fail("decoder has no demuxer");
			return;
		}

		while (!Stopping() && !m_owner.m_origin->Failed()
			&& !m_owner.m_origin->Ready() && !m_owner.m_origin->Eof())
			Wait();
		if (Stopping())
			return;
		if (m_owner.m_origin->Failed() || (!m_owner.m_origin->Ready() && !m_owner.m_origin->Eof())) {
			const auto reason = m_owner.m_origin->Error().value_or(StormByte::Safe::String("demuxer failed"));
			Fail(static_cast<std::string>(reason));
			return;
		}

		auto backend = m_owner.OpenOrigin();
		if (!backend)
			return;
		m_owner.Bind(std::move(backend));
		m_owner.m_serial.reset();
		m_owner.m_part = 0;
		m_owner.m_inDts.reset();
		Log(Level::Notice, std::format("open t={} impl={}",
			m_owner.m_index, std::string_view{m_owner.m_implementation.value_or(StormByte::Safe::String{"auto"})}));
	}

	void Decode::Process(Item::PointerType item) noexcept {
		if (!item) {
			Flush();
			return;
		}

		NameThread("STMM:Decode:" + std::to_string(m_owner.m_index));
		if (m_owner.Failed())
			return;
		auto packet = StormByte::Safe::DynamicPointerCast<Packet>(item);
		if (!packet) {
			Fail("decoder expected a packet");
			return;
		}

		if (m_owner.m_look && !m_owner.m_backend) {
			if (!m_owner.OpenLook(*packet))
				return;
		}

		if (!m_owner.m_backend) {
			Fail("decoder is not open");
			return;
		}

		if (packet->Track() != m_owner.m_index)
			return;
		if (!packet->Serial()) {
			Fail("packet has no serial");
			return;
		}

		if (m_owner.m_serial != packet->Serial()) {
			m_owner.m_serial = *packet->Serial();
			m_owner.m_part = 0;
			if (packet->Dts())
				m_owner.m_inDts = *packet->Dts();
			else
				m_owner.m_inDts.reset();
		}

		const auto payloadLength = static_cast<StormByte::Safe::String>(packet->Payload().Available());
		Log(Level::LowLevel, std::format("in t={} {}:{} pts={} dts={} bytes={} raw={} fnv1a={:016x}",
			packet->Track(), *packet->Serial(), packet->Part(),
			Ns(packet->Pts()), Ns(packet->Dts()), std::string_view{payloadLength},
			static_cast<std::size_t>(packet->Payload().Available()),
			PacketDigest(packet->Payload())));

		const auto started = std::chrono::steady_clock::now();
		unsigned emitted = 0;
		auto emit = [this, &started, &emitted](Frame::PointerType frame) {
			m_owner.StampLineage(*frame);
			m_owner.StampLook(*frame);
			Log(Level::LowLevel, std::format("out t={} {}:{} pts={} dts={} dur={}",
				frame->Track(), frame->Serial().value_or(0), frame->Part(),
				Ns(frame->Pts()), Ns(frame->Dts()), Ns(frame->Duration())));
			Emit(std::move(frame));
			++emitted;
			m_owner.RecordWork(ElapsedUs(started));
		};

		while (!m_owner.m_backend->Send(m_owner, packet)) {
			if (m_owner.Failed())
				return;
			Frame::PointerType frame = m_owner.m_backend->Receive(m_owner);
			if (m_owner.Failed())
				return;
			if (!frame) {
				Wait();
				continue;
			}

			emit(std::move(frame));
		}

		for (;;) {
			if (m_owner.Failed())
				return;
			Frame::PointerType frame = m_owner.m_backend->Receive(m_owner);
			if (!frame)
				break;
			emit(std::move(frame));
		}

		(void)emitted;
	}

	void Decode::Flush() noexcept {
		if (m_owner.Failed() || !m_owner.m_backend)
			return;
		const auto started = std::chrono::steady_clock::now();
		Log(Level::LowLevel, std::format("decoder EOF submit begin t={} look={} owner={}",
			m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner)));
		m_owner.m_backend->Flush(m_owner);
		Log(Level::LowLevel, std::format("decoder EOF submit end t={} look={} owner={}",
			m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner)));
		std::size_t received = 0;
		for (;;) {
			if (m_owner.Failed())
				return;
			Log(Level::LowLevel, std::format("decoder drain receive begin t={} look={} owner={} iteration={}",
				m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner), received));
			Frame::PointerType frame = m_owner.m_backend->Receive(m_owner);
			Log(Level::LowLevel, std::format("decoder drain receive end t={} look={} owner={} iteration={} frame={} failed={}",
				m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner), received,
				static_cast<bool>(frame), m_owner.Failed()));
			if (!frame)
				break;
			m_owner.StampLineage(*frame);
			m_owner.StampLook(*frame);
			Log(Level::LowLevel, std::format("out t={} {}:{} pts={} dts={} dur={}",
				frame->Track(), frame->Serial().value_or(0), frame->Part(),
				Ns(frame->Pts()), Ns(frame->Dts()), Ns(frame->Duration())));
			Log(Level::LowLevel, std::format("decoder drain emit begin t={} look={} owner={} iteration={}",
				m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner), received));
			Emit(std::move(frame));
			Log(Level::LowLevel, std::format("decoder drain emit end; record work begin t={} look={} owner={} iteration={}",
				m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner), received));
			m_owner.RecordWork(ElapsedUs(started));
			Log(Level::LowLevel, std::format("decoder drain record work end t={} look={} owner={} iteration={}",
				m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner), received));
			++received;
		}
		Log(Level::LowLevel, std::format("decoder drain complete; backend release begin t={} look={} owner={}",
			m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner)));
		m_owner.m_backend.reset();
		Log(Level::LowLevel, std::format("decoder backend release end t={} look={} owner={}",
			m_owner.m_index, m_owner.m_look, static_cast<const void*>(&m_owner)));
	}
}
