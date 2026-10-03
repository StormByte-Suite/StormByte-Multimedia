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

#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/backend/pipeline/host.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/backend/pipeline/pumper.hxx>
#include <StormByte/multimedia/backend/pipeline/worker.hxx>
#include <StormByte/multimedia/log.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>

#include <chrono>
#include <format>
#include <limits>
#include <string>
#include <utility>

using namespace StormByte::Multimedia::Pipeline;
namespace Backend = StormByte::Multimedia::Backend;
using StormByte::Logger::Level;

class Step::Surface final: public StormByte::Multimedia::Backend::Pipeline::Host {
	public:
		explicit Surface(Step& step) noexcept
		:	m_step(step) {}

		void Emit(Item::PointerType item) noexcept override {
			m_step.Emit(std::move(item));
		}

		void Wait() noexcept override {
			m_step.Wait();
		}

		bool Stopping() const noexcept override {
			return m_step.Stopping();
		}

		void Fail(std::string reason) noexcept override {
			m_step.Fail(std::move(reason));
		}

		void Log(StormByte::Logger::Level level, std::string_view message) noexcept override {
			m_step.Log(level, message);
		}

		void Ended() noexcept override {
			m_step.m_exhausted = true;
		}

		bool Exhausted() const noexcept override {
			return m_step.m_exhausted;
		}

		Item::PointerType Pull() noexcept override {
			Item::PointerType item;
			m_step.pipe() >> item;
			if (item)
				m_step.m_telemetry->RecordInput(item->Kind());
			return item;
		}

		bool InputEof() const noexcept override {
			return m_step.pipe().InputEof();
		}

		void CloseOutput() noexcept override {
			m_step.pipe().Close();
		}

		void BecameReady() noexcept override {
			m_step.Log(Level::Debug, "ready");
			m_step.m_wake.notify_all();
		}

		void RecordWork(std::chrono::nanoseconds duration) noexcept override {
			m_step.m_telemetry->RecordProcess(duration);
			m_step.RecordWork(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
		}

		StageTelemetry& Telemetry() noexcept override {
			return *m_step.m_telemetry;
		}

		void DumpWork() noexcept override {
			m_step.DumpWork();
		}

	private:
		Step& m_step;
};

Step::Step(std::shared_ptr<StormByte::Logger::Log> log,
	enum Producer name,
	Kinds receives, Kinds produces) noexcept
:	m_log(StormByte::Multimedia::UseLog(std::move(log), ToString(name))),
	m_name(name),
	m_receives(receives),
	m_produces(produces),
	m_wake(),
	m_pipe(std::make_unique<Backend::Pipeline::Pipe>(m_wake)),
	m_surface(std::make_unique<Surface>(*this)),
	m_telemetry(std::make_shared<StageTelemetry>()),
	m_exhausted(false),
	m_workN(0),
	m_workMin(std::numeric_limits<std::int64_t>::max()),
	m_workMax(0),
	m_lastWork(0) {
	Log(Level::Notice, "created");
}

Step::~Step() noexcept {
	Halt();
}

State Step::Status() const noexcept {
	if (m_pumper)
		return m_pumper->Status();
	return m_error ? State::Failed : State::Created;
}

std::shared_ptr<const StageTelemetry> Step::Telemetry() const noexcept {
	const std::string origin = Label();
	m_telemetry->SetOrigin(origin);
	return m_telemetry;
}

void Step::CloseHoppers() noexcept {
	m_pipe->Close();
}

void Step::Fail(std::string reason) noexcept {
	m_telemetry->SetError(reason);
	m_error = std::move(reason);
	Log(Level::Error, *m_error);
	if (m_pumper)
		m_pumper->Fail(*m_error);
	CloseHoppers();
	m_wake.notify_all();
}

bool Step::Failed() const noexcept {
	return Status() == State::Failed;
}

bool Step::Ready() const noexcept {
	return Status() == State::Ready;
}

void Step::Stop() noexcept {
	const State state = Status();
	const bool signaled = state == State::Created || state == State::Ready;
	if (m_pumper)
		m_pumper->Stop();
	if (signaled)
		Log(Level::LowLevel, "stop");
	CloseHoppers();
	m_wake.notify_all();
}

std::size_t Step::InputCeiling() const noexcept {
	if (!m_plan || m_plan->Tracks().empty())
		return 0;
	const Backend::Pipeline::Ceiling cap{m_plan, m_name, m_plan->Tracks()[0]};
	if (Receives().Has(Kind::Frame) && cap.Frames() != 0)
		return cap.Frames();
	if (Receives().Has(Kind::Packet) && cap.Packets() != 0)
		return cap.Packets();
	return 0;
}

bool Step::Stopping() const noexcept {
	const State state = Status();
	return state == State::Stopping || state == State::Stopped || state == State::Failed;
}

const std::optional<std::string>& Step::Error() const noexcept {
	return m_error;
}

std::condition_variable& Step::Wake() noexcept {
	return m_wake;
}

Backend::Pipeline::Pipe& Step::pipe() noexcept {
	return *m_pipe;
}

const Backend::Pipeline::Pipe& Step::pipe() const noexcept {
	return *m_pipe;
}

void Step::Wait() noexcept {
	Log(Level::LowLevel, "wait");
	std::unique_lock lock(m_wait);
	const auto started = std::chrono::steady_clock::now();
	m_wake.wait(lock, [this] {
		return Stopping() || m_pipe->Ready() || m_pipe->InputEof() || WakeNow();
	});
	m_telemetry->RecordWait(std::chrono::steady_clock::now() - started);
	Log(Level::LowLevel, "wake");
	AfterWait();
}

bool Step::WakeNow() const noexcept {
	return false;
}

void Step::AfterWait() noexcept {}

void Step::Emit(Item::PointerType item) noexcept {
	if (item)
		m_telemetry->RecordOutput(item->Kind());
	item >> *m_pipe;
}

Item::PointerType Step::CloneItem(const Item& item) const noexcept {
	return item.Clone();
}

std::string Step::Label() const noexcept {
	return std::string(ToString(m_name));
}

void Step::Log(StormByte::Logger::Level level, std::string_view message) noexcept {
	if (!m_log)
		return;
	*m_log << level << message << std::endl;
}

void Step::RecordWork(std::int64_t microseconds) noexcept {
	if (microseconds < 0)
		microseconds = 0;
	++m_workN;
	m_lastWork = microseconds;
	if (microseconds < m_workMin)
		m_workMin = microseconds;
	if (microseconds > m_workMax)
		m_workMax = microseconds;
}

std::int64_t Step::LastWork() const noexcept {
	return m_lastWork;
}

void Step::DumpWork() noexcept {
	if (m_workN == 0)
		return;
	Log(Level::Debug, std::format("work n={} min={}us max={}us",
		m_workN, m_workMin, m_workMax));
	m_workN = 0;
	m_workMin = std::numeric_limits<std::int64_t>::max();
	m_workMax = 0;
	m_lastWork = 0;
}

Backend::Pipeline::Host& Step::Face() noexcept {
	return *m_surface;
}

void Step::Mount(std::unique_ptr<Backend::Pipeline::Pumper> pumper,
	std::unique_ptr<Backend::Pipeline::Worker> worker) noexcept {
	if (m_pumper || !pumper || !worker)
		return;
	m_pumper = std::move(pumper);
	m_pumper->Bind(std::move(worker));
}

void Step::Launch() noexcept {
	if (!m_pumper || Stopping())
		return;
	Log(Level::LowLevel, "launch");
	m_pipe->Listen();
	m_pumper->Launch();
}

void Step::Halt() noexcept {
	Stop();
	if (m_pumper)
		m_pumper->Halt();
}

Step& StormByte::Multimedia::Pipeline::operator>>(Step& from, Step& to) noexcept {
	if (!to.m_plan)
		to.m_plan = from.m_plan;
	from.pipe() >> to.pipe();
	if (const std::size_t cap = to.InputCeiling(); cap > 0 && to.m_plan) {
		const auto& tracks = to.m_plan->Tracks();
		for (auto it = tracks.begin(); it != tracks.end(); ++it) {
			if (*it)
				to.pipe().Capacity((*it)->In(), cap);
		}
	}
	from.Log(Level::Debug, "bound to " + std::string(ToString(to.m_name)));
	return to;
}
