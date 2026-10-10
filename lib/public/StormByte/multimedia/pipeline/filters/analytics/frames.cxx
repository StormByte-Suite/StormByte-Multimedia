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

#include <StormByte/multimedia/pipeline/filters/analytics/frames.hxx>
#include <StormByte/multimedia/pipeline/filters/report.hxx>
#include <StormByte/multimedia/pipeline/typedefs.hxx>

#include <format>

using StormByte::Multimedia::Pipeline::Filter::Video::CountFrames;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Type;
using StormByte::Logger::Level;

namespace {
	bool DistLook(Producer producer) noexcept {
		return producer == Producer::Encoder || producer == Producer::Remuxer;
	}
}

CountFrames::CountFrames(Safe::Shared<StormByte::Logger::Log> log) noexcept
: Analytics(std::move(log), "frames") {}

CountFrames::~CountFrames() noexcept = default;

enum Type CountFrames::Media() const noexcept {
	return Type::Video;
}

void CountFrames::Clean() noexcept {
	m_frames.clear();
}

void CountFrames::Setup() noexcept {}

void CountFrames::Process(const Pipeline::Frame& frame) noexcept {
	const Type type = frame.Type();
	if (type != Type::Video && type != Type::Audio && type != Type::Subtitle)
		return;
	if (!DistLook(frame.Producer()))
		return;
	auto count = m_frames[frame.Track()];
	count = static_cast<std::uint64_t>(count) + 1;
	Log(Level::LowLevel, std::format("count t={} producer={} pts={}",
		frame.Track(), static_cast<int>(frame.Producer()),
		frame.Pts() ? frame.Pts()->Nanoseconds().count() : 0));
}

void CountFrames::Eof() noexcept {
	for (const auto& [track, n] : std::as_const(m_frames))
		Log(Level::Notice, std::format("t={} frames={}", track, n));
	Log(Level::Debug, std::format("eof tracks={}", m_frames.size()));
}

class StormByte::Multimedia::Pipeline::Filter::Report CountFrames::Report() const noexcept {
	if (m_frames.empty())
		return { Filter::Report::Status::Failed, {} };

	Safe::Map<Safe::String, Safe::String> data;
	if (m_frames.size() == 1)
		data.emplace(Safe::String("frames"), Safe::String(std::to_string(m_frames.begin()->second)));
	else {
		for (const auto& [track, n] : m_frames)
			data.emplace(Safe::String(std::format("{}.frames", track)), Safe::String(std::to_string(n)));
	}
	return { Filter::Report::Status::Ok, std::move(data) };
}
