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

#include <StormByte/multimedia/pipeline/progress.hxx>

#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string_view>

using namespace StormByte::Multimedia::Pipeline;

Progress::Progress() noexcept = default;

Progress::~Progress() noexcept = default;

Progress::Values Progress::Snapshot() const noexcept {
	std::lock_guard lock(m_lock);
	const double all = All();
	const Phase phase = m_calculatingDuration ? Phase::CalculatingDuration
		: all == 100.0 ? Phase::Complete
		: m_hasMeasure && !m_measureDone ? Phase::Measure : Phase::Processing;
	return {phase, m_durationCalculation, Measure(), Analytics(), Processing(), all,
		MeasureComplete(), AnalyticsComplete(), ProcessingComplete(), MuxComplete()};
}

double Progress::Processing() const noexcept {
	std::lock_guard lock(m_lock);
	if (m_calculatingDuration)
		return 0.0;
	return m_passDone || m_muxDone ? 100.0 : Axis(m_passNs, m_durationNs);
}

bool Progress::ProcessingComplete() const noexcept {
	std::lock_guard lock(m_lock);
	return m_passDone;
}

bool Progress::MuxComplete() const noexcept {
	std::lock_guard lock(m_lock);
	return m_muxDone;
}

StormByte::Safe::Optional<double> Progress::DurationCalculation() const noexcept {
	std::lock_guard lock(m_lock);
	return m_durationCalculation;
}

bool Progress::CalculatingDuration() const noexcept {
	std::lock_guard lock(m_lock);
	return m_calculatingDuration;
}

void Progress::BeginDurationCalculation() noexcept {
	std::lock_guard lock(m_lock);
	m_calculatingDuration = true;
	m_durationIndicator = '|';
	m_durationCalculation.reset();
}

void Progress::SetDurationCalculation(StormByte::Safe::Optional<double> percent) noexcept {
	std::lock_guard lock(m_lock);
	if (!percent) {
		m_calculatingDuration = false;
		m_durationCalculation.reset();
		return;
	}
	double value = *percent;
	if (!(value >= 0.0))
		value = 0.0;
	if (value > 100.0)
		value = 100.0;
	if (m_durationCalculation && value < *m_durationCalculation)
		value = *m_durationCalculation;
	m_calculatingDuration = true;
	m_durationCalculation = value;
}

double Progress::Axis(std::int64_t pos, std::int64_t dur) noexcept {
	if (dur <= 0 || pos <= 0)
		return 0.0;
	if (pos >= dur)
		return 100.0;
	return (static_cast<double>(pos) * 100.0) / static_cast<double>(dur);
}

StormByte::Safe::Optional<double> Progress::Measure() const noexcept {
	std::lock_guard lock(m_lock);
	if (m_calculatingDuration)
		return std::nullopt;
	if (!m_hasMeasure)
		return std::nullopt;
	if (m_measureDone)
		return 100.0;
	return Axis(m_measureNs, m_durationNs);
}

StormByte::Safe::Optional<double> Progress::Analytics() const noexcept {
	std::lock_guard lock(m_lock);
	if (m_calculatingDuration)
		return std::nullopt;
	if (!m_hasAnalytics)
		return std::nullopt;
	if (m_analyticsDone)
		return 100.0;
	return Axis(m_analyticsNs, m_durationNs);
}

bool Progress::HasMeasure() const noexcept {
	std::lock_guard lock(m_lock);
	return m_hasMeasure;
}

bool Progress::HasAnalytics() const noexcept {
	std::lock_guard lock(m_lock);
	return m_hasAnalytics;
}

bool Progress::MeasureComplete() const noexcept {
	std::lock_guard lock(m_lock);
	return !m_hasMeasure || m_measureDone;
}

bool Progress::AnalyticsComplete() const noexcept {
	std::lock_guard lock(m_lock);
	return !m_hasAnalytics || m_analyticsDone;
}

double Progress::All() const noexcept {
	std::lock_guard lock(m_lock);
	if (m_calculatingDuration)
		return 0.0;
	if (m_muxDone && MeasureComplete() && AnalyticsComplete()) {
		m_all = 100.0;
		return m_all;
	}

	const double measure = !m_hasMeasure ? 0.0
		: (m_measureDone ? 100.0 : Axis(m_measureNs, m_durationNs));
	const double pass = m_muxDone ? 100.0 : Axis(m_passNs, m_durationNs);
	const double analytics = !m_hasAnalytics ? 0.0
		: (m_analyticsDone ? 100.0 : Axis(m_analyticsNs, m_durationNs));

	const double wMeasure = m_hasMeasure ? 0.05 : 0.0;
	const double wAnalytics = m_hasAnalytics ? 0.10 : 0.0;
	const double wPass = 1.0 - wMeasure - wAnalytics;

	double raw = wMeasure * measure + wPass * pass + wAnalytics * analytics;
	if (raw >= 100.0)
		raw = 99.99;
	if (raw < m_all)
		raw = m_all;
	m_all = raw;
	return m_all;
}

Progress::operator StormByte::Safe::String() const {
	std::lock_guard lock(m_lock);
	if (m_calculatingDuration) {
		if (m_durationCalculation)
			return StormByte::Safe::String(std::format("Calculating duration {} {:6.2f}%", m_durationIndicator, *m_durationCalculation));
		constexpr std::string_view activity = "|/-\\";
		const auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count() / 125;
		m_durationIndicator = activity[static_cast<std::size_t>(tick) % activity.size()];
		return StormByte::Safe::String(std::format("Calculating duration {}", m_durationIndicator));
	}
	std::string line;
	const bool measureLive = m_hasMeasure && !m_measureDone;
	if (const auto v = Measure(); v && measureLive)
		line += std::format("measure {:6.2f}%  ", *v);
	if (!measureLive) {
		if (const auto v = Analytics(); v && !m_analyticsDone)
			line += std::format("analytics {:6.2f}%  ", *v);
	}
	line += std::format("all {:6.2f}%", All());
	return StormByte::Safe::String(line);
}

void Progress::HasMeasure(bool on) noexcept {
	std::lock_guard lock(m_lock);
	m_hasMeasure = on;
}

void Progress::HasAnalytics(bool on) noexcept {
	std::lock_guard lock(m_lock);
	m_hasAnalytics = on;
}

void Progress::SetDurationNs(std::int64_t ns) noexcept {
	std::lock_guard lock(m_lock);
	if (ns > 0)
		m_durationNs = ns;
}

void Progress::SetMeasureNs(std::int64_t ns) noexcept {
	std::lock_guard lock(m_lock);
	if (ns < 0)
		return;
	if (m_durationNs > 0 && ns > m_durationNs)
		ns = m_durationNs;
	if (ns < m_measureNs)
		return;
	m_measureNs = ns;
}

void Progress::SetPassNs(std::int64_t ns) noexcept {
	std::lock_guard lock(m_lock);
	if (ns < 0)
		return;
	if (m_durationNs > 0 && ns > m_durationNs)
		ns = m_durationNs;
	if (ns < m_passNs)
		return;
	m_passNs = ns;
}

void Progress::SetAnalyticsNs(std::int64_t ns) noexcept {
	std::lock_guard lock(m_lock);
	if (ns < 0)
		return;
	if (m_durationNs > 0 && ns > m_durationNs)
		ns = m_durationNs;
	if (ns < m_analyticsNs)
		return;
	m_analyticsNs = ns;
}

void Progress::MeasureDone() noexcept {
	std::lock_guard lock(m_lock);
	m_measureDone = true;
	if (m_durationNs > 0)
		m_measureNs = m_durationNs;
}

void Progress::PassDone() noexcept {
	std::lock_guard lock(m_lock);
	m_passDone = true;
	if (m_durationNs > 0)
		m_passNs = m_durationNs;
}

void Progress::MuxDone() noexcept {
	std::lock_guard lock(m_lock);
	m_muxDone = true;
	m_analyticsAtMux = m_analyticsNs;
}

void Progress::AnalyticsDone() noexcept {
	std::lock_guard lock(m_lock);
	m_analyticsDone = true;
	if (m_durationNs > 0)
		m_analyticsNs = m_durationNs;
}
