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

#include <StormByte/multimedia/backend/pipeline/pumper.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>

#include <chrono>
#include <utility>

using namespace StormByte::Multimedia::Backend::Pipeline;
using StormByte::Multimedia::Pipeline::State;
using StormByte::Logger::Level;

namespace {
	bool Terminal(State state) noexcept {
		return state == State::Failed || state == State::Stopped;
	}

	std::chrono::nanoseconds Elapsed(std::chrono::steady_clock::time_point started) noexcept {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now() - started);
	}
}

Pumper::Pumper(Host& host) noexcept
:	m_host(host),
	m_state(State::Created) {}

Pumper::~Pumper() noexcept {
	Halt();
}

void Pumper::Bind(std::unique_ptr<Worker> worker) noexcept {
	if (m_thread.joinable() || m_worker || !worker)
		return;
	m_worker = std::move(worker);
}

void Pumper::Launch() noexcept {
	if (m_thread.joinable() || !m_worker || Stopping())
		return;
	m_thread = std::jthread([this](std::stop_token token) {
		(void)token;
		auto& telemetry = m_host.Telemetry();
		telemetry.Start();
		const auto setup_started = std::chrono::steady_clock::now();
		m_worker->Setup();
		telemetry.RecordSetup(std::chrono::steady_clock::now() - setup_started);
		if (Stopping()) {
			telemetry.SetState(Status());
			telemetry.Finish();
			return;
		}
		State expected = State::Created;
		if (!m_state.compare_exchange_strong(expected, State::Ready,
				std::memory_order_acq_rel, std::memory_order_acquire)) {
			telemetry.SetState(Status());
			telemetry.Finish();
			return;
		}
		telemetry.SetState(State::Ready);
		m_host.BecameReady();
		Pump();
		m_host.CloseOutput();
		expected = State::Stopping;
		if (!m_state.compare_exchange_strong(expected, State::Stopped,
				std::memory_order_acq_rel, std::memory_order_acquire)) {
			expected = State::Ready;
			m_state.compare_exchange_strong(expected, State::Stopped,
				std::memory_order_acq_rel, std::memory_order_acquire);
		}

		m_host.Log(Level::LowLevel, "stopped");
		telemetry.SetState(Status());
		telemetry.Finish();
	});
}

void Pumper::Halt() noexcept {
	Stop();
	if (m_thread.joinable()) {
		m_thread.request_stop();
		m_thread.join();
	}

	State expected = State::Stopping;
	if (!m_state.compare_exchange_strong(expected, State::Stopped,
			std::memory_order_acq_rel, std::memory_order_acquire)) {
		expected = State::Ready;
		m_state.compare_exchange_strong(expected, State::Stopped,
			std::memory_order_acq_rel, std::memory_order_acquire);
	}
	m_host.Telemetry().SetState(Status());
}

void Pumper::Stop() noexcept {
	State current = m_state.load(std::memory_order_acquire);
	while (current == State::Created || current == State::Ready) {
		if (m_state.compare_exchange_weak(current, State::Stopping,
				std::memory_order_acq_rel, std::memory_order_acquire)) {
			m_host.Telemetry().SetState(State::Stopping);
			break;
		}
	}
}

State Pumper::Status() const noexcept {
	return m_state.load(std::memory_order_acquire);
}

bool Pumper::Failed() const noexcept {
	return Status() == State::Failed;
}

const std::optional<std::string>& Pumper::Error() const noexcept {
	return m_error;
}

bool Pumper::Stopping() const noexcept {
	const State state = Status();
	return state == State::Stopping || state == State::Stopped || state == State::Failed;
}

void Pumper::Fail(std::string reason) noexcept {
	m_error = std::move(reason);
	State current = m_state.load(std::memory_order_acquire);
	while (!Terminal(current)) {
		if (m_state.compare_exchange_weak(current, State::Failed,
				std::memory_order_acq_rel, std::memory_order_acquire))
			break;
	}
	m_host.Telemetry().SetState(Status());
}

void Pumper::PumpSource() noexcept {
	if (!m_worker)
		return;
	for (;;) {
		if (Stopping() || m_host.Exhausted())
			break;
		const auto started = std::chrono::steady_clock::now();
		m_worker->Process({});
		m_host.RecordWork(Elapsed(started));
		if (Stopping())
			break;
		if (m_host.Exhausted()) {
			m_host.DumpWork();
			break;
		}
	}
}

void Pumper::PumpPop() noexcept {
	if (!m_worker)
		return;
	for (;;) {
		if (Stopping())
			break;
		Multimedia::Pipeline::Item::PointerType item = m_host.Pull();
		if (!item) {
			if (!m_host.InputEof() && !Stopping()) {
				m_host.Wait();
				continue;
			}

			if (!Stopping()) {
				const auto started = std::chrono::steady_clock::now();
				m_worker->Process({});
				m_host.RecordWork(Elapsed(started));
				m_host.DumpWork();
			}

			break;
		}

		const auto started = std::chrono::steady_clock::now();
		m_worker->Process(std::move(item));
		m_host.RecordWork(Elapsed(started));
		if (Failed())
			break;
	}
}

Worker* Pumper::Bound() noexcept {
	return m_worker.get();
}

const Worker* Pumper::Bound() const noexcept {
	return m_worker.get();
}

Host& Pumper::Owner() noexcept {
	return m_host;
}

const Host& Pumper::Owner() const noexcept {
	return m_host;
}
