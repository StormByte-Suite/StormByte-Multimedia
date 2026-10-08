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

#include <StormByte/multimedia/pipeline/filters/audio/denoise.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <utility>

extern "C" {
	#include <libavutil/samplefmt.h>
}

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Denoise;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;
using FGraph = StormByte::Multimedia::FFmpeg::AVFilterGraph;

namespace {
	constexpr double WinSec = 0.35;
	constexpr std::size_t MaxCand = 64;
	constexpr double MuteDb = -80.0;
	constexpr double LoudDb = -28.0;
	constexpr double MaxCrest = 8.0;
	constexpr double MaxZcr = 0.25;
	constexpr double Dist = 0.18;
	constexpr int MinMatches = 3;
	constexpr double MinFrac = 0.25;

	float SampleAt(const FFrame& src, int ch, int i, int fmt) noexcept {
		const int nch = src.Channels();
		const uint8_t* plane = src.ExtendedData()
			? src.ExtendedData()[0]
			: src.Data(0);
		const uint8_t* p = src.ExtendedData() && src.ExtendedData()[ch]
			? src.ExtendedData()[ch]
			: src.Data(ch);
		if (!plane || !p)
			return 0.f;
		switch (fmt) {
			case AV_SAMPLE_FMT_FLT:
				return reinterpret_cast<const float*>(plane)[i * nch + ch];
			case AV_SAMPLE_FMT_FLTP:
				return reinterpret_cast<const float*>(p)[i];
			case AV_SAMPLE_FMT_DBL:
				return static_cast<float>(reinterpret_cast<const double*>(plane)[i * nch + ch]);
			case AV_SAMPLE_FMT_DBLP:
				return static_cast<float>(reinterpret_cast<const double*>(p)[i]);
			case AV_SAMPLE_FMT_S16:
				return static_cast<float>(reinterpret_cast<const int16_t*>(plane)[i * nch + ch]) / 32768.f;
			case AV_SAMPLE_FMT_S16P:
				return static_cast<float>(reinterpret_cast<const int16_t*>(p)[i]) / 32768.f;
			case AV_SAMPLE_FMT_S32:
				return static_cast<float>(reinterpret_cast<const int32_t*>(plane)[i * nch + ch]) / 2147483648.f;
			case AV_SAMPLE_FMT_S32P:
				return static_cast<float>(reinterpret_cast<const int32_t*>(p)[i]) / 2147483648.f;
			case AV_SAMPLE_FMT_U8:
				return (static_cast<float>(plane[i * nch + ch]) - 128.f) / 128.f;
			case AV_SAMPLE_FMT_U8P:
				return (static_cast<float>(p[i]) - 128.f) / 128.f;
			default:
				return 0.f;
		}
	}
}

Denoise::Denoise(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> nr) noexcept
	: Filter::ProcessTwoPasses(std::move(log), "denoise"),
	m_nrIn(nr),
	m_rate(0),
	m_win(0),
	m_frames(0),
	m_voted(false),
	m_skip(true),
	m_nf(-50.0),
	m_nr(std::clamp(nr.value_or(12.0), 1.0, 30.0)),
	m_matches(0) {}

Denoise::~Denoise() noexcept = default;

enum Type Denoise::Media() const noexcept {
	return Type::Audio;
}

void Denoise::Clean() noexcept {
	m_acc.clear();
	m_cand.clear();
	m_rate = 0;
	m_win = 0;
	m_frames = 0;
	m_voted = false;
	m_skip = true;
	m_nf = -50.0;
	m_matches = 0;
	m_graph.reset();
}

void Denoise::Setup() noexcept {
	m_acc.clear();
	m_graph.reset();
}

void Denoise::Ingest(const FFrame& src) noexcept {
	if (m_rate == 0) {
		m_rate = src.SampleRate();
		m_win = std::max(256, static_cast<int>(std::lround(
			WinSec * static_cast<double>(m_rate))));
	}
	if (src.SampleRate() != m_rate)
		return;
	const int n = src.NbSamples();
	const int ch = src.Channels();
	const int fmt = src.Format();
	m_acc.reserve(m_acc.size() + static_cast<std::size_t>(n));
	for (int i = 0; i < n; ++i) {
		float s = 0.f;
		for (int c = 0; c < ch; ++c)
			s += SampleAt(src, c, i, fmt);
		m_acc.push_back(s / static_cast<float>(std::max(ch, 1)));
	}
	while (m_win > 0 && static_cast<int>(m_acc.size()) >= m_win)
		EmitWindow();
}

void Denoise::EmitWindow() noexcept {
	if (m_win <= 0 || static_cast<int>(m_acc.size()) < m_win)
		return;

	double sum2 = 0.0;
	double peak = 0.0;
	double hp2 = 0.0;
	int zc = 0;
	float prev = m_acc[0];
	for (int i = 0; i < m_win; ++i) {
		const float x = m_acc[static_cast<std::size_t>(i)];
		sum2 += static_cast<double>(x) * static_cast<double>(x);
		peak = std::max(peak, static_cast<double>(std::fabs(x)));
		const float d = x - prev;
		hp2 += static_cast<double>(d) * static_cast<double>(d);
		if ((x >= 0.f) != (prev >= 0.f))
			++zc;
		prev = x;
	}

	const double rms = std::sqrt(sum2 / static_cast<double>(m_win));
	const double rmsDb = (rms > 1e-12) ? 20.0 * std::log10(rms) : -120.0;
	const double crest = (rms > 1e-12) ? peak / rms : 99.0;
	const double zcr = static_cast<double>(zc) / static_cast<double>(m_win);
	const double hp = (sum2 > 1e-18) ? hp2 / sum2 : 0.0;

	const int hop = std::max(1, m_win / 2);
	m_acc.erase(m_acc.begin(), m_acc.begin() + hop);

	if (rmsDb <= MuteDb || rmsDb >= LoudDb)
		return;
	if (crest > MaxCrest || zcr > MaxZcr)
		return;

	Candidate c;
	c.rmsDb = rmsDb;
	c.crest = crest;
	c.zcr = zcr;
	c.hp = hp;
	if (m_cand.size() >= MaxCand)
		m_cand.erase(m_cand.begin());
	m_cand.push_back(c);
}

void Denoise::Vote() noexcept {
	m_voted = true;
	m_skip = true;
	if (m_cand.size() < static_cast<std::size_t>(MinMatches)) {
		Log(Level::Warning, std::format(
			"denoise: not enough room-tone windows ({}); leaving audio untouched",
			m_cand.size()));
		return;
	}

	const auto dist2 = [](const Candidate& a, const Candidate& b) noexcept {
		const double dr = (a.rmsDb - b.rmsDb) / 20.0;
		const double dc = (a.crest - b.crest) / 8.0;
		const double dz = a.zcr - b.zcr;
		const double dh = a.hp - b.hp;
		return dr * dr + dc * dc + dz * dz + dh * dh;
	};

	int bestI = -1;
	int bestN = 0;
	const double thr = Dist * Dist;
	for (std::size_t i = 0; i < m_cand.size(); ++i) {
		int n = 0;
		for (std::size_t j = 0; j < m_cand.size(); ++j) {
			if (i == j)
				continue;
			if (dist2(m_cand[i], m_cand[j]) <= thr)
				++n;
		}
		if (n > bestN) {
			bestN = n;
			bestI = static_cast<int>(i);
		}
	}

	const double frac = static_cast<double>(bestN)
		/ static_cast<double>(m_cand.size());
	if (bestI < 0 || bestN < MinMatches || frac < MinFrac) {
		Log(Level::Warning, std::format(
			"denoise: room tone not stable (matches={} cand={} frac={:.2f}); leaving audio untouched",
			bestN, m_cand.size(), frac));
		return;
	}

	m_skip = false;
	m_matches = bestN;
	m_nf = std::clamp(m_cand[static_cast<std::size_t>(bestI)].rmsDb, -80.0, -20.0);
	m_nr = std::clamp(m_nrIn.value_or(12.0), 1.0, 30.0);
	Log(Level::Notice, std::format(
		"denoise room-tone nf={:.1f} nr={:.1f} matches={} cand={}",
		m_nf, m_nr, m_matches, m_cand.size()));
}

StormByte::Safe::String Denoise::Chain() const noexcept {
	return StormByte::Safe::String(std::format("afftdn=nr={}:nf={}:tn=0", m_nr, m_nf));
}

void Denoise::Measure(const Pipeline::Frame& frame) noexcept {
	if (m_voted)
		return;
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.NbSamples() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "denoise: frame has no audio");
		return;
	}
	Ingest(src);
	++m_frames;
}

void Denoise::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	if (!m_voted)
		Vote();
	if (m_skip)
		return;

	const FFrame& src = AVFrame();
	if (!src || src.NbSamples() <= 0) {
		Log(Level::Warning, "denoise: frame has no audio");
		return;
	}

	const StormByte::Safe::String chain = Chain();
	if (!m_graph) {
		FGraph opened = FGraph::Open(src, chain);
		if (!opened) {
			Fail("denoise: AVFilterGraph::Open failed");
			return;
		}
		m_graph = StormByte::Safe::MakeUnique<FGraph>(std::move(opened));
	} else if (!m_graph->Ensure(src, chain)) {
		Fail("denoise: AVFilterGraph::Ensure failed");
		return;
	}

	FFrame out;
	if (!m_graph->Filter(src, out)) {
		Fail("denoise: AVFilterGraph::Filter failed");
		return;
	}
	if (!out) {
		Log(Level::LowLevel, std::format(
			"denoise wait ch={} pts={}", src.Channels(), src.Pts()));
		return;
	}
	Log(Level::LowLevel, std::format(
		"denoise ch={} samples={} pts={}",
		out.Channels(), out.NbSamples(), out.Pts()));
	Save(std::move(out));
}

void Denoise::Eof() noexcept {
	if (!m_voted) {
		if (m_win > 0 && static_cast<int>(m_acc.size()) >= m_win)
			EmitWindow();
		Vote();
		return;
	}
	if (!m_graph)
		return;
	for (;;) {
		FFrame out;
		if (!m_graph->Flush(out)) {
			Fail("denoise: AVFilterGraph::Flush failed");
			break;
		}
		if (!out)
			break;
		Save(std::move(out));
	}
	m_graph.reset();
}

class StormByte::Multimedia::Pipeline::Filter::Report Denoise::Report() const noexcept {
	if (!m_voted)
		return { Filter::Report::Status::Failed, {} };
	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data;
	data.emplace(StormByte::Safe::String("skip"), m_skip ? "1" : "0");
	data.emplace(StormByte::Safe::String("nf"), std::format("{:.3f}", m_nf));
	data.emplace(StormByte::Safe::String("nr"), std::format("{:.3f}", m_nr));
	data.emplace(StormByte::Safe::String("matches"), std::to_string(m_matches));
	data.emplace(StormByte::Safe::String("candidates"), std::to_string(m_cand.size()));
	data.emplace(StormByte::Safe::String("frames"), std::to_string(m_frames));
	return { Filter::Report::Status::Ok, std::move(data) };
}
