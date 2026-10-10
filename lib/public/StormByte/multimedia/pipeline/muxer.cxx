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
#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/container.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/pumper/sink.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/worker/mux.hxx>
#include <StormByte/multimedia/backend/pipeline/demuxer.hxx>
#include <StormByte/multimedia/backend/pipeline/muxer.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/safe/pointers.hxx>

#include <chrono>
#include <cstdlib>
#include <format>
#include <mutex>
#include <string_view>
#include <utility>

extern "C" {
	#include <libavcodec/codec_par.h>
	#include <libavutil/rational.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using StormByte::Logger::Level;

namespace {
	bool Muxable(enum StormByte::Multimedia::Type type) noexcept {
		return type == StormByte::Multimedia::Type::Video
			|| type == StormByte::Multimedia::Type::Audio
			|| type == StormByte::Multimedia::Type::Subtitle;
	}

	bool PlanWantsAttachments(const Plan* plan) noexcept {
		if (!plan)
			return false;
		for (const auto& track : plan->Tracks()) {
			if (track && track->Type() == StormByte::Multimedia::Type::Attachment)
				return true;
		}
		return false;
	}
}

Muxer::Muxer(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept
: Step(std::move(log), Producer::Muxer, Kinds{Kind::Packet}, Kinds{}),
	m_container(nullptr),
	m_origin(nullptr),
	m_closed(false),
	m_reserved(0),
	m_positionNs(-1) {
	Mount(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Pumper::Sink>(Face()),
		StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Worker::Mux>(*this));
	Launch();
}

Muxer::~Muxer() noexcept = default;

Muxer::operator bool() const noexcept {
	return !Failed() && !m_closed.load(StormByte::Safe::MemoryOrder::Acquire) && Ready()
		&& m_backend && m_backend->IsOpen();
}

bool Muxer::Closed() const noexcept {
	return m_closed.load(StormByte::Safe::MemoryOrder::Acquire) || Failed();
}

StormByte::Safe::Optional<Property::Duration> Muxer::Position() const noexcept {
	const std::int64_t ns = m_positionNs.load(StormByte::Safe::MemoryOrder::Acquire);
	if (ns < 0)
		return std::nullopt;
	return Property::Duration{std::chrono::nanoseconds{ns}};
}

const Container& Muxer::Destination() const noexcept {
	return *m_container;
}

std::size_t Muxer::ExpectedSlots() const noexcept {
	if (!m_plan)
		return 0;
	std::size_t n = 0;
	for (const auto& track : m_plan->Tracks()) {
		if (!track)
			continue;
		if (Muxable(track->Type()))
			++n;
	}

	return n;
}

bool Muxer::Armed() const noexcept {
	const auto reserved = m_reserved.load(StormByte::Safe::MemoryOrder::Acquire);
	if (reserved == 0)
		return false;
	const auto expected = ExpectedSlots();
	if (expected == 0)
		return false;
	return reserved == expected;
}

bool Muxer::Ready() const noexcept {
	return Step::Ready() && Armed();
}

std::size_t Muxer::InputCeiling() const noexcept {
	if (!m_plan || m_plan->Tracks().empty())
		std::abort();
	for (const auto& held : m_plan->Tracks()) {
		if (!held)
			continue;
		const Track& track = *held;
		if (!Muxable(track.Type()))
			continue;
		const Backend::Pipeline::Ceiling cap{m_plan, Producer::Muxer, track};
		const std::size_t n = cap.Packets();
		return n > 0 ? n : 8;
	}
	std::abort();
}

StormByte::Safe::Optional<StormByte::Safe::String> Muxer::Language(int output_index) const noexcept {
	if (!m_language.contains(output_index))
		return std::nullopt;
	return m_language.at(output_index);
}

void Muxer::Language(int output_index, StormByte::Safe::String language) noexcept {
	if (language.empty())
		m_language.erase(output_index);
	else
		m_language[output_index] = std::move(language);
}

StormByte::Safe::Optional<StormByte::Safe::String> Muxer::Title(int output_index) const noexcept {
	if (!m_title.contains(output_index))
		return std::nullopt;
	return m_title.at(output_index);
}

void Muxer::Title(int output_index, StormByte::Safe::String title) noexcept {
	if (title.empty())
		m_title.erase(output_index);
	else
		m_title[output_index] = std::move(title);
}

void Muxer::WaitArmed() noexcept {
	pipe().WaitWake(this, [](void* context) noexcept {
		const auto& owner = *static_cast<Muxer*>(context);
		return owner.Stopping() || owner.Armed();
	});
}

bool Muxer::SpawnBackend() noexcept {
	if (m_backend)
		return true;
	if (!m_plan) {
		Fail("muxer has no plan");
		return false;
	}
	const auto check = m_plan->Check();
	if (!check) {
		Fail(check.error()->what());
		return false;
	}

	m_container = &m_plan->Container();
	if (!m_container->HasAccess(Access{Operation::Write})) {
		Fail("container does not allow write");
		return false;
	}

	m_backend = StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Muxer::FFmpeg::Container>();
	return true;
}

bool Muxer::ArmOctets() noexcept {
	if (m_backend && m_backend->IsOpen())
		return true;
	if (!SpawnBackend())
		return false;

	auto& writer = m_plan->Writer();
	if (!writer.IsOpen() && !writer.Open()) {
		Fail("muxer writer open failed");
		return false;
	}
	if (writer.Truncate().status != StormByte::Buffer::IO::Status::Ok) {
		Fail("muxer writer truncate failed");
		return false;
	}

	if (!m_backend->BindSink(*this))
		return false;

	Log(Level::Notice, std::format("writer {}", std::string_view{m_plan->Path()}));
	return true;
}

bool Muxer::BindPlanAttachments() noexcept {
	if (!m_backend)
		return SpawnBackend();
	m_attachments.clear();
	if (!PlanWantsAttachments(m_plan.get()))
		return m_backend->BindAttachments(*this, m_attachments);
	if (!m_plan || !*m_plan) {
		Fail("plan snapshot is not available");
		return false;
	}

	for (const auto& item : m_plan->Snapshot().Attachments()) {
		m_attachments.emplace_back(
			item.FileName(),
			item.MimeType(),
			StormByte::Buffer::FIFO{item.Payload()});
	}
	return m_backend->BindAttachments(*this, m_attachments);
}

void Muxer::FlushOctets() noexcept {
	if (!m_plan)
		return;
	auto& writer = m_plan->Writer();
	if (writer.IsOpen())
		static_cast<void>(writer.Flush());
}

bool Muxer::BindEncoderStream(Encoder& encoder, void* avStream) noexcept {
	return encoder.MuxBindStream(avStream);
}

bool Muxer::RemuxCodec(int inIndex, void*& params, void* timeBase) noexcept {
	params = nullptr;
	if (!timeBase || !m_origin || m_origin->Failed() || !m_origin->m_backend
		|| (!m_origin->Ready() && !m_origin->Eof()))
		return false;
	return m_origin->m_backend->CloneStream(
		inIndex,
		*reinterpret_cast<::AVCodecParameters**>(&params),
		*static_cast<StormByte::Multimedia::FFmpeg::AVRational*>(timeBase));
}

void Muxer::ClockMuxDone() noexcept {
	if (m_progress)
		m_progress->MuxDone();
}

void Muxer::ClockPass(std::int64_t ns) noexcept {
	if (m_progress)
		m_progress->SetPassNs(ns);
}

Encoder& StormByte::Multimedia::Pipeline::operator>>(Encoder& encoder, Muxer& muxer) noexcept {
	if (!muxer.m_plan)
		muxer.m_plan = encoder.m_plan;
	if (!encoder.m_plan)
		encoder.m_plan = muxer.m_plan;
	muxer.pipe().Listen();
	if (muxer.Failed() || encoder.Failed())
		return encoder;
	if (!muxer.SpawnBackend())
		return encoder;
	if (!muxer.ArmOctets())
		return encoder;

	if (!muxer.m_backend->ReserveEncoder(muxer, encoder))
		return encoder;
	encoder.pipe().To(encoder.Index()) >> muxer.pipe();
	if (const std::size_t cap = muxer.InputCeiling(); cap > 0)
		muxer.pipe().Capacity(encoder.Index(), cap);
	muxer.m_reserved.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::AcqRel);
	muxer.Log(Level::Debug, std::format("reserve encoder t={}", encoder.Index()));
	if (muxer.Armed())
		muxer.Wake();
	return encoder;
}

Remuxer& StormByte::Multimedia::Pipeline::operator>>(Remuxer& remuxer, Muxer& muxer) noexcept {
	if (!muxer.m_plan)
		muxer.m_plan = remuxer.m_plan;
	if (!remuxer.m_plan)
		remuxer.m_plan = muxer.m_plan;
	muxer.pipe().Listen();
	if (muxer.Failed() || remuxer.Failed())
		return remuxer;
	if (!muxer.SpawnBackend())
		return remuxer;
	if (muxer.m_wired.contains(remuxer.In())) {
		muxer.Fail("track already wired to muxer");
		return remuxer;
	}
	if (!muxer.ArmOctets())
		return remuxer;

	if (!muxer.m_backend->ReserveRemux(muxer, remuxer.In()))
		return remuxer;
	muxer.m_wired.insert(remuxer.In());
	remuxer.pipe().To(remuxer.In()) >> muxer.pipe();
	if (const std::size_t cap = muxer.InputCeiling(); cap > 0)
		muxer.pipe().Capacity(remuxer.In(), cap);
	muxer.m_reserved.fetch_add(std::uint64_t{1}, StormByte::Safe::MemoryOrder::AcqRel);
	muxer.Log(Level::Debug, std::format("reserve remux t={}", remuxer.In()));
	if (muxer.Armed())
		muxer.Wake();
	return remuxer;
}

Muxer& StormByte::Multimedia::Pipeline::operator>>(Demuxer& demuxer, Muxer& muxer) noexcept {
	if (!muxer.m_plan)
		muxer.m_plan = demuxer.m_plan;
	if (!demuxer.m_plan)
		demuxer.m_plan = muxer.m_plan;
	if (muxer.Failed())
		return muxer;
	if (!demuxer.Plan()) {
		muxer.Fail("demuxer has no plan");
		return muxer;
	}
	if (!muxer.SpawnBackend())
		return muxer;
	if (!muxer.ArmOctets())
		return muxer;

	muxer.m_origin = &demuxer;
	muxer.m_progress = demuxer.m_progress;
	if (!muxer.BindPlanAttachments())
		return muxer;
	muxer.Log(Level::Debug, "bound remux origin");
	return muxer;
}
