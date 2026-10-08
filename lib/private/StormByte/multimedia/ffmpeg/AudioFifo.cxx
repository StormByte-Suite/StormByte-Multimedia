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

#include <StormByte/multimedia/ffmpeg/AudioFifo.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>

extern "C" {
	#include <libavutil/audio_fifo.h>
	#include <libavutil/samplefmt.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AudioFifo::AudioFifo(::AVAudioFifo* fifo) noexcept
: AVPointer(fifo) {}

FFmpeg::AudioFifo::~AudioFifo() noexcept {
	Free();
}

FFmpeg::AudioFifo::operator bool() const noexcept {
	return m_ptr != nullptr;
}

FFmpeg::AudioFifo FFmpeg::AudioFifo::Open(int sample_fmt, int channels, int nb_samples) noexcept {
	if (channels <= 0 || nb_samples <= 0)
		return AudioFifo(nullptr);
	return AudioFifo(av_audio_fifo_alloc(static_cast<AVSampleFormat>(sample_fmt), channels, nb_samples));
}

int FFmpeg::AudioFifo::Size() const noexcept {
	return m_ptr ? av_audio_fifo_size(m_ptr) : 0;
}

int FFmpeg::AudioFifo::Space() const noexcept {
	return m_ptr ? av_audio_fifo_space(m_ptr) : 0;
}

bool FFmpeg::AudioFifo::Write(const AVFrame& src) noexcept {
	if (!m_ptr || src.NbSamples() <= 0)
		return false;
	const auto* planes = src.ExtendedData();
	if (!planes)
		return false;
	uint8_t** writable = const_cast<uint8_t**>(planes);
	return av_audio_fifo_write(m_ptr, reinterpret_cast<void**>(writable),
		src.NbSamples()) >= src.NbSamples();
}

bool FFmpeg::AudioFifo::Read(AVFrame& dst, int nb_samples) noexcept {
	if (!m_ptr || nb_samples <= 0)
		return false;
	uint8_t** planes = dst.ExtendedData();
	if (!planes)
		return false;
	return av_audio_fifo_read(m_ptr, reinterpret_cast<void**>(planes), nb_samples) >= nb_samples;
}

bool FFmpeg::AudioFifo::Realloc(int nb_samples) noexcept {
	return m_ptr && nb_samples >= 0 && av_audio_fifo_realloc(m_ptr, nb_samples) >= 0;
}

void FFmpeg::AudioFifo::Free() noexcept {
	if (m_ptr) {
		av_audio_fifo_free(m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVAudioFifo>;
