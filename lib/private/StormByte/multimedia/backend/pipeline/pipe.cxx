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

#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <cstdint>
#include <limits>
#include <optional>
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

Pipe::Pipe(std::condition_variable& wake) noexcept
:	m_wake(&wake) {}

Pipe::~Pipe() noexcept {
	m_in.Unnotify();
	m_out.Unnotify();
	for (auto& fork : m_forks)
		fork.hopper.Unnotify();
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

void Pipe::Capacity(int track, std::size_t n) noexcept {
	m_in.Capacity(track, n);
}

void Pipe::Listen() noexcept {
	m_in.Notify(*m_wake);
}

void Pipe::Close() noexcept {
	m_in.Eof();
	m_out.Eof();
	for (auto& fork : m_forks)
		fork.hopper.Eof();
}

Pipe& Pipe::CloneTo(int track, Pipe& dest) noexcept {
	dest.Listen();
	m_forks.emplace_back();
	m_forks.back().track = track;
	m_forks.back().hopper.To(track) >> dest.In();
	dest.m_inTracks.insert(track);
	return dest;
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

Pipe& Pipe::Lane::operator>>(Pipe& dest) noexcept {
	if (dest.m_inTracks.contains(m_track)) {
		dest.m_in.To(m_track) >> m_from->m_out;
		return dest;
	}

	dest.Listen();
	m_from->m_out.To(m_track) >> dest.m_in;
	dest.m_inTracks.insert(m_track);
	return dest;
}

Pipe& Pipe::operator>>(Pipe& dest) noexcept {
	dest.Listen();
	m_out >> dest.m_in;
	return dest;
}

Pipe& Pipe::operator>>(Item::PointerType& item) noexcept {
	const auto keys = m_in.Keys();
	int chosen = 0;
	bool found = false;
	std::size_t best_size = 0;
	std::int64_t best_ts = std::numeric_limits<std::int64_t>::max();

	for (const int key : keys) {
		const std::size_t n = m_in.Size(key);
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
	for (auto& fork : m_forks) {
		if (fork.track != key)
			continue;
		if (auto copy = item->Clone())
			fork.hopper.Push(key, std::move(copy));
	}
	m_out.Push(key, std::move(item));
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
