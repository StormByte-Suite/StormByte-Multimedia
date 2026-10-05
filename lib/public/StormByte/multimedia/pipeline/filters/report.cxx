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

#include <StormByte/multimedia/pipeline/filters/report.hxx>

#include <sstream>

using namespace StormByte::Multimedia::Pipeline::Filter;

Report::Report()
: m_status(Status::None), m_data() {}

Report::Report(Status status, StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data) noexcept
: m_status(status), m_data(std::move(data)) {}

Report::Report(const Report& other) = default;

Report::Report(Report&& other) noexcept = default;

Report::~Report() noexcept = default;

Report& Report::operator=(const Report& other) = default;

Report& Report::operator=(Report&& other) noexcept = default;

Report::Status Report::Kind() const noexcept {
	return m_status;
}

const StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>& Report::Data() const noexcept {
	return m_data;
}

StormByte::Safe::String Report::operator()() const {
	const char* kind = "none";
	if (m_status == Status::Ok)
		kind = "ok";
	else if (m_status == Status::Failed)
		kind = "failed";

	std::ostringstream out;
	out << "report status=" << kind;
	for (const auto& [key, value] : m_data)
		out << " " << key << "=" << value;
	return StormByte::Safe::String(out.str());
}
