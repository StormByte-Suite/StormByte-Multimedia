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

#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>

#include <cstring>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavutil/mastering_display_metadata.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVCodecParameters::AVCodecParameters(::AVCodecParameters* par) noexcept
:AVPointer(avcodec_parameters_alloc()) {
	if (par)
		avcodec_parameters_copy(m_ptr, par);
}

FFmpeg::AVCodecParameters::AVCodecParameters(const AVCodecParameters& other) noexcept
:AVPointer(avcodec_parameters_alloc()) {
	if (other.m_ptr)
		avcodec_parameters_copy(m_ptr, other.m_ptr);
}

FFmpeg::AVCodecParameters::~AVCodecParameters() noexcept {
	Free();
}

FFmpeg::AVCodecParameters& FFmpeg::AVCodecParameters::operator=(const AVCodecParameters& other) noexcept {
	if (this != &other) {
		Free();
		m_ptr = avcodec_parameters_alloc();
		avcodec_parameters_copy(m_ptr, other.m_ptr);
	}

	return *this;
}

int FFmpeg::AVCodecParameters::CodecId() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->codec_id) : static_cast<int>(AV_CODEC_ID_NONE);
}

void FFmpeg::AVCodecParameters::CodecId(int id) noexcept {
	if (m_ptr)
		m_ptr->codec_id = static_cast<::AVCodecID>(id);
}

std::int64_t FFmpeg::AVCodecParameters::BitRate() const noexcept {
	return m_ptr ? m_ptr->bit_rate : 0;
}

void FFmpeg::AVCodecParameters::BitRate(std::int64_t bit_rate) noexcept {
	if (m_ptr)
		m_ptr->bit_rate = bit_rate;
}

int FFmpeg::AVCodecParameters::Width() const noexcept {
	return m_ptr ? m_ptr->width : 0;
}

void FFmpeg::AVCodecParameters::Width(int width) noexcept {
	if (m_ptr)
		m_ptr->width = width;
}

int FFmpeg::AVCodecParameters::Height() const noexcept {
	return m_ptr ? m_ptr->height : 0;
}

void FFmpeg::AVCodecParameters::Height(int height) noexcept {
	if (m_ptr)
		m_ptr->height = height;
}

FFmpeg::AVRational FFmpeg::AVCodecParameters::SampleAspectRatio() const noexcept {
	return m_ptr ? FFmpeg::FromRaw(m_ptr->sample_aspect_ratio) : FFmpeg::AVRational{0, 1};
}

void FFmpeg::AVCodecParameters::SampleAspectRatio(FFmpeg::AVRational sar) noexcept {
	if (m_ptr)
		m_ptr->sample_aspect_ratio = FFmpeg::ToRaw(sar);
}

int FFmpeg::AVCodecParameters::Format() const noexcept {
	return m_ptr ? m_ptr->format : static_cast<int>(AV_PIX_FMT_NONE);
}

void FFmpeg::AVCodecParameters::Format(int format) noexcept {
	if (m_ptr)
		m_ptr->format = format;
}

int FFmpeg::AVCodecParameters::ColorRange() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_range) : static_cast<int>(AVCOL_RANGE_UNSPECIFIED);
}

void FFmpeg::AVCodecParameters::ColorRange(int range) noexcept {
	if (m_ptr)
		m_ptr->color_range = static_cast<::AVColorRange>(range);
}

int FFmpeg::AVCodecParameters::ColorSpace() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_space) : static_cast<int>(AVCOL_SPC_UNSPECIFIED);
}

void FFmpeg::AVCodecParameters::ColorSpace(int space) noexcept {
	if (m_ptr)
		m_ptr->color_space = static_cast<::AVColorSpace>(space);
}

int FFmpeg::AVCodecParameters::ColorPrimaries() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_primaries) : static_cast<int>(AVCOL_PRI_UNSPECIFIED);
}

void FFmpeg::AVCodecParameters::ColorPrimaries(int primaries) noexcept {
	if (m_ptr)
		m_ptr->color_primaries = static_cast<::AVColorPrimaries>(primaries);
}

int FFmpeg::AVCodecParameters::ColorTransfer() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_trc) : static_cast<int>(AVCOL_TRC_UNSPECIFIED);
}

void FFmpeg::AVCodecParameters::ColorTransfer(int transfer) noexcept {
	if (m_ptr)
		m_ptr->color_trc = static_cast<::AVColorTransferCharacteristic>(transfer);
}

int FFmpeg::AVCodecParameters::SampleRate() const noexcept {
	return m_ptr ? m_ptr->sample_rate : 0;
}

void FFmpeg::AVCodecParameters::SampleRate(int rate) noexcept {
	if (m_ptr)
		m_ptr->sample_rate = rate;
}

int FFmpeg::AVCodecParameters::Channels() const noexcept {
	return m_ptr ? m_ptr->ch_layout.nb_channels : 0;
}

int FFmpeg::AVCodecParameters::Profile() const noexcept {
	return m_ptr ? m_ptr->profile : AV_PROFILE_UNKNOWN;
}

void FFmpeg::AVCodecParameters::Profile(int profile) noexcept {
	if (m_ptr)
		m_ptr->profile = profile;
}

FFmpeg::AVChannelLayout FFmpeg::AVCodecParameters::ChannelLayout() const noexcept {
	return m_ptr ? FFmpeg::FromRaw(m_ptr->ch_layout) : FFmpeg::AVChannelLayout{};
}

void FFmpeg::AVCodecParameters::DefaultChannelLayout(int channels) noexcept {
	if (!m_ptr || channels <= 0)
		return;
	av_channel_layout_uninit(&m_ptr->ch_layout);
	av_channel_layout_default(&m_ptr->ch_layout, channels);
}

int FFmpeg::AVCodecParameters::CodecType() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->codec_type) : static_cast<int>(AVMEDIA_TYPE_UNKNOWN);
}

void FFmpeg::AVCodecParameters::CodecType(int type) noexcept {
	if (m_ptr)
		m_ptr->codec_type = static_cast<::AVMediaType>(type);
}

FFmpeg::AVCodecParameters::operator bool() const noexcept {
	return m_ptr != nullptr;
}

bool FFmpeg::AVCodecParameters::CopyChannelLayout(const FFmpeg::AVChannelLayout& layout) noexcept {
	const auto* raw = FFmpeg::ToRaw(layout);
	return m_ptr && raw && av_channel_layout_copy(&m_ptr->ch_layout, raw) >= 0;
}

bool FFmpeg::AVCodecParameters::Export(::AVCodecParameters* dest) const noexcept {
	if (!m_ptr || !dest)
		return false;
	return avcodec_parameters_copy(dest, m_ptr) >= 0;
}

void FFmpeg::AVCodecParameters::WriteHdr10(const StormByte::Multimedia::Property::HDR10& hdr10) noexcept {
	if (!m_ptr)
		return;
	const bool hasMastering =
		hdr10.Red().X() != 0 || hdr10.Red().Y() != 0 ||
		hdr10.Green().X() != 0 || hdr10.Green().Y() != 0 ||
		hdr10.Blue().X() != 0 || hdr10.Blue().Y() != 0 ||
		hdr10.White().X() != 0 || hdr10.White().Y() != 0 ||
		hdr10.Luminance().X() != 0 || hdr10.Luminance().Y() != 0;
	const auto& light = hdr10.LightLevel();
	const bool hasLight = light.has_value() && (light->X() != 0 || light->Y() != 0);
	if (hasMastering) {
		AVMasteringDisplayMetadata mdm{};
		mdm.display_primaries[0][0] = av_make_q(static_cast<int>(hdr10.Red().X()), 50000);
		mdm.display_primaries[0][1] = av_make_q(static_cast<int>(hdr10.Red().Y()), 50000);
		mdm.display_primaries[1][0] = av_make_q(static_cast<int>(hdr10.Green().X()), 50000);
		mdm.display_primaries[1][1] = av_make_q(static_cast<int>(hdr10.Green().Y()), 50000);
		mdm.display_primaries[2][0] = av_make_q(static_cast<int>(hdr10.Blue().X()), 50000);
		mdm.display_primaries[2][1] = av_make_q(static_cast<int>(hdr10.Blue().Y()), 50000);
		mdm.white_point[0] = av_make_q(static_cast<int>(hdr10.White().X()), 50000);
		mdm.white_point[1] = av_make_q(static_cast<int>(hdr10.White().Y()), 50000);
		mdm.min_luminance = av_make_q(static_cast<int>(hdr10.Luminance().X()), 10000);
		mdm.max_luminance = av_make_q(static_cast<int>(hdr10.Luminance().Y()), 10000);
		mdm.has_primaries = 1;
		mdm.has_luminance = 1;
		if (AVPacketSideData* sd = av_packet_side_data_new(&m_ptr->coded_side_data, &m_ptr->nb_coded_side_data,
				AV_PKT_DATA_MASTERING_DISPLAY_METADATA, sizeof(mdm), 0))
			std::memcpy(sd->data, &mdm, sizeof(mdm));
	}

	if (hasLight) {
		AVContentLightMetadata cll{};
		cll.MaxCLL = static_cast<unsigned>(light->X());
		cll.MaxFALL = static_cast<unsigned>(light->Y());
		if (AVPacketSideData* sd = av_packet_side_data_new(&m_ptr->coded_side_data, &m_ptr->nb_coded_side_data,
				AV_PKT_DATA_CONTENT_LIGHT_LEVEL, sizeof(cll), 0))
			std::memcpy(sd->data, &cll, sizeof(cll));
	}
}

void FFmpeg::AVCodecParameters::Free() noexcept {
	if (m_ptr) {
		avcodec_parameters_free(&m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVCodecParameters>;
