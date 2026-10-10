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

#include <StormByte/multimedia/backend/pipeline/detail/pumper/through.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/worker/filter.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/backend/pipeline/host.hxx>
#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/log.hxx>
#include <StormByte/multimedia/name_thread.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <chrono>
#include <deque>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <utility>

using StormByte::Multimedia::Pipeline::Filter::Analytics;
using StormByte::Multimedia::Pipeline::Filter::FFmpeg;
using StormByte::Multimedia::Pipeline::Filter::Packet;
using StormByte::Multimedia::Pipeline::Filter::Process;
using StormByte::Multimedia::Pipeline::Filter::ProcessTwoPasses;
using StormByte::Multimedia::Pipeline::Item;
using StormByte::Multimedia::Pipeline::Kind;
using StormByte::Multimedia::Pipeline::Kinds;
using StormByte::Multimedia::Pipeline::State;
using StormByte::Multimedia::ToString;
using StormByte::Logger::Level;

namespace {
	int TrackOf(const StormByte::Multimedia::Pipeline::Item& item) noexcept {
		if (item.Kind() == Kind::Frame)
			return static_cast<const StormByte::Multimedia::Pipeline::Frame&>(item).Track();
		return static_cast<const StormByte::Multimedia::Pipeline::Packet&>(item).Track();
	}

	std::uint64_t PartOf(const StormByte::Multimedia::Pipeline::Item& item) noexcept {
		if (item.Kind() == Kind::Frame)
			return static_cast<const StormByte::Multimedia::Pipeline::Frame&>(item).Part();
		return static_cast<const StormByte::Multimedia::Pipeline::Packet&>(item).Part();
	}

	StormByte::Safe::Optional<std::uint64_t> SerialOf(const StormByte::Multimedia::Pipeline::Item& item) noexcept {
		if (item.Kind() == Kind::Frame)
			return static_cast<const StormByte::Multimedia::Pipeline::Frame&>(item).Serial();
		return static_cast<const StormByte::Multimedia::Pipeline::Packet&>(item).Serial();
	}

	bool IsAnalytics(const FFmpeg& node) noexcept {
		return dynamic_cast<const Analytics*>(&node) != nullptr;
	}
}

class FFmpeg::Surface final: public StormByte::Multimedia::Backend::Pipeline::Host {
	public:
		explicit Surface(FFmpeg& owner) noexcept
		:	m_owner(owner) {}

		void Emit(Item::PointerType item) noexcept override {
			m_owner.Emit(std::move(item));
		}

		void Wait() noexcept override {
			m_owner.Wait();
		}

		bool Stopping() const noexcept override {
			return m_owner.Stopping();
		}

		void Fail(std::string_view reason) noexcept override {
			m_owner.Fail(reason);
		}

		void Log(StormByte::Logger::Level level, std::string_view message) noexcept override {
			m_owner.Log(level, message);
		}

		void Ended() noexcept override {
			m_owner.m_exhausted = true;
		}

		bool Exhausted() const noexcept override {
			return m_owner.m_exhausted;
		}

		Item::PointerType Pull() noexcept override {
			Item::PointerType item;
			m_owner.pipe() >> item;
			if (item)
				m_owner.m_telemetry->RecordInput(item->Kind());
			return item;
		}

		bool InputEof() const noexcept override {
			return m_owner.pipe().InputEof();
		}

		void CloseOutput() noexcept override {
			m_owner.pipe().Close();
		}

		void BecameReady() noexcept override {
			m_owner.Log(Level::Debug, "ready");
			m_owner.Wake();
		}

		void RecordWork(std::chrono::nanoseconds duration) noexcept override {
			m_owner.m_telemetry->RecordProcess(duration);
			m_owner.RecordWork(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
		}

		StormByte::Multimedia::Pipeline::StageTelemetry& Telemetry() noexcept override {
			return *m_owner.m_telemetry;
		}

		void DumpWork() noexcept override {
			m_owner.DumpWork();
		}

	private:
		FFmpeg& m_owner;
};

class FFmpeg::PrivateState {
	public:
		explicit PrivateState(FFmpeg& owner) noexcept
		:	surface(owner) {}

		StormByte::Multimedia::Backend::Pipeline::Pipe pipe;					///< Input/output queues and wait synchronization
		Surface surface;														///< Host adapter borrowing the owning filter
		std::unique_ptr<StormByte::Multimedia::Backend::Pipeline::Pumper> pumper;	///< Worker driver allocated and destroyed in Multimedia
		std::deque<Item::PointerType> queue;										///< Held input items awaiting delayed filter output
};

FFmpeg::FFmpeg(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	std::string_view name, Kinds receives, Kinds produces) noexcept
:	m_log(std::move(log)),
	m_name(name),
	m_receives(receives),
	m_produces(produces),
	m_state(new PrivateState(*this)),
	m_telemetry(StormByte::Safe::MakeShared<StormByte::Multimedia::Pipeline::StageTelemetry>()),
	m_exhausted(false),
	m_workN(0),
	m_workMin(std::numeric_limits<std::int64_t>::max()),
	m_workMax(0),
	m_lastWork(0),
	m_hold(0),
	m_heldFor(0) {
	m_state->pumper = std::make_unique<StormByte::Multimedia::Backend::Pipeline::Detail::Pumper::Through>(Face());
	m_state->pumper->Bind(StormByte::Safe::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Detail::Worker::Filter>(*this));
}

FFmpeg::~FFmpeg() noexcept {
	Halt();
	delete m_state;
}

StormByte::Safe::String FFmpeg::Name() const noexcept {
	StormByte::Safe::String name(ToString(Media()));
	name.append("/");
	name.append(m_name);
	return name;
}

StormByte::Safe::Shared<const StormByte::Multimedia::Pipeline::StageTelemetry> FFmpeg::Telemetry() const noexcept {
	const StormByte::Safe::String origin = Name();
	m_telemetry->SetOrigin(origin);
	return StormByte::Safe::StaticPointerCast<const StormByte::Multimedia::Pipeline::StageTelemetry>(m_telemetry);
}

void FFmpeg::Process(const Pipeline::Frame&) noexcept {}

void FFmpeg::Process(const Pipeline::Packet&) noexcept {}

class StormByte::Multimedia::Pipeline::Filter::Report FFmpeg::Report() const noexcept {
	return {};
}

State FFmpeg::Status() const noexcept {
	if (m_state->pumper)
		return m_state->pumper->Status();
	return m_error ? State::Failed : State::Created;
}

bool FFmpeg::Failed() const noexcept {
	return Status() == State::Failed;
}

const StormByte::Safe::Optional<StormByte::Safe::String>& FFmpeg::Error() const noexcept {
	return m_error;
}

StormByte::Size FFmpeg::InputCeiling() const noexcept {
	if (Media() == StormByte::Multimedia::Type::Video && m_receives.Has(Kind::Frame))
		return 27;
	if (m_receives.Has(Kind::Frame) || m_receives.Has(Kind::Packet))
		return 32;
	return 0;
}

void FFmpeg::Log(StormByte::Logger::Level level, std::string_view message) noexcept {
	if (!m_log)
		return;
	*m_log << level << message << std::endl;
}

void FFmpeg::Fail(StormByte::Safe::String reason) noexcept {
	m_hold = 0;
	m_heldFor = 0;
	m_state->queue.clear();
	m_telemetry->SetError(StormByte::Safe::String(reason));
	m_error = std::move(reason);
	const StormByte::Safe::String message = std::as_const(m_error).value();
	Log(Level::Error, message);
	if (m_state->pumper)
		m_state->pumper->Fail(static_cast<std::string>(message));
	CloseHoppers();
	Wake();
}

void FFmpeg::Fail(std::string_view reason) noexcept {
	Fail(StormByte::Safe::String(reason));
}

void FFmpeg::Hold(std::uint8_t n) noexcept {
	if (Held()) {
		Fail(StormByte::Safe::String("Hold while already Held"));
		return;
	}

	if (!m_current) {
		Fail(StormByte::Safe::String("Hold without a unit"));
		return;
	}

	m_hold = n == 0 ? std::numeric_limits<std::uint8_t>::max() : n;
	m_heldFor = 0;
	Log(Level::Debug, std::format("hold n={}", static_cast<unsigned>(m_hold)));
	Park();
}

void FFmpeg::Release() noexcept {
	if (!Held())
		return;
	Log(Level::Debug, std::format("release held={}", static_cast<unsigned>(m_heldFor)));
	m_hold = 0;
	m_heldFor = 0;
	auto parked = std::move(m_state->queue);
	if (m_current) {
		const bool queued = !parked.empty() &&
			std::find(parked.begin(), parked.end(), m_current) != parked.end();
		if (!queued)
			parked.push_back(std::move(m_current));
		else
			m_current.reset();
	}
	for (auto& item : parked) {
		m_current = std::move(item);
		if (!m_current)
			continue;
		if (IsAnalytics(*this)) {
			Work(std::move(m_current));
			continue;
		}

		if (m_current->Kind() == Pipeline::Kind::Frame) {
			if (MeasuringTwoPass())
				static_cast<ProcessTwoPasses*>(this)->Measure(
					static_cast<const Pipeline::Frame&>(*m_current));
			else
				Process(static_cast<const Pipeline::Frame&>(*m_current));
		}
		else
			Process(static_cast<const Pipeline::Packet&>(*m_current));
		if (Failed())
			return;
		if (MeasuringTwoPass()) {
			m_current.reset();
			continue;
		}
		if (m_current)
			Emit(std::move(m_current));
	}

	m_current.reset();
}

bool FFmpeg::Held() const noexcept {
	return m_hold > 0;
}

std::uint8_t FFmpeg::HeldFor() const noexcept {
	return m_heldFor;
}

void FFmpeg::Eof() noexcept {}

const StormByte::Multimedia::FFmpeg::AVFrame& FFmpeg::AVFrame() const noexcept {
	static StormByte::Multimedia::FFmpeg::AVFrame empty;
	static const bool primed = []() noexcept {
		empty.Reset(nullptr);
		return true;
	}();
	(void)primed;
	auto frame = StormByte::Safe::DynamicPointerCast<const Pipeline::Frame>(m_current);
	if (!frame || !frame->m_backend)
		return empty;
	return frame->m_backend->Handle();
}

const StormByte::Multimedia::FFmpeg::AVPacket& FFmpeg::AVPacket() const noexcept {
	static StormByte::Multimedia::FFmpeg::AVPacket empty;
	static const bool primed = []() noexcept {
		empty.Reset(nullptr);
		return true;
	}();
	(void)primed;
	auto packet = StormByte::Safe::DynamicPointerCast<const Pipeline::Packet>(m_current);
	if (!packet || !packet->m_backend)
		return empty;
	return packet->m_backend->Handle();
}

bool FFmpeg::MeasuringTwoPass() const noexcept {
	auto* two = dynamic_cast<const ProcessTwoPasses*>(this);
	return two && two->m_measuring;
}

void FFmpeg::Save(StormByte::Multimedia::FFmpeg::AVFrame&& incoming) noexcept {
	if (MeasuringTwoPass()) {
		(void)incoming;
		Log(Level::Warning, "Save during measure is a no-op");
		return;
	}
	auto frame = StormByte::Safe::DynamicPointerCast<Pipeline::Frame>(m_current);
	if (!frame)
		return;
	if (!incoming) {
		Log(Level::Warning, "Save of empty frame");
		return;
	}
	if (frame->m_backend && frame->m_backend->Handle().Get()
		&& incoming.Get() == frame->m_backend->Handle().Get()) {
		Fail(StormByte::Safe::String("Save of the current frame; paint a new AVFrame"));
		return;
	}
	if (!frame->m_backend)
		frame->m_backend = StormByte::Safe::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Frame>();
	frame->m_payload = StormByte::Buffer::FIFO{};
	frame->m_backend->Put(*frame, incoming.Detach());
	if (!frame->m_backend->ContentValid()) {
		Fail("failed to preserve required frame metadata during replacement");
		return;
	}
	if (!frame->m_backend->Warning().empty())
		Log(Level::Warning, frame->m_backend->Warning());
	if (!frame->m_backend->Diagnostic().empty())
		Log(Level::LowLevel, std::format("frame side-data{}", frame->m_backend->Diagnostic()));
	Log(Level::LowLevel, std::format("save frame t={} {}:{}",
		frame->Track(), frame->Serial().value_or(0), frame->Part()));
}

void FFmpeg::Save(StormByte::Multimedia::FFmpeg::AVPacket&& incoming) noexcept {
	if (MeasuringTwoPass()) {
		(void)incoming;
		Log(Level::Warning, "Save during measure is a no-op");
		return;
	}
	auto packet = StormByte::Safe::DynamicPointerCast<Pipeline::Packet>(m_current);
	if (!packet)
		return;
	if (!incoming) {
		Log(Level::Warning, "Save of empty packet");
		return;
	}
	if (packet->m_backend && packet->m_backend->Handle().Get()
		&& incoming.Get() == packet->m_backend->Handle().Get()) {
		Fail(StormByte::Safe::String("Save of the current packet; emit a new AVPacket"));
		return;
	}
	if (!packet->m_backend)
		packet->m_backend = StormByte::Safe::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Packet>();
	packet->m_payload = StormByte::Buffer::FIFO{};
	packet->m_backend->Handle().Reset(incoming.Detach());
	packet->m_backend->BindProperties(*packet);
	Log(Level::LowLevel, std::format("save packet t={} {}:{}",
		packet->Track(), packet->Serial().value_or(0), packet->Part()));
}

void FFmpeg::Open() noexcept {
	StormByte::Safe::String scope("Filters/");
	scope.append(Name());
	m_log = StormByte::Multimedia::UseLog(m_log, scope);
	NameThread(std::string("SB/MM:").append(std::string_view(m_name)));
	Log(Level::Notice, "setup");
	Clean();
	Setup();
}

void FFmpeg::LastChance(const Pipeline::Frame&) noexcept {}

void FFmpeg::LastChance(const Pipeline::Packet&) noexcept {}

void FFmpeg::Park() noexcept {
	if (!m_current)
		return;
	if (!m_state->queue.empty() && m_state->queue.back() == m_current)
		return;
	if (m_heldFor >= m_hold) {
		Fail(StormByte::Safe::String("Hold exceeded"));
		return;
	}

	m_state->queue.push_back(m_current);
	++m_heldFor;
}

void FFmpeg::CallLastChance() noexcept {
	if (!m_current)
		return;
	Log(Level::Debug, "last-chance");
	if (m_current->Kind() == Pipeline::Kind::Frame)
		LastChance(static_cast<const Pipeline::Frame&>(*m_current));
	else if (!IsAnalytics(*this))
		LastChance(static_cast<const Pipeline::Packet&>(*m_current));
}

void FFmpeg::Work(Pipeline::Item::PointerType item) noexcept {
	m_current = std::move(item);
	Log(Level::LowLevel, std::format("in t={} {}:{}",
		TrackOf(*m_current), SerialOf(*m_current).value_or(0), PartOf(*m_current)));

	if (IsAnalytics(*this)) {
		if (m_current->Kind() == Pipeline::Kind::Frame)
			Process(static_cast<const Pipeline::Frame&>(*m_current));
		if (Failed())
			return;
		if (m_current)
			Emit(std::move(m_current));
		return;
	}

	if (m_current->Kind() == Pipeline::Kind::Frame) {
		if (MeasuringTwoPass()) {
			const auto& frame = static_cast<const Pipeline::Frame&>(*m_current);
			static_cast<ProcessTwoPasses*>(this)->Measure(frame);
			if (auto* two = static_cast<ProcessTwoPasses*>(this); two->m_measureOwner) {
				if (const auto& pts = frame.Pts(); pts)
					two->m_measureOwner->NoteMeasure(pts->Nanoseconds().count());
			}
		}
		else
			Process(static_cast<const Pipeline::Frame&>(*m_current));
	}
	else
		Process(static_cast<const Pipeline::Packet&>(*m_current));
	if (Failed())
		return;
	if (MeasuringTwoPass()) {
		m_current.reset();
		return;
	}
	if (Held()) {
		if (m_heldFor >= m_hold) {
			CallLastChance();
			if (Held()) {
				Fail(StormByte::Safe::String("Hold exceeded"));
				return;
			}
		}

		else {
			Park();
			return;
		}
	}

	if (m_current)
		Emit(std::move(m_current));
}

void FFmpeg::Finish() noexcept {
	if (Held()) {
		if (!m_current && !m_state->queue.empty())
			m_current = m_state->queue.back();
		CallLastChance();
		if (Held())
			Fail(StormByte::Safe::String("Hold + EoF without Release"));
	}

	if (!Failed())
		Eof();
}

void FFmpeg::Emit(Pipeline::Item::PointerType item) noexcept {
	if (item)
		m_telemetry->RecordOutput(item->Kind());
	item >> m_state->pipe;
}

void FFmpeg::Wait() noexcept {
	Log(Level::LowLevel, "wait");
	m_state->pipe.Wait(this, [](void* owner) noexcept {
		auto& filter = *static_cast<FFmpeg*>(owner);
		if (filter.Stopping() || filter.pipe().Ready())
			return true;
		if (!filter.MeasuringTwoPass())
			return false;
		auto* two = static_cast<const ProcessTwoPasses*>(&filter);
		if (two->m_measureOwner && two->m_measureOwner->MeasureReadyToFinish())
			return true;
		return two->m_measureClosed.load(StormByte::Safe::MemoryOrder::Acquire)
			&& !two->m_measureDrained.load(StormByte::Safe::MemoryOrder::Acquire)
			&& !filter.pipe().Ready();
	}, [](void* owner, std::chrono::nanoseconds duration) noexcept {
		auto& filter = *static_cast<FFmpeg*>(owner);
		filter.m_telemetry->RecordWait(duration);
		filter.Log(Level::LowLevel, "wake");
		if (filter.MeasuringTwoPass()) {
			auto* two = static_cast<ProcessTwoPasses*>(&filter);
			if (two->m_measureClosed.load(StormByte::Safe::MemoryOrder::Acquire) && !filter.pipe().Ready())
				two->DrainMeasure();
			if (two->m_measureOwner)
				two->m_measureOwner->MaybeFinishMeasure();
		}
	});
}

void FFmpeg::Launch() noexcept {
	if (!m_state->pumper || Stopping())
		return;
	Log(Level::LowLevel, "launch");
	m_state->pipe.Listen();
	m_state->pumper->Launch();
}

void FFmpeg::Halt() noexcept {
	Stop();
	if (m_state->pumper)
		m_state->pumper->Halt();
}

void FFmpeg::Stop() noexcept {
	const State state = Status();
	const bool signaled = state == State::Created || state == State::Ready;
	if (m_state->pumper)
		m_state->pumper->Stop();
	if (signaled)
		Log(Level::LowLevel, "stop");
	CloseHoppers();
	Wake();
}

bool FFmpeg::Stopping() const noexcept {
	const State state = Status();
	return state == State::Stopping || state == State::Stopped || state == State::Failed;
}

StormByte::Multimedia::Backend::Pipeline::Host& FFmpeg::Face() noexcept {
	return m_state->surface;
}

void FFmpeg::Wake() noexcept {
	m_state->pipe.Wake();
}

StormByte::Multimedia::Backend::Pipeline::Pipe& FFmpeg::pipe() noexcept {
	return m_state->pipe;
}

const StormByte::Multimedia::Backend::Pipeline::Pipe& FFmpeg::pipe() const noexcept {
	return m_state->pipe;
}

void FFmpeg::CloseHoppers() noexcept {
	m_state->pipe.Close();
}

void FFmpeg::RecordWork(std::int64_t microseconds) noexcept {
	if (microseconds < 0)
		microseconds = 0;
	++m_workN;
	m_lastWork = microseconds;
	if (microseconds < m_workMin)
		m_workMin = microseconds;
	if (microseconds > m_workMax)
		m_workMax = microseconds;
}

void FFmpeg::DumpWork() noexcept {
	if (m_workN == 0)
		return;
	Log(Level::Debug, std::format("work n={} min={}us max={}us last={}us",
		m_workN, m_workMin, m_workMax, m_lastWork));
	m_workN = 0;
	m_workMin = std::numeric_limits<std::int64_t>::max();
	m_workMax = 0;
	m_lastWork = 0;
}

void FFmpeg::Clean() noexcept {}

void FFmpeg::Setup() noexcept {}

Process::Process(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name) noexcept
: FFmpeg(std::move(log), name, Kinds{Kind::Frame}, Kinds{Kind::Frame}) {}

Process::Process(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name,
	Kinds receives, Kinds produces) noexcept
: FFmpeg(std::move(log), name, receives, produces) {}

ProcessTwoPasses::ProcessTwoPasses(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name) noexcept
: StormByte::Multimedia::Pipeline::Filter::Process(std::move(log), name) {}

ProcessTwoPasses::ProcessTwoPasses(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name,
	Kinds receives, Kinds produces) noexcept
: StormByte::Multimedia::Pipeline::Filter::Process(std::move(log), name, receives, produces) {}

bool ProcessTwoPasses::Measuring() const noexcept {
	return m_measuring;
}

void ProcessTwoPasses::Measured() noexcept {}

void ProcessTwoPasses::EnterMeasure() noexcept {
	m_measuring = true;
	m_measureClosed.store(false, StormByte::Safe::MemoryOrder::Release);
	m_measureDrained.store(false, StormByte::Safe::MemoryOrder::Release);
}

void ProcessTwoPasses::LeaveMeasure() noexcept {
	Eof();
	DumpWork();
	m_measuring = false;
	m_measureClosed.store(false, StormByte::Safe::MemoryOrder::Release);
	m_measureDrained.store(false, StormByte::Safe::MemoryOrder::Release);
	Measured();
}

void ProcessTwoPasses::BindMeasure(StormByte::Multimedia::Pipeline::Filters* owner) noexcept {
	m_measureOwner = owner;
}

void ProcessTwoPasses::MeasureSourceClosed() noexcept {
	m_measureClosed.store(true, StormByte::Safe::MemoryOrder::Release);
	Wake();
}

void ProcessTwoPasses::DrainMeasure() noexcept {
	if (!m_measureClosed.load(StormByte::Safe::MemoryOrder::Acquire))
		return;
	bool expected = false;
	if (!m_measureDrained.compare_exchange_strong(expected, true,
			StormByte::Safe::MemoryOrder::AcqRel, StormByte::Safe::MemoryOrder::Acquire))
		return;
	if (m_measureOwner)
		m_measureOwner->OnMeasureFilterDrained();
}

Packet::Packet(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name) noexcept
: FFmpeg(std::move(log), name, Kinds{Kind::Packet}, Kinds{Kind::Packet}) {}

Packet::Packet(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name,
	Kinds receives, Kinds produces) noexcept
: FFmpeg(std::move(log), name, receives, produces) {}

Analytics::Analytics(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name) noexcept
: FFmpeg(std::move(log), name, Kinds{Kind::Frame}, Kinds{}) {}

Analytics::Analytics(StormByte::Safe::Shared<StormByte::Logger::Log> log, std::string_view name,
	Kinds receives, Kinds produces) noexcept
: FFmpeg(std::move(log), name, receives, produces) {}
