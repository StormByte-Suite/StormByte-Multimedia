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

#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <thread>
#include <utility>

using StormByte::Multimedia::Backend::Pipeline::Pipe;
using StormByte::Multimedia::Pipeline::Packet;

namespace {
	std::optional<std::int64_t> FrontTs(const Pipe::Item::PointerType& item) noexcept {
		auto packet = StormByte::Safe::DynamicPointerCast<Packet>(item);
		if (!packet)
			return std::nullopt;
		if (const auto& dts = packet->Dts(); dts)
			return dts->Nanoseconds().count();
		if (const auto& pts = packet->Pts(); pts)
			return pts->Nanoseconds().count();
		return std::nullopt;
	}
}

Pipe::Pipe() = default;

Pipe::~Pipe() noexcept {
	m_in.Unnotify();
	m_out.Unnotify();
	for (auto& fork : m_forks)
		fork.second->Unnotify();
}

bool Pipe::BindInput(int track, ItemSink& source) {
	if (m_inTracks.contains(track)) {
		m_in.To(track) >> source;
		return true;
	}
	// Keep the initial writer private until the budget is attached. Its
	// destruction releases only that hold, leaving the published producer.
	ItemSink staged;
	staged.To(track) >> m_in;
	Capacity(track, m_initialCeiling);
	if (!m_configuredTracks.contains(track)) {
		staged.Abort();
		source.Abort();
		return false;
	}
	m_inTracks.insert(track);
	staged.To(track) >> source;
	return true;
}

Pipe::ItemSink& Pipe::In() noexcept {
	return m_in;
}

const Pipe::ItemSink& Pipe::In() const noexcept {
	return m_in;
}

Pipe::ItemSink& Pipe::Out() noexcept {
	return m_out;
}

const Pipe::ItemSink& Pipe::Out() const noexcept {
	return m_out;
}

void Pipe::Capacity(int track, StormByte::Size n) noexcept {
	StormByte::Safe::Optional<StormByte::Size> elements;
	if (n > 0)
		elements = n;
	const StormByte::Buffer::QueueLimits limits{elements, m_buffering.QueueBytes()};
	StormByte::Buffer::QueueStatus status;
	if (m_configuredTracks.contains(track)) {
		status = m_in.Limits(track, limits);
	}
	else {
		status = m_in.Configure(track, limits, m_buffering.Budget());
		if (status == StormByte::Buffer::QueueStatus::Ok) {
			try {
				m_configuredTracks.insert(track);
			} catch (const StormByte::Safe::AllocationError&) {
				status = StormByte::Buffer::QueueStatus::Error;
			} catch (const std::bad_alloc&) {
				status = StormByte::Buffer::QueueStatus::Error;
			}
		}
	}
	if (status != StormByte::Buffer::QueueStatus::Ok) {
		if (m_failureHandler)
			m_failureHandler(m_failureOwner, status);
		else
			m_in.Abort();
	}
}

StormByte::Buffer::QueueSnapshot Pipe::Snapshot(int track) const noexcept {
	return m_in.Snapshot(track);
}

void Pipe::Buffering(const StormByte::Multimedia::Pipeline::BufferingPolicy& policy,
	StormByte::Size ceiling) noexcept {
	m_buffering = policy;
	m_initialCeiling = ceiling;
	StormByte::Safe::Optional<StormByte::Size> elements;
	if (ceiling > StormByte::Size{0})
		elements = ceiling;
	const StormByte::Buffer::QueueLimits limits{elements, policy.QueueBytes()};
	for (const int track : m_in.Keys()) {
		const auto status = m_in.Configure(track, limits, policy.Budget());
		if (status != StormByte::Buffer::QueueStatus::Ok) {
			if (m_failureHandler)
				m_failureHandler(m_failureOwner, status);
			else
				m_in.Abort();
			return;
		}
	}
}

void Pipe::FailureHandler(void* owner,
	void (*handler)(void*, StormByte::Buffer::QueueStatus) noexcept) noexcept {
	m_failureOwner = owner;
	m_failureHandler = handler;
}

bool Pipe::PushWeighted(ItemSink& sink, int track, Item::PointerType& item) noexcept {
	if (!item)
		return true;
	StormByte::ByteSize weight{1};
	if (const auto frame = StormByte::Safe::DynamicPointerCast<StormByte::Multimedia::Pipeline::Frame>(item))
		weight = frame->BufferingWeight();
	else if (const auto packet = StormByte::Safe::DynamicPointerCast<StormByte::Multimedia::Pipeline::Packet>(item))
		weight = packet->BufferingWeight();

	for (;;) {
		const auto status = sink.PushFor(track, item, weight, std::chrono::milliseconds{100});
		if (status == StormByte::Buffer::QueueStatus::Ok)
			return true;
		if (status == StormByte::Buffer::QueueStatus::Timeout)
			continue;
		if (status == StormByte::Buffer::QueueStatus::Missing) {
			if (sink.Draining())
				return true;
			if (!sink.EoF()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{1});
				continue;
			}
		}
		if (m_failureHandler)
			m_failureHandler(m_failureOwner, status);
		else
			sink.Abort();
		return false;
	}
}

void Pipe::Listen() noexcept {
	m_in.Notify(m_wake, m_wakeGeneration);
}

void Pipe::Wake() noexcept {
	m_wakeGeneration.fetch_add(std::size_t{1}, StormByte::Safe::MemoryOrder::Release);
	m_wakeGeneration.notify_all();
	m_wake.notify_all();
}

void Pipe::WaitWake(void* owner, bool (*ready)(void*) noexcept) noexcept {
	for (;;) {
		const auto generation = m_wakeGeneration.load(StormByte::Safe::MemoryOrder::Acquire);
		if (ready(owner))
			return;
		m_wakeGeneration.wait(generation, StormByte::Safe::MemoryOrder::Acquire);
	}
}

void Pipe::Wait(void* owner, bool (*ready)(void*) noexcept,
	void (*completed)(void*, std::chrono::nanoseconds) noexcept) noexcept {
	StormByte::Safe::UniqueLock lock(m_wait);
	const auto started = std::chrono::steady_clock::now();
	WaitWake(owner, ready);
	completed(owner, std::chrono::steady_clock::now() - started);
}

void Pipe::Close(void* owner, void (*trace)(void*, std::string_view) noexcept) noexcept {
	auto log = [owner, trace](std::string_view phase) noexcept {
		if (trace)
			trace(owner, phase);
	};
	log("input begin");
	m_in.Eof();
	log("input end; output begin");
	m_out.Eof();
	log("output end; fork lock begin");
	StormByte::Safe::UniqueLock lock(m_forkMutex);
	log("fork lock acquired");
	m_closed = true;
	for (auto& fork : m_forks) {
		if (trace)
			trace(owner, "pipe EOF fork begin");
		fork.second->Eof();
		if (trace)
			trace(owner, "pipe EOF fork end");
	}
	log("complete");
}

void Pipe::Abort() noexcept {
	m_in.Abort();
	m_out.Abort();
	{
		StormByte::Safe::UniqueLock lock(m_forkMutex);
		m_closed = true;
		for (auto& fork : m_forks) {
			fork.second->Abort();
		}
	}
	Wake();
}

Pipe& Pipe::CloneTo(int track, Pipe& dest) {
	return CloneTo(track, dest, track);
}

Pipe& Pipe::CloneTo(int track, Pipe& dest, int output) {
	dest.Listen();
	auto hopper = StormByte::Safe::Shared<ItemSink>::MakePointer<ItemSink>();
	if (!dest.BindInput(output, *hopper))
		return dest;
	StormByte::Safe::UniqueLock lock(m_forkMutex);
	if (m_forkDestinations.contains({track, &dest, output}))
		return dest;
	const auto destination = m_forkDestinations.emplace(
		std::make_tuple(track, &dest, output), m_forks.size()).first;
	try {
		m_forks.emplace_back(output, std::move(hopper));
	} catch (...) {
		m_forkDestinations.erase(destination);
		throw;
	}
	if (m_closed)
		m_forks.back().second->Eof();
	return dest;
}

bool Pipe::RedirectCloneTo(int track, Pipe& previous, Pipe& dest) {
	dest.Listen();
	{
		StormByte::Safe::UniqueLock lock(m_forkMutex);
		if (!m_forkDestinations.contains({track, &previous, track})
			|| m_forkDestinations.contains({track, &dest, track}))
			return false;
	}
	auto hopper = StormByte::Safe::Shared<ItemSink>::MakePointer<ItemSink>();
	if (!dest.BindInput(track, *hopper))
		return false;
	StormByte::Safe::UniqueLock lock(m_forkMutex);
	const auto found = m_forkDestinations.find({track, &previous, track});
	if (found == m_forkDestinations.end() || m_forkDestinations.contains({track, &dest, track}))
		return false;
	const auto index = found->second;
	m_forkDestinations.emplace(std::make_tuple(track, &dest, track), index);
	if (m_closed)
		hopper->Eof();
	m_forks[index].second = std::move(hopper);
	m_forkDestinations.erase(found);
	return true;
}

void Pipe::Drain() noexcept {
	m_out.Drain();
}

bool Pipe::Ready() const noexcept {
	return m_in.Ready();
}

bool Pipe::InputEof() const noexcept {
	return m_in.EoF();
}

Pipe::Lane::Lane(Pipe& from, int track) noexcept
:	m_from(&from), m_track(track) {}

Pipe::Lane Pipe::To(int track) noexcept {
	return Lane(*this, track);
}

Pipe& Pipe::Lane::operator>>(Pipe& dest) {
	dest.Listen();
	dest.BindInput(m_track, m_from->m_out);
	return dest;
}

Pipe& Pipe::operator>>(Pipe& dest) {
	dest.Listen();
	m_out >> dest.m_in;
	return dest;
}

Pipe& Pipe::operator>>(Item::PointerType& item) noexcept {
	const auto keys = m_in.Keys();
	int chosen = 0;
	bool found = false;
	StormByte::Size best_size = 0;
	std::int64_t best_ts = std::numeric_limits<std::int64_t>::max();

	for (const int key : keys) {
		const StormByte::Size n = m_in.Size(key);
		if (n == 0)
			continue;
		const auto ts = FrontTs(m_in.Front(key));
		const std::int64_t stamp = ts.value_or(std::numeric_limits<std::int64_t>::max());
		if (!found
			|| n > best_size
			|| (n == best_size && stamp < best_ts)
			|| (n == best_size && stamp == best_ts && key < chosen)) {
			found = true;
			chosen = key;
			best_size = n;
			best_ts = stamp;
		}
	}

	item = found ? m_in.Pop(chosen) : m_in.Pop();
	return *this;
}

Pipe& Pipe::operator<<(Item::PointerType item) noexcept {
	if (!item)
		return *this;
	const int key = item->Track();
	const auto packet = StormByte::Safe::DynamicPointerCast<StormByte::Multimedia::Pipeline::Packet>(item);
	const bool measuring = packet && StormByte::Multimedia::Backend::Pipeline::Packet::Measuring(*packet);
	StormByte::Safe::Deque<Fork> forks;
	{
		StormByte::Safe::UniqueLock lock(m_forkMutex);
		for (const auto& [destination, index] : m_forkDestinations) {
			if (std::get<0>(destination) == key && (!measuring || std::get<1>(destination)->MeasuresInput()))
				forks.push_back(m_forks[index]);
		}
	}
	for (auto& fork : forks) {
		if (auto copy = item->Clone()) {
			copy->m_track = fork.first;
			if (!PushWeighted(*fork.second, fork.first, copy))
				return *this;
		}
	}
	if (!PushWeighted(m_out, key, item))
		return *this;
	return *this;
}

namespace StormByte::Multimedia::Backend::Pipeline {
	Pipe& operator>>(Pipe::Item::PointerType& item, Pipe& pipe) noexcept {
		pipe << std::move(item);
		return pipe;
	}

	Pipe& operator>>(Pipe::Item::PointerType&& item, Pipe& pipe) noexcept {
		pipe << std::move(item);
		return pipe;
	}
}
