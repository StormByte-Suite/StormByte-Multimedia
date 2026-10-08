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

#include <StormByte/multimedia/backend/pipeline/detail/worker/demux.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/multimedia/backend/pipeline/demuxer.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>

#include <format>
#include <utility>
#include <algorithm>

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Worker {
	using StormByte::Multimedia::Pipeline::CheckResult;
	using StormByte::Multimedia::Pipeline::Demuxer;
	using StormByte::Multimedia::Pipeline::Item;
	using StormByte::Multimedia::Pipeline::Packet;
	using StormByte::Logger::Level;

	FeedThread::FeedThread(Demux& owner, int track)
	: m_thread([&owner, track]() { owner.FeedTrack(track); }), m_track(track) {}

	FeedThread::~FeedThread() noexcept {
		Join();
	}

	int FeedThread::Track() const noexcept {
		return m_track;
	}

	void FeedThread::Join() noexcept {
		if (m_thread.joinable())
			m_thread.join();
	}

	Demux::Demux(Demuxer& owner) noexcept
	:	StormByte::Multimedia::Backend::Pipeline::Worker(owner.Face()),
		m_owner(owner) {}

	Demux::~Demux() noexcept {
		StopFeed();
	}

	void Demux::StopFeed() noexcept {
		m_feedStop.store(true, StormByte::Safe::MemoryOrder::Release);
		m_parkCv.notify_all();
		for (auto& feeder : m_feeds)
			feeder->Join();
		m_feeds.clear();
	}

	void Demux::EnsureFeed(int track) noexcept {
		if (std::find_if(m_feeds.begin(), m_feeds.end(), [track](const auto& feeder) {
				return feeder->Track() == track;
			}) != m_feeds.end())
			return;
		m_feeds.push_back(StormByte::Safe::Shared<FeedThread>::MakePointer<FeedThread>(*this, track));
	}

	bool Demux::ParkPending() const noexcept {
		for (const auto& [track, queue] : m_park) {
			if (!queue.empty())
				return true;
		}
		return false;
	}

	void Demux::FeedTrack(int track) noexcept {
		NameThread(std::format("STMM:DmxF{}", track));
		for (;;) {
			Packet::PointerType packet;
			{
				StormByte::Safe::UniqueLock lock(m_parkMutex);
				m_parkCv.wait(lock, [this, track]() {
					return m_feedStop.load(StormByte::Safe::MemoryOrder::Acquire)
						|| !m_park[track].empty();
				});
				if (m_feedStop.load(StormByte::Safe::MemoryOrder::Acquire) && m_park[track].empty())
					return;
				if (m_park[track].empty())
					continue;
				packet = std::move(m_park[track].front());
				m_park[track].pop_front();
			}
			m_parkCv.notify_all();
			if (packet)
				Emit(std::move(packet));
		}
	}

	void Demux::Setup() noexcept {
		NameThread("STMM:Demuxer");
		m_owner.WaitForPlan();
		if (Stopping() || !m_owner.Plan())
			return;

		if (const CheckResult check = m_owner.Plan()->Check(); !check) {
			Fail((*check.error()).what());
			return;
		}

		m_owner.m_backend = StormByte::Safe::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Demuxer>();
		if (!m_owner.m_backend->Open(m_owner))
			return;

		m_owner.m_eof = false;
		m_owner.m_positionNs.store(-1, StormByte::Safe::MemoryOrder::Release);
		m_owner.m_nextSerial.clear();
		m_feedStop.store(false, StormByte::Safe::MemoryOrder::Release);
			Log(Level::Notice, std::format("open {}", std::string_view{m_owner.Origin().Path()}));
	}

	void Demux::Process(Item::PointerType) noexcept {
		if (!m_owner.m_backend || !m_owner.m_backend->IsOpen()) {
			Ended();
			return;
		}

		if (Stopping()) {
			StopFeed();
			return;
		}

		const bool measure = m_owner.Measuring();
		Packet::PointerType packet = m_owner.m_backend->Read(m_owner);
		if (m_owner.Failed()) {
			StopFeed();
			return;
		}
		if (!packet) {
			if (measure) {
				if (!m_owner.Eof())
					m_owner.ReachedEof();
				if (m_owner.Measuring())
					Wait();
				return;
			}
			if (!m_owner.Eof())
				m_owner.ReachedEof();
			if (!m_owner.Eof())
				return;

			// Source EoF. Do not sit on ParkPending forever: the feeder
			// can be blocked in Emit while the decoder already Wait()s on
			// an empty hopper. Drain the park on this thread, then stop.
			m_parkCv.notify_all();
			for (;;) {
				StormByte::Safe::Vector<Packet::PointerType> leftover;
				{
					StormByte::Safe::UniqueLock lock(m_parkMutex);
					if (!ParkPending())
						break;
					for (auto& [track, queue] : m_park) {
						while (!queue.empty()) {
							leftover.push_back(std::move(queue.front()));
							queue.pop_front();
						}
					}
				}
				m_parkCv.notify_all();
				for (auto& item : leftover) {
					if (item)
						Emit(std::move(item));
				}
			}

			StopFeed();
			Ended();
			return;
		}

		if (const auto& pts = packet->Pts(); pts)
			m_owner.m_positionNs.store(pts->Nanoseconds().count(), StormByte::Safe::MemoryOrder::Release);

		const int track = packet->Track();
		{
			StormByte::Safe::UniqueLock lock(m_parkMutex);
			EnsureFeed(track);
			m_parkCv.wait(lock, [this, track]() {
				return m_feedStop.load(StormByte::Safe::MemoryOrder::Acquire)
					|| m_park[track].size() < ParkCeiling;
			});
			if (m_feedStop.load(StormByte::Safe::MemoryOrder::Acquire))
				return;
			m_park[track].push_back(std::move(packet));
		}
		m_parkCv.notify_all();
	}

	void Demux::Flush() noexcept {}
}
