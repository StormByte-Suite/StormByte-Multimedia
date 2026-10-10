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

#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/decoder.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/route.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>
#include <StormByte/multimedia/stream.hxx>
#include <StormByte/multimedia/type.hxx>
#include <StormByte/safe/pointers.hxx>

using namespace StormByte::Multimedia::Pipeline;

namespace {
	bool Terminal(State state) noexcept {
		return state == State::Stopped || state == State::Failed;
	}

	enum StormByte::Multimedia::Type StreamMedia(const Step& origin, int track) noexcept {
		const auto& plan = origin.Plan();
		if (!plan || !*plan)
			return StormByte::Multimedia::Type::Unknown;
		for (const auto& stream : plan->Snapshot().Streams()) {
			if (stream.Index() == track)
				return stream.Type();
		}
		return StormByte::Multimedia::Type::Unknown;
	}

	void Cap(StormByte::Multimedia::Backend::Pipeline::Pipe& pipe, int track, std::size_t n) noexcept {
		if (n > 0)
			pipe.Capacity(track, n);
	}
}

Route::Route(int track,
	StormByte::Safe::Shared<Step> origin,
	StormByte::Safe::Shared<Step> destination) noexcept
: m_track(track),
	m_origin(std::move(origin)),
	m_destination(std::move(destination)) {}

Route::~Route() noexcept {
	for (auto& filter : m_filters) {
		if (filter)
			filter->Halt();
	}

	for (auto& look : m_looks) {
		if (look)
			look->Halt();
	}
}

int Route::Track() const noexcept {
	return m_track;
}

void Route::Observe(StormByte::Safe::Shared<Filter::FFmpeg> analytics) noexcept {
	if (analytics)
		m_analytics.push_back(std::move(analytics));
}

Route& Route::Add(StormByte::Safe::Shared<Filter::FFmpeg> filter) noexcept {
	if (!filter)
		return *this;

	if (!StormByte::Safe::DynamicPointerCast<Filter::Process>(filter)
		&& !StormByte::Safe::DynamicPointerCast<Filter::Packet>(filter)
		&& !StormByte::Safe::DynamicPointerCast<Filter::Analytics>(filter)) {
		filter->Fail("inherit Process, Packet or Analytics; FFmpeg is not a leaf");
		return *this;
	}

	const Kinds receives = filter->Receives();
	const bool frame = receives.Has(Kind::Frame);
	const bool packet = receives.Has(Kind::Packet);

	Filter::FFmpeg& node = *filter;
	const bool analytics = static_cast<bool>(StormByte::Safe::DynamicPointerCast<Filter::Analytics>(filter));
	if (analytics)
		m_analytics.push_back(filter);
	else {
		if (packet)
			Hook(m_packets, node);
		if (frame)
			Hook(m_frames, node);
	}

	m_filters.push_back(std::move(filter));
	m_filters.back()->Launch();
	return *this;
}

void Route::Close() noexcept {
	if (!m_origin || !m_destination)
		return;
	Step& origin = *m_origin;
	Step& destination = *m_destination;

	const Kinds stretch = origin.Produces() & destination.Receives();
	if (stretch == Kinds{}) {
		destination.Fail("route stretch has no overlapping kinds");
		return;
	}

	const auto media = StreamMedia(origin, m_track);
	for (const auto& filter : m_filters) {
		if (!StormByte::Safe::DynamicPointerCast<Filter::Analytics>(filter)
			&& !filter->Receives().Has(stretch)) {
			destination.Fail(std::format("{} does not cover this stretch", filter->Name()));
			return;
		}

		if (media != StormByte::Multimedia::Type::Unknown && filter->Media() != media) {
			destination.Fail(std::format("{} media does not match this stretch", filter->Name()));
			return;
		}
	}

	Filter::FFmpeg* const packetFirst = m_packets.First();
	Filter::FFmpeg* const packetLast = m_packets.Last();
	Filter::FFmpeg* const frameFirst = m_frames.First();
	Filter::FFmpeg* const frameLast = m_frames.Last();

	if (packetLast != nullptr && frameFirst != nullptr) {
		packetLast->pipe().To(m_track) >> frameFirst->pipe();
		Cap(frameFirst->pipe(), m_track, frameFirst->InputCeiling());
	}

	Filter::FFmpeg* first = packetFirst != nullptr ? packetFirst : frameFirst;
	Filter::FFmpeg* last = frameLast != nullptr ? frameLast : packetLast;

	Cap(destination.pipe(), m_track, destination.InputCeiling());

	for (const auto& analyticsOwner : m_analytics) {
		Filter::FFmpeg* analytics = analyticsOwner.get();
		if (!analytics)
			continue;
		if (analytics->Leaf() != "frames")
			TapDecode(origin, *analytics);
		TapEncode(destination, *analytics);
		Cap(analytics->pipe(), m_track, analytics->InputCeiling());
		analytics->pipe().Drain();
	}

	if (first == nullptr) {
		origin.pipe().To(m_track) >> destination.pipe();
	}
	else {
		Cap(first->pipe(), m_track, first->InputCeiling());
		last->pipe().To(m_track) >> destination.pipe();
		origin.pipe().To(m_track) >> first->pipe();
	}
}

bool Route::Idle() const noexcept {
	for (const auto& filter : m_filters) {
		if (filter && !Terminal(filter->Status()))
			return false;
	}

	for (const auto& look : m_looks) {
		if (look && !Terminal(look->Status()))
			return false;
	}

	return true;
}

StormByte::Safe::Vector<Filter::Report> Route::Reports() const noexcept {
	StormByte::Safe::Vector<Filter::Report> reports;
	reports.reserve(m_filters.size());
	for (const auto& filter : m_filters)
		reports.push_back(filter->Report());
	return reports;
}

void Route::Hook(Lane& lane, Filter::FFmpeg& filter) noexcept {
	filter.pipe().Listen();
	if (lane.LastProcess != nullptr) {
		lane.LastProcess->pipe().To(m_track) >> filter.pipe();
		Cap(filter.pipe(), m_track, filter.InputCeiling());
	}
	if (lane.FirstProcess == nullptr)
		lane.FirstProcess = &filter;
	lane.LastProcess = &filter;
}

void Route::TapDecode(Step& origin, Filter::FFmpeg& analytics) noexcept {
	if (origin.Produces().Has(Kind::Frame)) {
		origin.pipe().CloneTo(m_track, analytics.pipe());
		Cap(analytics.pipe(), m_track, analytics.InputCeiling());
		return;
	}

	if (!origin.Produces().Has(Kind::Packet))
		return;
	for (const auto& look : m_looks) {
		if (look->m_lookStamp)
			continue;
		look->pipe().CloneTo(m_track, analytics.pipe());
		Cap(analytics.pipe(), m_track, analytics.InputCeiling());
		return;
	}
	StormByte::Safe::Shared<Decoder> look = StormByte::Safe::Shared<Decoder>::MakePointer<Decoder>(
		origin.m_log, m_track, Decoder::SourceLook{});
	look->m_plan = origin.Plan();
	look->pipe().CloneTo(m_track, analytics.pipe());
	look->pipe().Drain();
	Cap(look->pipe(), m_track, look->InputCeiling());
	Cap(analytics.pipe(), m_track, analytics.InputCeiling());
	origin.pipe().CloneTo(m_track, look->pipe());
	m_looks.push_back(std::move(look));
}

void Route::TapEncode(Step& destination, Filter::FFmpeg& analytics) noexcept {
	if (destination.m_name != Producer::Remuxer && destination.m_name != Producer::Encoder)
		return;
	for (const auto& held : m_looks) {
		if (held->m_lookStamp != destination.m_name)
			continue;
		held->pipe().CloneTo(m_track, analytics.pipe());
		Cap(analytics.pipe(), m_track, analytics.InputCeiling());
		return;
	}
	StormByte::Safe::Shared<Decoder> look;
	if (destination.m_name == Producer::Remuxer)
		look = StormByte::Safe::Shared<Decoder>::MakePointer<Decoder>(
			destination.m_log, m_track, Decoder::RemuxLook{});
	else if (destination.m_name == Producer::Encoder)
		look = StormByte::Safe::Shared<Decoder>::MakePointer<Decoder>(
			destination.m_log, m_track, Decoder::EncodeLook{});
	else
		return;
	look->m_plan = destination.Plan();
	look->pipe().Listen();
	look->pipe().CloneTo(m_track, analytics.pipe());
	look->pipe().Drain();
	Cap(look->pipe(), m_track, look->InputCeiling());
	Cap(analytics.pipe(), m_track, analytics.InputCeiling());
	destination.pipe().CloneTo(m_track, look->pipe());
	m_looks.push_back(std::move(look));
}
