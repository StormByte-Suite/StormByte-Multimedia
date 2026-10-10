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

#include <StormByte/multimedia/backend/pipeline/decoder.hxx>
#include <StormByte/multimedia/backend/pipeline/demuxer.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/pumper/source.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/worker/demux.hxx>
#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/filters.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/memory_order.hxx>
#include <StormByte/safe/pointers.hxx>

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using StormByte::Logger::Level;
using StormByte::Buffer::IO::BufferedFileReader;

namespace {
	std::string Ns(const StormByte::Safe::Optional<Property::Duration>& value) noexcept {
		if (!value)
			return "-";
		return std::format("{}", value->Nanoseconds().count());
	}
}

Demuxer::Demuxer(StormByte::Safe::Shared<StormByte::Logger::Log> log) noexcept
: Demuxer(std::move(log), StormByte::Safe::MakeShared<class Progress>()) {}

Demuxer::Demuxer(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Shared<class Progress> progress) noexcept
: Step(std::move(log), Producer::Demuxer, Kinds{}, Kinds{Kind::Packet}),
	m_eof(false), m_positionNs(-1),
	m_progress(progress ? std::move(progress) : StormByte::Safe::MakeShared<class Progress>()) {
	Mount(StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Pumper::Source>(Face()),
		StormByte::Safe::MakeUnique<Backend::Pipeline::Detail::Worker::Demux>(*this));
	Launch();
}

Demuxer::~Demuxer() noexcept = default;

Demuxer::operator bool() const noexcept {
	return !Failed() && !m_eof && Ready() && m_backend && m_backend->IsOpen();
}

bool Demuxer::Eof() const noexcept {
	return m_eof;
}

StormByte::Safe::Optional<Property::Duration> Demuxer::Position() const noexcept {
	const std::int64_t ns = m_positionNs.load(StormByte::Safe::MemoryOrder::Acquire);
	if (ns < 0)
		return std::nullopt;
	return Property::Duration{std::chrono::nanoseconds{ns}};
}

Progress::Pointer Demuxer::Progress() const noexcept {
	return m_progress;
}

void Demuxer::WaitForPlan() noexcept {
	StormByte::Safe::UniqueLock lock(m_planMutex);
	m_planPresent.wait(lock, [this]() {
		return Stopping() || static_cast<bool>(m_plan);
	});
	if (!Stopping() && m_plan)
		LatchDuration();
}

void Demuxer::LatchDuration() noexcept {
	if (!m_progress || !m_plan || !static_cast<bool>(*m_plan))
		return;
	if (const auto& duration = m_plan->Snapshot().Duration(); duration)
		m_progress->SetDurationNs(duration->Nanoseconds().count());
}

void Demuxer::ReachedEof() noexcept {
	if (m_measuring && m_filters) {
		Log(Level::Debug, "measure eof");
		m_eof = false;
		m_filters->CloseMeasureSource();
		return;
	}

	if (!m_eof)
		Log(Level::Notice, "eof");
	m_eof = true;
}

void Demuxer::Measure(StormByte::Safe::Vector<int> tracks) noexcept {
	m_measureTracks = std::move(tracks);
	m_measuring = !m_measureTracks.empty();
	m_eof = false;
	if (m_progress && m_measuring)
		m_progress->HasMeasure(true);
}

bool Demuxer::Measuring() const noexcept {
	return m_measuring;
}

bool Demuxer::WakeNow() const noexcept {
	return !m_measuring;
}

bool Demuxer::Rewind() noexcept {
	if (m_plan) {
		auto& reader = Origin();
		if (reader.IsOpen() && !reader.Rewind()) {
			Fail("reader rewind failed");
			return false;
		}
	}

	if (!m_backend || !m_backend->Rewind(*this))
		return false;
	m_eof = false;
	m_positionNs.store(-1, StormByte::Safe::MemoryOrder::Release);
	m_nextSerial.clear();
	m_measuring = false;
	m_measureTracks.clear();
	if (m_progress)
		m_progress->MeasureDone();
	Wake();
	m_planPresent.notify_all();
	return true;
}

const StormByte::Buffer::IO::BufferedLocationReader& Demuxer::Origin() const noexcept {
	return m_plan->Reader();
}

StormByte::Buffer::IO::BufferedLocationReader& Demuxer::Origin() noexcept {
	return m_plan->Reader();
}

StormByte::Safe::Unique<Backend::Pipeline::Decoder> Demuxer::OpenDecoder(Decoder& decoder) noexcept {
	if (!m_backend || !m_backend->IsOpen()) {
		decoder.Fail("demuxer is not open");
		return {};
	}

	return m_backend->OpenDecoder(*this, decoder);
}

Packet::PointerType Demuxer::Wrap(
	int track,
	Type type,
	StormByte::Buffer::FIFO payload,
	StormByte::Safe::Optional<Property::Duration> pts,
	StormByte::Safe::Optional<Property::Duration> dts,
	StormByte::Safe::Optional<Property::Duration> duration,
	bool keyframe,
	StormByte::Safe::Unique<Backend::Pipeline::Packet> backend) noexcept {
	if (m_measuring) {
		const auto& tracks = m_measureTracks;
		if (std::find(tracks.begin(), tracks.end(), track) == tracks.end())
			return {};
	}

	const std::uint64_t serial = m_nextSerial[track]++;
	Log(Level::LowLevel, std::format("t={} {} {}:0 pts={} dts={} dur={} key={} bytes={}",
		track, ToString(type), serial, Ns(pts), Ns(dts), Ns(duration),
		keyframe ? 1 : 0, static_cast<std::uint64_t>(payload.Size())));
	auto packet = Packet::PointerType::MakePointer<Packet>(
		track,
		type,
		Producer::Demuxer,
		std::move(payload),
		std::move(pts),
		std::move(dts),
		std::move(duration),
		keyframe,
		StormByte::Safe::Vector<SideData>{},
		nullptr,
		serial,
		0);
	if (backend)
		packet->Bind(std::move(backend));
	return packet;
}

Decoder& StormByte::Multimedia::Pipeline::operator>>(Demuxer& demuxer, Decoder& decoder) noexcept {
	if (decoder.Failed())
		return decoder;
	if (!decoder.Plan())
		decoder.m_plan = demuxer.m_plan;
	if (demuxer.Failed()) {
		decoder.Fail(demuxer.Error().value_or(StormByte::Safe::String("demuxer failed")));
		return decoder;
	}
	if (const auto plan = decoder.Plan(); plan) {
		try {
			if (!decoder.Implementation()) {
				for (const auto& track : plan->Tracks()) {
					if (!track || track->In() != decoder.Index() || !track->Config())
						continue;
					const auto& implementation = track->Config()->Implementation();
					if (implementation.Decoder)
						decoder.Implementation(implementation.Decoder.value());
					break;
				}
			}
			for (const auto& stream : plan->Snapshot().Streams()) {
				if (stream.Index() != decoder.Index())
					continue;
				decoder.Stamp(stream.Metadata().Language(), stream.Metadata().Title());
				break;
			}
		}
		catch (...) {
			decoder.Fail("failed to copy source stream tags");
			return decoder;
		}
	}

	decoder.AttachOrigin(demuxer);
	demuxer.pipe().To(decoder.Index()) >> decoder.pipe();
	if (const std::size_t cap = decoder.InputCeiling(); cap > 0)
		decoder.pipe().Capacity(decoder.Index(), cap);
	demuxer.Log(Level::Debug, std::format("bind decoder t={}", decoder.Index()));
	return decoder;
}
