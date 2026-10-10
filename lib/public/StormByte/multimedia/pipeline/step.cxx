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

#include <StormByte/multimedia/backend/pipeline/ceiling.hxx>
#include <StormByte/multimedia/backend/pipeline/host.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/backend/pipeline/pumper.hxx>
#include <StormByte/multimedia/backend/pipeline/worker.hxx>
#include <StormByte/multimedia/log.hxx>
#include <StormByte/multimedia/pipeline/step.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/safe/pointers.hxx>

#include <chrono>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <utility>

using namespace StormByte::Multimedia::Pipeline;
namespace Backend = StormByte::Multimedia::Backend;
using StormByte::Logger::Level;

namespace {
	class MountedWorker final: public Backend::Pipeline::Worker {
		public:
			MountedWorker(Backend::Pipeline::Host& host,
				StormByte::Safe::Unique<Backend::Pipeline::Worker> worker) noexcept
			: Backend::Pipeline::Worker(host), m_worker(std::move(worker)) {}

			void Setup() noexcept override {
				m_worker->Setup();
			}

			void Process(Item::PointerType item) noexcept override {
				m_worker->Process(std::move(item));
			}

		protected:
			void Flush() noexcept override {}

		private:
			StormByte::Safe::Unique<Backend::Pipeline::Worker> m_worker;
	};
}

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

		void Fail(std::string_view reason) noexcept override {
			m_step.Fail(StormByte::Safe::String(reason));
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
			m_step.Wake();
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

class Step::PrivateState {
	public:
		explicit PrivateState(Step& owner) noexcept
		:	surface(owner) {}

		Backend::Pipeline::Pipe pipe;							///< Input/output queues and wait synchronization
		Surface surface;										///< Host adapter borrowing the owning step
		StormByte::Safe::Unique<Backend::Pipeline::Pumper> pumper;	///< Worker driver owned through Base heap callbacks
};

Step::Step(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	enum Producer name,
	Kinds receives, Kinds produces) noexcept
:	m_log(StormByte::Multimedia::UseLog(std::move(log), ToString(name))),
	m_name(name),
	m_receives(receives),
	m_produces(produces),
	m_state(new PrivateState(*this)),
	m_telemetry(StormByte::Safe::MakeShared<StageTelemetry>()),
	m_exhausted(false),
	m_workN(0),
	m_workMin(std::numeric_limits<std::int64_t>::max()),
	m_workMax(0),
	m_lastWork(0) {
	Log(Level::Notice, "created");
}

Step::~Step() noexcept {
	Halt();
	delete m_state;
}

State Step::Status() const noexcept {
	if (m_state->pumper)
		return m_state->pumper->Status();
	return m_error ? State::Failed : State::Created;
}

StormByte::Safe::Shared<const StageTelemetry> Step::Telemetry() const noexcept {
	const auto origin = Label();
	m_telemetry->SetOrigin(origin);
	return m_telemetry;
}

void Step::CloseHoppers() noexcept {
	m_state->pipe.Close();
}

void Step::Fail(StormByte::Safe::String reason) noexcept {
	m_telemetry->SetError(reason);
	m_error = std::move(reason);
	const StormByte::Safe::String message = *m_error;
	Log(Level::Error, message);
	if (m_state->pumper)
		m_state->pumper->Fail(static_cast<std::string>(message));
	CloseHoppers();
	Wake();
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
	if (m_state->pumper)
		m_state->pumper->Stop();
	if (signaled)
		Log(Level::LowLevel, "stop");
	CloseHoppers();
	Wake();
}

StormByte::Size Step::InputCeiling() const noexcept {
	if (!m_plan || m_plan->Tracks().empty())
		return 0;
	const StormByte::Safe::Shared<const StormByte::Multimedia::Pipeline::Plan> plan = m_plan;
	const Backend::Pipeline::Ceiling cap{plan, m_name, plan->Tracks()[0]};
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

const StormByte::Safe::Optional<StormByte::Safe::String>& Step::Error() const noexcept {
	return m_error;
}

void Step::Wake() noexcept {
	m_state->pipe.Wake();
}

Backend::Pipeline::Pipe& Step::pipe() noexcept {
	return m_state->pipe;
}

const Backend::Pipeline::Pipe& Step::pipe() const noexcept {
	return m_state->pipe;
}

void Step::Wait() noexcept {
	Log(Level::LowLevel, "wait");
	m_state->pipe.Wait(this, [](void* owner) noexcept {
		auto& step = *static_cast<Step*>(owner);
		return step.Stopping() || step.pipe().Ready() || step.pipe().InputEof() || step.WakeNow();
	}, [](void* owner, std::chrono::nanoseconds duration) noexcept {
		auto& step = *static_cast<Step*>(owner);
		step.m_telemetry->RecordWait(duration);
		step.Log(Level::LowLevel, "wake");
		step.AfterWait();
	});
}

bool Step::WakeNow() const noexcept {
	return false;
}

void Step::AfterWait() noexcept {}

void Step::Emit(Item::PointerType item) noexcept {
	if (item)
		m_telemetry->RecordOutput(item->Kind());
	item >> m_state->pipe;
}

Item::PointerType Step::CloneItem(const Item& item) const noexcept {
	return item.Clone();
}

StormByte::Safe::String Step::Label() const noexcept {
	return StormByte::Safe::String(ToString(m_name));
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
	return m_state->surface;
}

void Step::Mount(StormByte::Safe::Unique<Backend::Pipeline::Pumper> pumper,
	StormByte::Safe::Unique<Backend::Pipeline::Worker> worker) noexcept {
	if (m_state->pumper || !pumper || !worker)
		return;
	m_state->pumper = std::move(pumper);
	m_state->pumper->Bind(StormByte::Safe::MakeUnique<MountedWorker>(Face(), std::move(worker)));
}

void Step::Launch() noexcept {
	if (!m_state->pumper || Stopping())
		return;
	Log(Level::LowLevel, "launch");
	m_state->pipe.Listen();
	m_state->pumper->Launch();
}

void Step::Halt() noexcept {
	Stop();
	if (m_state->pumper)
		m_state->pumper->Halt();
}

Step& StormByte::Multimedia::Pipeline::operator>>(Step& from, Step& to) noexcept {
	if (!to.m_plan)
		to.m_plan = from.m_plan;
	if (to.m_plan) {
		for (const auto& track : to.m_plan->Tracks()) {
			if (track && track->Type() != Type::Attachment)
				from.pipe().To(track->In()) >> to.pipe();
		}
	}
	else
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
