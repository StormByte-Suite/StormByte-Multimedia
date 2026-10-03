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

#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>

#include <cstring>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavutil/avutil.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVPacket::AVPacket() noexcept:
AVPointer(av_packet_alloc()) {}

FFmpeg::AVPacket::AVPacket(const AVPacket& other) noexcept:
AVPointer(av_packet_alloc()) {
	if (m_ptr && other.m_ptr)
		av_packet_ref(m_ptr, other.m_ptr);
}

FFmpeg::AVPacket::~AVPacket() noexcept {
	Free();
}

FFmpeg::AVPacket& FFmpeg::AVPacket::operator=(const AVPacket& other) noexcept {
	if (this == &other)
		return *this;
	Free();
	m_ptr = av_packet_alloc();
	if (m_ptr && other.m_ptr)
		av_packet_ref(m_ptr, other.m_ptr);
	return *this;
}

FFmpeg::AVPacket FFmpeg::AVPacket::Ref() const noexcept {
	return AVPacket(*this);
}

void FFmpeg::AVPacket::Unref() noexcept {
	if (m_ptr)
		av_packet_unref(m_ptr);
}

bool FFmpeg::AVPacket::Load(const std::uint8_t* data, int size, int stream_index, bool key_frame) noexcept {
	Unref();
	if (!m_ptr)
		return false;
	if (size > 0) {
		if (av_new_packet(m_ptr, size) < 0)
			return false;
		if (data)
			std::memcpy(m_ptr->data, data, static_cast<std::size_t>(size));
	}

	m_ptr->stream_index = stream_index;
	m_ptr->flags = key_frame ? AV_PKT_FLAG_KEY : 0;
	m_ptr->pts = AV_NOPTS_VALUE;
	m_ptr->dts = AV_NOPTS_VALUE;
	m_ptr->duration = 0;
	return true;
}

void FFmpeg::AVPacket::Timestamps(std::int64_t pts, std::int64_t dts, std::int64_t duration) noexcept {
	if (!m_ptr)
		return;
	m_ptr->pts = pts;
	m_ptr->dts = dts;
	m_ptr->duration = duration;
}

int FFmpeg::AVPacket::StreamIndex() const noexcept {
	return m_ptr ? m_ptr->stream_index : -1;
}

std::int64_t FFmpeg::AVPacket::Pts() const noexcept {
	return m_ptr ? m_ptr->pts : AV_NOPTS_VALUE;
}

std::int64_t FFmpeg::AVPacket::Dts() const noexcept {
	return m_ptr ? m_ptr->dts : AV_NOPTS_VALUE;
}

std::int64_t FFmpeg::AVPacket::Duration() const noexcept {
	return m_ptr ? m_ptr->duration : 0;
}

int FFmpeg::AVPacket::Flags() const noexcept {
	return m_ptr ? m_ptr->flags : 0;
}

const std::uint8_t* FFmpeg::AVPacket::Data() const noexcept {
	return m_ptr ? m_ptr->data : nullptr;
}

int FFmpeg::AVPacket::Size() const noexcept {
	return m_ptr ? m_ptr->size : 0;
}

std::int64_t FFmpeg::AVPacket::Position() const noexcept {
	return m_ptr ? m_ptr->pos : -1;
}

FFmpeg::AVPacket::operator bool() const noexcept {
	return m_ptr != nullptr;
}

int FFmpeg::AVPacket::SideDataCount() const noexcept {
	return m_ptr ? m_ptr->side_data_elems : 0;
}

int FFmpeg::AVPacket::SideDataType(int index) const noexcept {
	if (!m_ptr || index < 0 || index >= m_ptr->side_data_elems)
		return -1;
	return static_cast<int>(m_ptr->side_data[index].type);
}

const std::uint8_t* FFmpeg::AVPacket::SideData(int index, int& size) const noexcept {
	size = 0;
	if (!m_ptr || index < 0 || index >= m_ptr->side_data_elems)
		return nullptr;
	size = static_cast<int>(m_ptr->side_data[index].size);
	return m_ptr->side_data[index].data;
}

::AVPacket* FFmpeg::AVPacket::Detach() noexcept {
	::AVPacket* raw = m_ptr;
	m_ptr = nullptr;
	return raw;
}

void FFmpeg::AVPacket::Reset(::AVPacket* raw) noexcept {
	Free();
	m_ptr = raw;
}

void FFmpeg::AVPacket::Free() noexcept {
	if (m_ptr) {
		av_packet_free(&m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVPacket>;
