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

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/pipeline/filters/audio/loudnorm.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/type.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>

extern "C" {
	#include <libavutil/samplefmt.h>
}

using StormByte::Logger::Level;
using StormByte::Multimedia::Type;
using StormByte::Multimedia::Pipeline::Filter::Audio::Loudnorm;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

namespace {
	constexpr double DefaultI = -23.0;
	constexpr double DefaultTp = -1.5;

	float SampleAt(const FFrame& src, int ch, int i, int fmt) noexcept {
		const int nch = src.Channels();
		const uint8_t* plane = src.ExtendedData()
			? src.ExtendedData()[0]
			: src.Data(0);
		const uint8_t* p = src.ExtendedData() && src.ExtendedData()[ch]
			? src.ExtendedData()[ch]
			: src.Data(ch);
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

	void StoreAt(FFrame& dst, int ch, int i, int fmt, float v) noexcept {
		const int nch = dst.Channels();
		uint8_t* plane = dst.ExtendedData()
			? dst.ExtendedData()[0]
			: dst.Data(0);
		uint8_t* p = dst.ExtendedData() && dst.ExtendedData()[ch]
			? dst.ExtendedData()[ch]
			: dst.Data(ch);
		const float c = std::clamp(v, -1.f, 1.f);
		switch (fmt) {
			case AV_SAMPLE_FMT_FLT:
				reinterpret_cast<float*>(plane)[i * nch + ch] = c;
				break;
			case AV_SAMPLE_FMT_FLTP:
				reinterpret_cast<float*>(p)[i] = c;
				break;
			case AV_SAMPLE_FMT_DBL:
				reinterpret_cast<double*>(plane)[i * nch + ch] = static_cast<double>(c);
				break;
			case AV_SAMPLE_FMT_DBLP:
				reinterpret_cast<double*>(p)[i] = static_cast<double>(c);
				break;
			case AV_SAMPLE_FMT_S16:
				reinterpret_cast<int16_t*>(plane)[i * nch + ch] =
					static_cast<int16_t>(std::lround(c * 32767.f));
				break;
			case AV_SAMPLE_FMT_S16P:
				reinterpret_cast<int16_t*>(p)[i] =
					static_cast<int16_t>(std::lround(c * 32767.f));
				break;
			case AV_SAMPLE_FMT_S32:
				reinterpret_cast<int32_t*>(plane)[i * nch + ch] =
					static_cast<int32_t>(std::lround(static_cast<double>(c) * 2147483647.0));
				break;
			case AV_SAMPLE_FMT_S32P:
				reinterpret_cast<int32_t*>(p)[i] =
					static_cast<int32_t>(std::lround(static_cast<double>(c) * 2147483647.0));
				break;
			case AV_SAMPLE_FMT_U8:
				plane[i * nch + ch] = static_cast<uint8_t>(std::lround((c + 1.f) * 127.5f));
				break;
			case AV_SAMPLE_FMT_U8P:
				p[i] = static_cast<uint8_t>(std::lround((c + 1.f) * 127.5f));
				break;
			default:
				break;
		}
	}

	double DbTp(double linear) noexcept {
		if (!(linear > 0.0) || !std::isfinite(linear))
			return -std::numeric_limits<double>::infinity();
		return 20.0 * std::log10(linear);
	}
}

Loudnorm::Loudnorm(StormByte::Safe::Shared<StormByte::Logger::Log> log,
	StormByte::Safe::Optional<double> integrated, StormByte::Safe::Optional<double> truePeak) noexcept
	: Filter::ProcessTwoPasses(std::move(log), "loudnorm"),
	m_targetI(integrated.value_or(DefaultI)),
	m_targetTp(truePeak.value_or(DefaultTp)),
	m_st(nullptr), m_channels(0), m_rate(0),
	m_measuredI(0.0), m_measuredLra(0.0), m_gain(1.0),
	m_ceiling(0.0), m_limit(false), m_ready(false), m_frames(0) {}

Loudnorm::~Loudnorm() noexcept {
	Clean();
}

enum Type Loudnorm::Media() const noexcept {
	return Type::Audio;
}

void Loudnorm::Clean() noexcept {
	if (m_st)
		ebur128_destroy(&m_st);
	m_st = nullptr;
	m_channels = 0;
	m_rate = 0;
	m_measuredI = 0.0;
	m_measuredLra = 0.0;
	m_tp.clear();
	m_gain = 1.0;
	m_ceiling = 0.0;
	m_limit = false;
	m_ready = false;
	m_frames = 0;
}

void Loudnorm::Setup() noexcept {
	if (m_st)
		ebur128_destroy(&m_st);
	m_st = nullptr;
	m_channels = 0;
	m_rate = 0;
	m_frames = 0;
}

int Loudnorm::MapChannel(int i, int channels) noexcept {
	if (channels == 1)
		return EBUR128_CENTER;
	if (channels == 2)
		return i == 0 ? EBUR128_LEFT : EBUR128_RIGHT;
	if (channels >= 6) {
		switch (i) {
			case 0: return EBUR128_LEFT;
			case 1: return EBUR128_RIGHT;
			case 2: return EBUR128_CENTER;
			case 3: return EBUR128_UNUSED;
			case 4: return EBUR128_LEFT_SURROUND;
			case 5: return EBUR128_RIGHT_SURROUND;
			default: return EBUR128_UNUSED;
		}
	}
	if (i == 0)
		return EBUR128_LEFT;
	if (i == 1)
		return EBUR128_RIGHT;
	if (i == 2)
		return EBUR128_CENTER;
	return EBUR128_UNUSED;
}

bool Loudnorm::OpenMeter(int channels, int rate) noexcept {
	if (channels <= 0 || rate <= 0)
		return false;
	m_st = ebur128_init(static_cast<unsigned>(channels),
		static_cast<unsigned long>(rate),
		EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK);
	if (!m_st)
		return false;
	m_channels = channels;
	m_rate = rate;
	m_tp = StormByte::Safe::Vector<double>(static_cast<std::size_t>(channels), 0.0);
	for (int i = 0; i < channels; ++i)
		ebur128_set_channel(m_st, static_cast<unsigned>(i), MapChannel(i, channels));
	return true;
}

bool Loudnorm::Add(const FFrame& src) noexcept {
	if (!m_st)
		return false;
	const int n = src.NbSamples();
	const int ch = src.Channels();
	const int fmt = src.Format();
	if (n <= 0 || ch <= 0)
		return false;
	StormByte::Safe::Vector<float> interleaved(static_cast<std::size_t>(n) * static_cast<std::size_t>(ch));
	for (int i = 0; i < n; ++i) {
		for (int c = 0; c < ch; ++c)
			interleaved[static_cast<std::size_t>(i) * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c)]
				= SampleAt(src, c, i, fmt);
	}
	return ebur128_add_frames_float(m_st, interleaved.data(),
		static_cast<std::size_t>(n)) == EBUR128_SUCCESS;
}

void Loudnorm::CloseMeter() noexcept {
	if (!m_st || m_ready)
		return;
	if (ebur128_loudness_global(m_st, &m_measuredI) != EBUR128_SUCCESS
		|| !std::isfinite(m_measuredI)) {
		Log(Level::Warning, "loudnorm: integrated loudness unavailable");
		ebur128_destroy(&m_st);
		m_st = nullptr;
		return;
	}
	if (ebur128_loudness_range(m_st, &m_measuredLra) != EBUR128_SUCCESS)
		m_measuredLra = 0.0;
	for (int c = 0; c < m_channels; ++c) {
		double tp = 0.0;
		if (ebur128_true_peak(m_st, static_cast<unsigned>(c), &tp) == EBUR128_SUCCESS)
			m_tp[static_cast<std::size_t>(c)] = tp;
	}
	ebur128_destroy(&m_st);
	m_st = nullptr;

	const double gainDb = m_targetI - m_measuredI;
	m_gain = std::pow(10.0, gainDb / 20.0);
	m_ceiling = std::pow(10.0, m_targetTp / 20.0);
	double worstTp = -std::numeric_limits<double>::infinity();
	for (double lin : m_tp)
		worstTp = std::max(worstTp, DbTp(lin));
	m_limit = std::isfinite(worstTp) && worstTp + gainDb > m_targetTp;
	m_ready = true;
	Log(Level::Notice, std::format(
		"loudnorm I={:.2f} LRA={:.2f} targetI={:.2f} targetTP={:.2f} gain={:.3f} dB limit={} frames={}",
		m_measuredI, m_measuredLra, m_targetI, m_targetTp, gainDb,
		m_limit ? 1 : 0, m_frames));
}

FFrame Loudnorm::Gain(const FFrame& src) const noexcept {
	FFrame out;
	if (!out.AllocAudio(src.NbSamples(), src.Format(), src.ChannelLayout(), src.SampleRate())
		|| !out.CopyProps(src))
		return {};
	const int n = src.NbSamples();
	const int ch = src.Channels();
	const int fmt = src.Format();
	const float g = static_cast<float>(m_gain);
	const float ceil = static_cast<float>(m_ceiling > 0.0 ? m_ceiling : 1.0);
	for (int i = 0; i < n; ++i) {
		float peak = 0.f;
		for (int c = 0; c < ch; ++c)
			peak = std::max(peak, std::fabs(SampleAt(src, c, i, fmt) * g));
		const float linked = (m_limit && peak > ceil && peak > 0.f)
			? ceil / peak
			: 1.f;
		for (int c = 0; c < ch; ++c)
			StoreAt(out, c, i, fmt, SampleAt(src, c, i, fmt) * g * linked);
	}
	return out;
}

void Loudnorm::Measure(const Pipeline::Frame& frame) noexcept {
	if (m_ready)
		return;
	if (frame.Type() != Type::Audio)
		return;
	const FFrame& src = AVFrame();
	if (!src || src.NbSamples() <= 0 || src.Channels() <= 0) {
		Log(Level::Warning, "loudnorm: frame has no audio");
		return;
	}
	if (!m_st && !OpenMeter(src.Channels(), src.SampleRate())) {
		Fail("loudnorm: ebur128_init failed");
		return;
	}
	if (src.Channels() != m_channels || src.SampleRate() != m_rate) {
		Fail("loudnorm: layout or rate changed mid-stream");
		return;
	}
	if (!Add(src)) {
		if (!m_st)
			return;
		Fail("loudnorm: ebur128_add_frames failed");
		return;
	}
	++m_frames;
}

void Loudnorm::Process(const Pipeline::Frame& frame) noexcept {
	if (frame.Type() != Type::Audio)
		return;
	if (!m_ready) {
		Fail("loudnorm: Process before a finished Measure");
		return;
	}
	const FFrame& src = AVFrame();
	if (!src || src.NbSamples() <= 0) {
		Log(Level::Warning, "loudnorm: frame has no audio");
		return;
	}
	FFrame out = Gain(src);
	if (!out) {
		Fail("loudnorm: AllocAudio failed");
		return;
	}
	Log(Level::LowLevel, std::format("loudnorm process pts={} gain={:.6f}", src.Pts(), m_gain));
	Save(std::move(out));
}

void Loudnorm::Eof() noexcept {
	if (!m_ready)
		CloseMeter();
}

class StormByte::Multimedia::Pipeline::Filter::Report Loudnorm::Report() const noexcept {
	if (!m_ready)
		return { Filter::Report::Status::Failed, {} };
	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> data;
	data.emplace(StormByte::Safe::String("I"), std::format("{:.3f}", m_measuredI));
	data.emplace(StormByte::Safe::String("LRA"), std::format("{:.3f}", m_measuredLra));
	data.emplace(StormByte::Safe::String("I_target"), std::format("{:.3f}", m_targetI));
	data.emplace(StormByte::Safe::String("TP_target"), std::format("{:.3f}", m_targetTp));
	data.emplace(StormByte::Safe::String("gain_db"), std::format("{:.3f}", 20.0 * std::log10(std::max(m_gain, 1e-12))));
	data.emplace(StormByte::Safe::String("limit"), m_limit ? "1" : "0");
	data.emplace(StormByte::Safe::String("frames"), std::to_string(m_frames));
	data.emplace(StormByte::Safe::String("channels"), std::to_string(m_channels));
	for (std::size_t c = 0; c < m_tp.size(); ++c)
		data.emplace(StormByte::Safe::String(std::format("TP_{}", c)), std::format("{:.3f}", DbTp(m_tp[c])));
	return { Filter::Report::Status::Ok, std::move(data) };
}
