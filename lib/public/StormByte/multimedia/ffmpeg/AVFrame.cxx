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

#include <StormByte/multimedia/backend/zimg/zimg.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/Sws.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/avutil.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/frame.h>
	#include <libavutil/hdr_dynamic_metadata.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/pixdesc.h>
	#include <libavutil/pixfmt.h>
	#include <libavutil/samplefmt.h>
	#include <libswscale/swscale.h>
}

using namespace StormByte::Multimedia;
using StormByte::Multimedia::Pipeline::SideDataKind;

namespace {
	constexpr int ChromaDenominator = 50000;
	constexpr int LumaDenominator = 10000;

	AVCodecID ImageCodecFromHint(std::string_view hint) noexcept {
		std::string ext;
		const auto slash = hint.find_last_of("/\\");
		const auto base = slash == std::string_view::npos ? hint : hint.substr(slash + 1);
		const auto dot = base.find_last_of('.');
		ext = std::string(dot == std::string_view::npos ? base : base.substr(dot));
		for (char& c : ext)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (ext == ".png" || ext == "png")
			return AV_CODEC_ID_PNG;
		if (ext == ".jpg" || ext == ".jpeg" || ext == "jpg" || ext == "jpeg")
			return AV_CODEC_ID_MJPEG;
		if (ext == ".webp" || ext == "webp")
			return AV_CODEC_ID_WEBP;
		if (ext == ".bmp" || ext == "bmp")
			return AV_CODEC_ID_BMP;
		return AV_CODEC_ID_MJPEG;
	}

	int ToSwsFlags(FFmpeg::AVFrame::Resample filter) noexcept {
		switch (filter) {
			case FFmpeg::AVFrame::Resample::Point:    return SWS_POINT;
			case FFmpeg::AVFrame::Resample::Bilinear: return SWS_BILINEAR;
			case FFmpeg::AVFrame::Resample::Bicubic:  return SWS_BICUBIC;
			case FFmpeg::AVFrame::Resample::Spline:   return SWS_SPLINE;
			case FFmpeg::AVFrame::Resample::Lanczos:  return SWS_LANCZOS;
			case FFmpeg::AVFrame::Resample::Default:  return SWS_BILINEAR;
		}
		return SWS_BILINEAR;
	}
}

FFmpeg::AVFrame::AVFrame() noexcept:
AVPointer(av_frame_alloc()) {}

FFmpeg::AVFrame::AVFrame(const AVFrame& other) noexcept:
AVPointer(av_frame_alloc()) {
	if (m_ptr && other.m_ptr)
		av_frame_ref(m_ptr, other.m_ptr);
}

FFmpeg::AVFrame::~AVFrame() noexcept {
	Free();
}

FFmpeg::AVFrame& FFmpeg::AVFrame::operator=(const AVFrame& other) noexcept {
	if (this == &other)
		return *this;
	Free();
	m_ptr = av_frame_alloc();
	if (m_ptr && other.m_ptr)
		av_frame_ref(m_ptr, other.m_ptr);
	return *this;
}

void FFmpeg::AVFrame::Unref() noexcept {
	av_frame_unref(m_ptr);
}

const AVFrameSideData* FFmpeg::AVFrame::SideData(int type) const noexcept {
	if (!m_ptr)
		return nullptr;
	return av_frame_get_side_data(m_ptr, static_cast<AVFrameSideDataType>(type));
}

std::int64_t FFmpeg::AVFrame::Pts() const noexcept {
	return m_ptr ? m_ptr->pts : AV_NOPTS_VALUE;
}

std::int64_t FFmpeg::AVFrame::DurationTicks() const noexcept {
	return m_ptr ? m_ptr->duration : 0;
}

void FFmpeg::AVFrame::CopyPrimaryBuffer(StormByte::Safe::Binary& out) const noexcept {
	out.clear();
	if (!m_ptr)
		return;

	if (m_ptr->width > 0 && m_ptr->height > 0 && m_ptr->data[0]) {
		const auto format = static_cast<AVPixelFormat>(m_ptr->format);
		const int size = av_image_get_buffer_size(format, m_ptr->width, m_ptr->height, 1);
		if (size <= 0)
			return;
		out.resize(StormByte::ByteSize{static_cast<std::uint64_t>(size)});
		if (av_image_copy_to_buffer(
			reinterpret_cast<std::uint8_t*>(out.data()), size,
			m_ptr->data, m_ptr->linesize,
			format, m_ptr->width, m_ptr->height, 1) < 0)
			out.clear();
		return;
	}

	if (m_ptr->nb_samples > 0 && m_ptr->data[0]) {
		const auto format = static_cast<AVSampleFormat>(m_ptr->format);
		const int bytes = av_samples_get_buffer_size(nullptr, m_ptr->ch_layout.nb_channels,
			m_ptr->nb_samples, format, 1);
		if (bytes <= 0)
			return;
		out.resize(StormByte::ByteSize{static_cast<std::uint64_t>(bytes)});
		auto* dst = reinterpret_cast<std::uint8_t*>(out.data());
		if (av_samples_copy(&dst, m_ptr->extended_data, 0, 0,
			m_ptr->nb_samples, m_ptr->ch_layout.nb_channels, format) < 0)
			out.clear();
	}
}

void FFmpeg::AVFrame::WriteHdr10(const StormByte::Multimedia::Property::HDR10& hdr10) noexcept {
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
		av_frame_remove_side_data(m_ptr, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
		auto* mdm = av_mastering_display_metadata_create_side_data(m_ptr);
		if (mdm) {
			mdm->has_primaries = 1;
			mdm->display_primaries[0][0] = ::AVRational{hdr10.Red().X(), ChromaDenominator};
			mdm->display_primaries[0][1] = ::AVRational{hdr10.Red().Y(), ChromaDenominator};
			mdm->display_primaries[1][0] = ::AVRational{hdr10.Green().X(), ChromaDenominator};
			mdm->display_primaries[1][1] = ::AVRational{hdr10.Green().Y(), ChromaDenominator};
			mdm->display_primaries[2][0] = ::AVRational{hdr10.Blue().X(), ChromaDenominator};
			mdm->display_primaries[2][1] = ::AVRational{hdr10.Blue().Y(), ChromaDenominator};
			mdm->white_point[0] = ::AVRational{hdr10.White().X(), ChromaDenominator};
			mdm->white_point[1] = ::AVRational{hdr10.White().Y(), ChromaDenominator};
			mdm->has_luminance = 1;
			mdm->min_luminance = ::AVRational{hdr10.Luminance().X(), LumaDenominator};
			mdm->max_luminance = ::AVRational{hdr10.Luminance().Y(), LumaDenominator};
		}
	}

	if (hasLight) {
		av_frame_remove_side_data(m_ptr, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
		auto* cll = av_content_light_metadata_create_side_data(m_ptr);
		if (cll) {
			cll->MaxCLL = static_cast<unsigned>(light->X());
			cll->MaxFALL = static_cast<unsigned>(light->Y());
		}
	}
}

void FFmpeg::AVFrame::WriteSideData(
	const StormByte::Safe::Vector<StormByte::Multimedia::Pipeline::SideData>& attachments) noexcept {
	if (!m_ptr)
		return;

	for (const auto& item : attachments) {
		const auto size = item.Payload().Available();
		if (size == 0)
			continue;
		StormByte::Safe::Binary bytes;
		if (!item.Payload().Peek(size, bytes) || bytes.empty())
			continue;

		if (item.Kind() == SideDataKind::MasteringDisplay
			|| item.Kind() == SideDataKind::ContentLight)
			continue;

		if (item.Kind() == SideDataKind::HdrPlus) {
			av_frame_remove_side_data(m_ptr, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
			AVDynamicHDRPlus* plus = av_dynamic_hdr_plus_create_side_data(m_ptr);
			if (!plus)
				continue;
			const std::size_t copy = std::min(static_cast<std::size_t>(bytes.size()), sizeof(AVDynamicHDRPlus));
			std::memcpy(plus, bytes.data(), copy);
			continue;
		}

		AVFrameSideDataType type = AV_FRAME_DATA_SEI_UNREGISTERED;
		switch (item.Kind()) {
			case SideDataKind::A53CC:
				type = AV_FRAME_DATA_A53_CC;
				break;
			default:
				type = AV_FRAME_DATA_SEI_UNREGISTERED;
				break;
		}

		AVFrameSideData* side = av_frame_new_side_data(m_ptr, type, static_cast<int>(bytes.size()));
		if (!side)
			continue;
		std::memcpy(side->data, bytes.data(), bytes.size());
	}
}

void FFmpeg::AVFrame::Reset(::AVFrame* raw) noexcept {
	Free();
	m_ptr = raw;
}

FFmpeg::AVFrame::operator bool() const noexcept {
	return m_ptr != nullptr;
}

FFmpeg::AVFrame FFmpeg::AVFrame::Clone() const noexcept {
	return AVFrame(*this);
}

bool FFmpeg::AVFrame::Ref(const AVFrame& other) noexcept {
	if (!m_ptr || !other.m_ptr)
		return false;
	Unref();
	return av_frame_ref(m_ptr, other.m_ptr) >= 0;
}

bool FFmpeg::AVFrame::Copy(const AVFrame& other) noexcept {
	if (!m_ptr || !other.m_ptr)
		return false;
	return av_frame_copy(m_ptr, other.m_ptr) >= 0;
}

bool FFmpeg::AVFrame::CopyProps(const AVFrame& other) noexcept {
	if (!m_ptr || !other.m_ptr)
		return false;
	return av_frame_copy_props(m_ptr, other.m_ptr) >= 0;
}

bool FFmpeg::AVFrame::GetBuffer(int align) noexcept {
	return m_ptr && av_frame_get_buffer(m_ptr, align) >= 0;
}

bool FFmpeg::AVFrame::AllocVideo(int width, int height, int format, int align) noexcept {
	if (!m_ptr || width <= 0 || height <= 0)
		return false;
	Unref();
	m_ptr->width = width;
	m_ptr->height = height;
	m_ptr->format = format;
	return GetBuffer(align);
}

bool FFmpeg::AVFrame::AllocAudio(int nb_samples, int format, const FFmpeg::AVChannelLayout& layout, int sample_rate) noexcept {
	if (!m_ptr || nb_samples <= 0)
		return false;
	Unref();
	m_ptr->nb_samples = nb_samples;
	m_ptr->format = format;
	m_ptr->sample_rate = sample_rate;
	const auto* raw = FFmpeg::ToRaw(layout);
	if (!raw || av_channel_layout_copy(&m_ptr->ch_layout, raw) < 0)
		return false;
	return GetBuffer(0);
}

int FFmpeg::AVFrame::Width() const noexcept {
	return m_ptr ? m_ptr->width : 0;
}

void FFmpeg::AVFrame::Width(int width) noexcept {
	if (m_ptr)
		m_ptr->width = width;
}

int FFmpeg::AVFrame::Height() const noexcept {
	return m_ptr ? m_ptr->height : 0;
}

void FFmpeg::AVFrame::Height(int height) noexcept {
	if (m_ptr)
		m_ptr->height = height;
}

int FFmpeg::AVFrame::Format() const noexcept {
	return m_ptr ? m_ptr->format : AV_PIX_FMT_NONE;
}

void FFmpeg::AVFrame::Format(int format) noexcept {
	if (m_ptr)
		m_ptr->format = format;
}

bool FFmpeg::AVFrame::Hardware() const noexcept {
	if (!m_ptr)
		return false;
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(m_ptr->format));
	return !desc || (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0;
}

int FFmpeg::AVFrame::FormatNone() noexcept {
	return static_cast<int>(AV_PIX_FMT_NONE);
}

int FFmpeg::AVFrame::FormatGray8() noexcept {
	return static_cast<int>(AV_PIX_FMT_GRAY8);
}

int FFmpeg::AVFrame::FormatRgba() noexcept {
	return static_cast<int>(AV_PIX_FMT_RGBA);
}

int FFmpeg::AVFrame::FormatYUV420P() noexcept {
	return static_cast<int>(AV_PIX_FMT_YUV420P);
}

FFmpeg::AVFrame::VideoLayout FFmpeg::AVFrame::Layout() const noexcept {
	if (!m_ptr)
		return VideoLayout::Unknown;
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(m_ptr->format));
	if (!desc || (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0)
		return VideoLayout::Unknown;
	if (desc->nb_components <= 1)
		return VideoLayout::Gray;
	if (desc->log2_chroma_w == 1 && desc->log2_chroma_h == 1)
		return VideoLayout::Yuv420;
	if (desc->log2_chroma_w == 1 && desc->log2_chroma_h == 0)
		return VideoLayout::Yuv422;
	if (desc->log2_chroma_w == 0 && desc->log2_chroma_h == 0)
		return VideoLayout::Yuv444;
	return VideoLayout::Unknown;
}

FFmpeg::AVFrame FFmpeg::AVFrame::DecodeImage(const std::uint8_t* data, std::size_t size,
	std::string_view hint) noexcept {
	AVFrame out;
	out.Reset(nullptr);
	if (!data || size == 0 || size > static_cast<std::size_t>(INT_MAX))
		return out;

	const AVCodec* codec = avcodec_find_decoder(ImageCodecFromHint(hint));
	if (!codec)
		return out;

	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	if (!ctx || avcodec_open2(ctx, codec, nullptr) < 0) {
		avcodec_free_context(&ctx);
		return out;
	}

	AVPacket* pkt = av_packet_alloc();
	if (!pkt) {
		avcodec_free_context(&ctx);
		return out;
	}

	pkt->data = const_cast<std::uint8_t*>(data);
	pkt->size = static_cast<int>(size);

	AVFrame decoded;
	const bool ok = avcodec_send_packet(ctx, pkt) >= 0
		&& avcodec_receive_frame(ctx, decoded.Get()) >= 0
		&& decoded.Width() > 0 && decoded.Height() > 0;

	pkt->data = nullptr;
	pkt->size = 0;
	av_packet_free(&pkt);
	avcodec_free_context(&ctx);

	if (!ok)
		return out;
	return decoded;
}

void FFmpeg::AVFrame::Pts(std::int64_t pts) noexcept {
	if (m_ptr)
		m_ptr->pts = pts;
}

std::int64_t FFmpeg::AVFrame::BestEffortTimestamp() const noexcept {
	return m_ptr ? m_ptr->best_effort_timestamp : AV_NOPTS_VALUE;
}

void FFmpeg::AVFrame::BestEffortTimestamp(std::int64_t ts) noexcept {
	if (m_ptr)
		m_ptr->best_effort_timestamp = ts;
}

void FFmpeg::AVFrame::DurationTicks(std::int64_t duration) noexcept {
	if (m_ptr)
		m_ptr->duration = duration;
}

int FFmpeg::AVFrame::ColorRange() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_range) : 0;
}

void FFmpeg::AVFrame::ColorRange(int range) noexcept {
	if (m_ptr)
		m_ptr->color_range = static_cast<AVColorRange>(range);
}

int FFmpeg::AVFrame::ColorSpace() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->colorspace) : 0;
}

void FFmpeg::AVFrame::ColorSpace(int space) noexcept {
	if (m_ptr)
		m_ptr->colorspace = static_cast<AVColorSpace>(space);
}

int FFmpeg::AVFrame::ColorPrimaries() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_primaries) : 0;
}

void FFmpeg::AVFrame::ColorPrimaries(int primaries) noexcept {
	if (m_ptr)
		m_ptr->color_primaries = static_cast<AVColorPrimaries>(primaries);
}

int FFmpeg::AVFrame::ColorTransfer() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->color_trc) : 0;
}

void FFmpeg::AVFrame::ColorTransfer(int transfer) noexcept {
	if (m_ptr)
		m_ptr->color_trc = static_cast<AVColorTransferCharacteristic>(transfer);
}

int FFmpeg::AVFrame::ChromaLocation() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->chroma_location) : 0;
}

void FFmpeg::AVFrame::ChromaLocation(int location) noexcept {
	if (m_ptr)
		m_ptr->chroma_location = static_cast<AVChromaLocation>(location);
}

FFmpeg::AVRational FFmpeg::AVFrame::SampleAspectRatio() const noexcept {
	return m_ptr ? FFmpeg::FromRaw(m_ptr->sample_aspect_ratio) : FFmpeg::AVRational{0, 1};
}

void FFmpeg::AVFrame::SampleAspectRatio(FFmpeg::AVRational sar) noexcept {
	if (m_ptr)
		m_ptr->sample_aspect_ratio = FFmpeg::ToRaw(sar);
}

int FFmpeg::AVFrame::PictType() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->pict_type) : 0;
}

void FFmpeg::AVFrame::PictType(int type) noexcept {
	if (m_ptr)
		m_ptr->pict_type = static_cast<AVPictureType>(type);
}

bool FFmpeg::AVFrame::KeyFrame() const noexcept {
	if (!m_ptr)
		return false;
#if defined(AV_FRAME_FLAG_KEY)
	return (m_ptr->flags & AV_FRAME_FLAG_KEY) != 0;
#else
	return m_ptr->key_frame != 0;
#endif
}

void FFmpeg::AVFrame::KeyFrame(bool key) noexcept {
	if (!m_ptr)
		return;
#if defined(AV_FRAME_FLAG_KEY)
	if (key)
		m_ptr->flags |= AV_FRAME_FLAG_KEY;
	else
		m_ptr->flags &= ~AV_FRAME_FLAG_KEY;
#else
	m_ptr->key_frame = key ? 1 : 0;
#endif
}

int FFmpeg::AVFrame::RepeatPict() const noexcept {
	return m_ptr ? m_ptr->repeat_pict : 0;
}

void FFmpeg::AVFrame::RepeatPict(int repeat) noexcept {
	if (m_ptr)
		m_ptr->repeat_pict = repeat;
}

bool FFmpeg::AVFrame::Interlaced() const noexcept {
	if (!m_ptr)
		return false;
#if defined(AV_FRAME_FLAG_INTERLACED)
	return (m_ptr->flags & AV_FRAME_FLAG_INTERLACED) != 0;
#else
	return m_ptr->interlaced_frame != 0;
#endif
}

void FFmpeg::AVFrame::Interlaced(bool interlaced, bool top_first) noexcept {
	if (!m_ptr)
		return;
#if defined(AV_FRAME_FLAG_INTERLACED)
	if (interlaced)
		m_ptr->flags |= AV_FRAME_FLAG_INTERLACED;
	else
		m_ptr->flags &= ~AV_FRAME_FLAG_INTERLACED;
#if defined(AV_FRAME_FLAG_TOP_FIELD_FIRST)
	if (top_first)
		m_ptr->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
	else
		m_ptr->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
#endif
#else
	m_ptr->interlaced_frame = interlaced ? 1 : 0;
	m_ptr->top_field_first = top_first ? 1 : 0;
#endif
}

bool FFmpeg::AVFrame::TopFieldFirst() const noexcept {
	if (!m_ptr)
		return false;
#if defined(AV_FRAME_FLAG_TOP_FIELD_FIRST)
	return (m_ptr->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
#else
	return m_ptr->top_field_first != 0;
#endif
}

int FFmpeg::AVFrame::CropLeft() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->crop_left) : 0;
}

int FFmpeg::AVFrame::CropRight() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->crop_right) : 0;
}

int FFmpeg::AVFrame::CropTop() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->crop_top) : 0;
}

int FFmpeg::AVFrame::CropBottom() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->crop_bottom) : 0;
}

void FFmpeg::AVFrame::Crop(int left, int right, int top, int bottom) noexcept {
	if (!m_ptr)
		return;
	m_ptr->crop_left = static_cast<std::size_t>(left < 0 ? 0 : left);
	m_ptr->crop_right = static_cast<std::size_t>(right < 0 ? 0 : right);
	m_ptr->crop_top = static_cast<std::size_t>(top < 0 ? 0 : top);
	m_ptr->crop_bottom = static_cast<std::size_t>(bottom < 0 ? 0 : bottom);
}

bool FFmpeg::AVFrame::ApplyCropping(int flags) noexcept {
	return m_ptr && av_frame_apply_cropping(m_ptr, flags) >= 0;
}

int FFmpeg::AVFrame::PlaneCount() const noexcept {
	if (!m_ptr)
		return 0;
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(m_ptr->format));
	if (desc)
		return desc->nb_components;
	if (m_ptr->nb_samples > 0)
		return m_ptr->ch_layout.nb_channels;
	return 0;
}

int FFmpeg::AVFrame::PlaneWidth(int plane) const noexcept {
	if (!m_ptr || m_ptr->width <= 0)
		return 0;
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(m_ptr->format));
	if (!desc || (plane != 1 && plane != 2))
		return m_ptr->width;
	return AV_CEIL_RSHIFT(m_ptr->width, desc->log2_chroma_w);
}

int FFmpeg::AVFrame::PlaneHeight(int plane) const noexcept {
	if (!m_ptr || m_ptr->height <= 0)
		return 0;
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(m_ptr->format));
	if (!desc || (plane != 1 && plane != 2))
		return m_ptr->height;
	return AV_CEIL_RSHIFT(m_ptr->height, desc->log2_chroma_h);
}

int FFmpeg::AVFrame::BitsPerComponent() const noexcept {
	if (!m_ptr)
		return 0;
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(m_ptr->format));
	if (!desc)
		return 8;
	return desc->comp[0].depth;
}

bool FFmpeg::AVFrame::PlanarInteger() const noexcept {
	if (!m_ptr || Width() <= 0 || Height() <= 0)
		return false;
	const auto* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(Format()));
	if (!desc || (desc->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_RGB
		| AV_PIX_FMT_FLAG_BITSTREAM | AV_PIX_FMT_FLAG_PAL | AV_PIX_FMT_FLAG_FLOAT)))
		return false;
	if (desc->nb_components != 1 && desc->nb_components != 3 && desc->nb_components != 4)
		return false;
	const int depth = desc->comp[0].depth;
	if (depth != 8 && depth != 10 && depth != 12 && depth != 16)
		return false;
	for (int component = 0; component < desc->nb_components; ++component) {
		const auto& sample = desc->comp[component];
		if (sample.plane != component || sample.depth != depth || sample.shift != 0
			|| sample.offset != 0 || sample.step != (depth == 8 ? 1 : 2)
			|| !Data(component))
			return false;
	}
	return true;
}

bool FFmpeg::AVFrame::BigEndianSamples() const noexcept {
	const auto* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(Format()));
	return desc && (desc->flags & AV_PIX_FMT_FLAG_BE);
}

const char* FFmpeg::AVFrame::FormatName() const noexcept {
	if (!m_ptr)
		return "?";
	if (m_ptr->width > 0) {
		const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(m_ptr->format));
		return name ? name : "?";
	}
	const char* name = av_get_sample_fmt_name(static_cast<AVSampleFormat>(m_ptr->format));
	return name ? name : "?";
}

const uint8_t* FFmpeg::AVFrame::Data(int plane) const noexcept {
	if (!m_ptr || plane < 0 || plane >= AV_NUM_DATA_POINTERS)
		return nullptr;
	return m_ptr->data[plane];
}

uint8_t* FFmpeg::AVFrame::Data(int plane) noexcept {
	if (!m_ptr || plane < 0 || plane >= AV_NUM_DATA_POINTERS)
		return nullptr;
	return m_ptr->data[plane];
}

int FFmpeg::AVFrame::Linesize(int plane) const noexcept {
	if (!m_ptr || plane < 0 || plane >= AV_NUM_DATA_POINTERS)
		return 0;
	return m_ptr->linesize[plane];
}

int FFmpeg::AVFrame::NbSamples() const noexcept {
	return m_ptr ? m_ptr->nb_samples : 0;
}

void FFmpeg::AVFrame::NbSamples(int samples) noexcept {
	if (m_ptr)
		m_ptr->nb_samples = samples;
}

int FFmpeg::AVFrame::SampleRate() const noexcept {
	return m_ptr ? m_ptr->sample_rate : 0;
}

void FFmpeg::AVFrame::SampleRate(int rate) noexcept {
	if (m_ptr)
		m_ptr->sample_rate = rate;
}

int FFmpeg::AVFrame::Channels() const noexcept {
	return m_ptr ? m_ptr->ch_layout.nb_channels : 0;
}

FFmpeg::AVChannelLayout FFmpeg::AVFrame::ChannelLayout() const noexcept {
	return m_ptr ? FFmpeg::FromRaw(m_ptr->ch_layout) : FFmpeg::AVChannelLayout{};
}

bool FFmpeg::AVFrame::CopyChannelLayout(const FFmpeg::AVChannelLayout& layout) noexcept {
	const auto* raw = FFmpeg::ToRaw(layout);
	return m_ptr && raw && av_channel_layout_copy(&m_ptr->ch_layout, raw) >= 0;
}

uint8_t** FFmpeg::AVFrame::ExtendedData() noexcept {
	return m_ptr ? m_ptr->extended_data : nullptr;
}

const uint8_t* const* FFmpeg::AVFrame::ExtendedData() const noexcept {
	return m_ptr ? m_ptr->extended_data : nullptr;
}

AVFrameSideData* FFmpeg::AVFrame::SideData(int type) noexcept {
	if (!m_ptr)
		return nullptr;
	return av_frame_get_side_data(m_ptr, static_cast<AVFrameSideDataType>(type));
}

void FFmpeg::AVFrame::RemoveSideData(int type) noexcept {
	if (m_ptr)
		av_frame_remove_side_data(m_ptr, static_cast<AVFrameSideDataType>(type));
}

int FFmpeg::AVFrame::SideDataCount() const noexcept {
	return m_ptr ? m_ptr->nb_side_data : 0;
}

const AVFrameSideData* FFmpeg::AVFrame::SideDataAt(int index) const noexcept {
	if (!m_ptr || index < 0 || index >= m_ptr->nb_side_data)
		return nullptr;
	return m_ptr->side_data[index];
}

uint8_t* FFmpeg::AVFrame::NewSideData(int type, int size) noexcept {
	if (!m_ptr || size < 0)
		return nullptr;
	AVFrameSideData* sd = av_frame_new_side_data(m_ptr, static_cast<AVFrameSideDataType>(type), size);
	return sd ? sd->data : nullptr;
}

bool FFmpeg::AVFrame::ScaleTo(AVFrame& dst, int dst_w, int dst_h,
	Resample filter, Scaler scaler) const noexcept {
	if (!m_ptr || dst_w <= 0 || dst_h <= 0 || Width() <= 0 || Height() <= 0)
		return false;
	const int fmt = (dst.Format() == AV_PIX_FMT_NONE) ? Format() : dst.Format();
	if (!dst.m_ptr || dst.Width() != dst_w || dst.Height() != dst_h
		|| dst.Format() != fmt || !dst.Data(0)) {
		if (!dst.AllocVideo(dst_w, dst_h, fmt))
			return false;
		(void)dst.CopyProps(*this);
		dst.Width(dst_w);
		dst.Height(dst_h);
		dst.Format(fmt);
	}
	if (scaler == Scaler::Zimg) {
		thread_local Backend::Zimg zimg;
		if (!zimg.Ensure(*this, dst, filter))
			return false;
		return zimg.Scale(*this, dst);
	}
	thread_local Sws sws;
	if (!sws.Ensure(Width(), Height(), Format(),
		dst.Width(), dst.Height(), dst.Format(), ToSwsFlags(filter)))
		return false;
	return sws.Scale(*this, dst);
}

void FFmpeg::AVFrame::Free() noexcept {
	if (m_ptr) {
		av_frame_free(&m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVFrame>;
