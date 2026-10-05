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
#include <StormByte/multimedia/backend/pipeline/detail/pumper/through.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/worker/remux.hxx>
#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/demuxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/remuxer.hxx>
#include <StormByte/multimedia/property/duration.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <StormByte/safe/pointers.hxx>

#include <format>
#include <string>

using namespace StormByte::Multimedia::Pipeline;
using StormByte::Logger::Level;

Remuxer::Remuxer(StormByte::Safe::Shared<StormByte::Logger::Log> log, int in) noexcept
:	Step(std::move(log), Producer::Remuxer, Kinds{Kind::Packet}, Kinds{Kind::Packet}),
	m_index(in) {
	Mount(StormByte::Safe::Heap::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Detail::Pumper::Through>(Face()),
		StormByte::Safe::Heap::MakeUnique<StormByte::Multimedia::Backend::Pipeline::Detail::Worker::Remux>(*this));
	Launch();
}

Remuxer::~Remuxer() noexcept = default;

std::size_t Remuxer::InputCeiling() const noexcept {
	const StormByte::Safe::Shared<const StormByte::Multimedia::Pipeline::Plan> plan = Plan();
	const auto* track = StormByte::Multimedia::Backend::Pipeline::TrackByIn(plan, m_index);
	if (!track)
		return 0;
	const StormByte::Multimedia::Backend::Pipeline::Ceiling cap{plan, Producer::Remuxer, *track};
	return cap.Packets();
}

void Remuxer::Emit(Packet::PointerType packet) noexcept {
	Step::Emit(std::move(packet));
}

Remuxer& StormByte::Multimedia::Pipeline::operator>>(Demuxer& demuxer, Remuxer& remuxer) noexcept {
	if (remuxer.Failed())
		return remuxer;
	if (!remuxer.m_plan)
		remuxer.m_plan = demuxer.m_plan;
	if (demuxer.Failed()) {
		remuxer.Fail(demuxer.Error().value_or(StormByte::Safe::String("demuxer failed")));
		return remuxer;
	}

	demuxer.pipe().To(remuxer.In()) >> remuxer.pipe();
	if (const std::size_t cap = remuxer.InputCeiling(); cap > 0)
		remuxer.pipe().Capacity(remuxer.In(), cap);
	demuxer.Log(Level::Debug, std::format("bind remuxer t={}", remuxer.In()));
	return remuxer;
}

StormByte::Safe::String Remuxer::Label() const noexcept {
	if (m_plan && *m_plan) {
		for (const auto& stream : m_plan->Snapshot().Streams()) {
			if (stream.Index() == m_index)
				return StormByte::Safe::String("Remuxer(" + std::string(stream.Codec().Name()) + ")");
		}
	}

	return StormByte::Safe::String("Remuxer(t=" + std::to_string(m_index) + ")");
}
