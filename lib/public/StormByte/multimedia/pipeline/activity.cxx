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

#include <StormByte/multimedia/pipeline/activity.hxx>
#include <StormByte/safe/unique_lock.hxx>

#include <atomic>

using namespace StormByte::Multimedia::Pipeline;

namespace {
	std::atomic<std::uint64_t> NextWorker{1};

	std::chrono::nanoseconds Now() noexcept {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
	}

	bool Terminal(ActivityState state) noexcept {
		return state == ActivityState::Finished || state == ActivityState::Failed;
	}

	void Add(WorkerActivitySnapshot& value, std::chrono::nanoseconds duration) noexcept {
		switch (value.State) {
			case ActivityState::Preparing:
				value.Preparing += duration;
				break;
			case ActivityState::Processing:
				value.Processing += duration;
				break;
			case ActivityState::WaitingInput:
				value.WaitingInput += duration;
				break;
			case ActivityState::WaitingOutput:
				value.WaitingOutput += duration;
				break;
			case ActivityState::WaitingBudget:
				value.WaitingBudget += duration;
				break;
			case ActivityState::Paused:
				value.Paused += duration;
				break;
			case ActivityState::Unknown:
				value.Unknown += duration;
				break;
			case ActivityState::Finished:
			case ActivityState::Failed:
				break;
		}
	}
}

WorkerActivity::WorkerActivity(ActivityRole role, int track) {
	m_value.Id = NextWorker.fetch_add(1, std::memory_order_relaxed);
	m_value.Role = role;
	m_value.Track = track;
}

WorkerActivity::~WorkerActivity() noexcept = default;

void WorkerActivity::Start() noexcept {
	StormByte::Safe::UniqueLock lock(m_mutex);
	if (m_value.Available && !Terminal(m_value.State))
		return;
	const auto id = m_value.Id;
	const auto generation = m_value.Generation + 1;
	const auto role = m_value.Role;
	const auto track = m_value.Track;
	m_value = {};
	m_value.Id = id;
	m_value.Generation = generation;
	m_value.Role = role;
	m_value.Track = track;
	m_value.Available = true;
	m_value.State = ActivityState::Preparing;
	m_value.StartedAt = m_value.SampledAt = m_changed = Now();
}

ActivityState WorkerActivity::Exchange(ActivityState state) noexcept {
	StormByte::Safe::UniqueLock lock(m_mutex);
	const auto previous = m_value.State;
	if (!m_value.Available || Terminal(previous) || Terminal(state))
		return previous;
	const auto now = Now();
	Add(m_value, now - m_changed);
	m_changed = now;
	m_value.State = state;
	return previous;
}

void WorkerActivity::Finish(bool failed) noexcept {
	StormByte::Safe::UniqueLock lock(m_mutex);
	if (!m_value.Available || Terminal(m_value.State))
		return;
	const auto now = Now();
	Add(m_value, now - m_changed);
	m_value.SampledAt = m_value.FinishedAt = m_changed = now;
	m_value.State = failed ? ActivityState::Failed : ActivityState::Finished;
}

WorkerActivitySnapshot WorkerActivity::Snapshot() const noexcept {
	StormByte::Safe::UniqueLock lock(m_mutex);
	auto value = m_value;
	if (value.Available && !Terminal(value.State)) {
		value.SampledAt = Now();
		Add(value, value.SampledAt - m_changed);
	}
	return value;
}
