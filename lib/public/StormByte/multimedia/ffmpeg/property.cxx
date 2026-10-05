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
#include <StormByte/multimedia/ffmpeg/AVStream.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>
#include <StormByte/multimedia/ffmpeg/property.hxx>
#include <StormByte/multimedia/property/av_rational.hxx>

#include <cstdint>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/pixfmt.h>
}

namespace FFmpeg = StormByte::Multimedia::FFmpeg;
using StormByte::Multimedia::Property::Audio;
using StormByte::Multimedia::Property::ChannelLayout;
using StormByte::Multimedia::Property::Color;
using StormByte::Multimedia::Property::HDR10;
using StormByte::Multimedia::Property::PixelFormat;
using StormByte::Multimedia::Property::Point;
using StormByte::Multimedia::Property::Primaries;
using StormByte::Multimedia::Property::Range;
using StormByte::Multimedia::Property::Resolution;
using StormByte::Multimedia::Property::Space;
using StormByte::Multimedia::Property::Transfer;
using StormByte::Multimedia::Property::Video;

namespace {
	constexpr int ChromaDenominator = 50000;
	constexpr int LuminanceDenominator = 10000;

	Point FromChromaPair(const ::AVRational& x, const ::AVRational& y) noexcept {
		return Point::Normalized(x.num, x.den, y.num, y.den, ChromaDenominator);
	}

	Point FromLuminancePair(const ::AVRational& minNits, const ::AVRational& maxNits) noexcept {
		return Point::Normalized(minNits.num, minNits.den, maxNits.num, maxNits.den, LuminanceDenominator);
	}

	const uint8_t* CodecSideData(const ::AVStream* raw, enum AVPacketSideDataType type, size_t& size) noexcept {
		size = 0;
		if (!raw || !raw->codecpar)
			return nullptr;
		const AVPacketSideData* sd = av_packet_side_data_get(
			raw->codecpar->coded_side_data, raw->codecpar->nb_coded_side_data, type);
		if (!sd)
			return nullptr;
		size = sd->size;
		return sd->data;
	}

	StormByte::Safe::Optional<StormByte::Multimedia::Property::AVRational> IfValid(FFmpeg::AVRational r) noexcept {
		if (!r.Valid())
			return std::nullopt;
		return r;
	}

	bool LooksLikeHDR10(Transfer transfer, Primaries primaries) noexcept {
		return transfer == Transfer::SMPTE2084 && primaries == Primaries::BT2020;
	}

	StormByte::Safe::Optional<HDR10> MapHDR10(const ::AVStream* raw, Transfer transfer, Primaries primaries) noexcept {
		if (!LooksLikeHDR10(transfer, primaries))
			return std::nullopt;

		size_t mdmSize = 0;
		const uint8_t* mdmData = CodecSideData(raw, AV_PKT_DATA_MASTERING_DISPLAY_METADATA, mdmSize);
		const auto* mdm = (mdmData && mdmSize >= sizeof(AVMasteringDisplayMetadata))
			? reinterpret_cast<const AVMasteringDisplayMetadata*>(mdmData)
			: nullptr;

		size_t cllSize = 0;
		const uint8_t* cllData = CodecSideData(raw, AV_PKT_DATA_CONTENT_LIGHT_LEVEL, cllSize);
		const auto* cll = (cllData && cllSize >= sizeof(AVContentLightMetadata))
			? reinterpret_cast<const AVContentLightMetadata*>(cllData)
			: nullptr;

		size_t plusSize = 0;
		const bool hdr10plus = CodecSideData(raw, AV_PKT_DATA_DYNAMIC_HDR10_PLUS, plusSize) != nullptr;

		StormByte::Safe::Optional<Point> light;
		if (cll && (cll->MaxCLL || cll->MaxFALL))
			light = Point{static_cast<int>(cll->MaxCLL), static_cast<int>(cll->MaxFALL)};

		if (mdm && mdm->has_primaries && mdm->has_luminance) {
			HDR10 out{
				FromChromaPair(mdm->display_primaries[0][0], mdm->display_primaries[0][1]),
				FromChromaPair(mdm->display_primaries[1][0], mdm->display_primaries[1][1]),
				FromChromaPair(mdm->display_primaries[2][0], mdm->display_primaries[2][1]),
				FromChromaPair(mdm->white_point[0], mdm->white_point[1]),
				FromLuminancePair(mdm->min_luminance, mdm->max_luminance),
				light,
				HDR10::Source::Metadata
			};
			out.HDR10Plus(hdr10plus);
			return out;
		}

		if (light.has_value()) {
			HDR10 out{
				HDR10::DEFAULT.Red(), HDR10::DEFAULT.Green(), HDR10::DEFAULT.Blue(),
				HDR10::DEFAULT.White(), HDR10::DEFAULT.Luminance(),
				light, HDR10::Source::Heuristics
			};
			out.HDR10Plus(hdr10plus);
			return out;
		}

		HDR10 out = HDR10::DEFAULT;
		out.HDR10Plus(hdr10plus);
		return out;
	}

	PixelFormat MapPixelFormat(int format) noexcept {
		switch (format) {
			case AV_PIX_FMT_YUV420P:
			case AV_PIX_FMT_YUVJ420P:
				return PixelFormat::YUV420P;
			case AV_PIX_FMT_YUV422P:
			case AV_PIX_FMT_YUVJ422P:
				return PixelFormat::YUV422P;
			case AV_PIX_FMT_YUV444P:
			case AV_PIX_FMT_YUVJ444P:
				return PixelFormat::YUV444P;
			case AV_PIX_FMT_YUV420P10LE:
			case AV_PIX_FMT_YUV420P10BE:
				return PixelFormat::YUV420P10;
			case AV_PIX_FMT_YUV422P10LE:
			case AV_PIX_FMT_YUV422P10BE:
				return PixelFormat::YUV422P10;
			case AV_PIX_FMT_YUV444P10LE:
			case AV_PIX_FMT_YUV444P10BE:
				return PixelFormat::YUV444P10;
			case AV_PIX_FMT_YUV420P12LE:
			case AV_PIX_FMT_YUV420P12BE:
				return PixelFormat::YUV420P12;
			case AV_PIX_FMT_YUV422P12LE:
			case AV_PIX_FMT_YUV422P12BE:
				return PixelFormat::YUV422P12;
			case AV_PIX_FMT_YUV444P12LE:
			case AV_PIX_FMT_YUV444P12BE:
				return PixelFormat::YUV444P12;
			case AV_PIX_FMT_NV12:
				return PixelFormat::NV12;
			case AV_PIX_FMT_NV21:
				return PixelFormat::NV21;
			case AV_PIX_FMT_P010LE:
			case AV_PIX_FMT_P010BE:
				return PixelFormat::P010;
			case AV_PIX_FMT_RGB24:
				return PixelFormat::RGB24;
			case AV_PIX_FMT_BGR24:
				return PixelFormat::BGR24;
			case AV_PIX_FMT_RGBA:
				return PixelFormat::RGBA;
			case AV_PIX_FMT_BGRA:
				return PixelFormat::BGRA;
			case AV_PIX_FMT_GRAY8:
				return PixelFormat::GRAY8;
			case AV_PIX_FMT_GRAY10LE:
			case AV_PIX_FMT_GRAY10BE:
				return PixelFormat::GRAY10;
			case AV_PIX_FMT_GRAY16LE:
			case AV_PIX_FMT_GRAY16BE:
				return PixelFormat::GRAY16;
			default:
				return PixelFormat::Unknown;
		}
	}

	Range MapRange(int range) noexcept {
		switch (range) {
			case AVCOL_RANGE_MPEG:
				return Range::TV;
			case AVCOL_RANGE_JPEG:
				return Range::Full;
			case AVCOL_RANGE_UNSPECIFIED:
				return Range::Unspecified;
			default:
				return Range::Unknown;
		}
	}

	Space MapSpace(int space) noexcept {
		switch (space) {
			case AVCOL_SPC_RGB:
				return Space::RGB;
			case AVCOL_SPC_BT709:
				return Space::BT709;
			case AVCOL_SPC_FCC:
				return Space::FCC;
			case AVCOL_SPC_BT470BG:
				return Space::BT470BG;
			case AVCOL_SPC_SMPTE170M:
				return Space::SMPTE170M;
			case AVCOL_SPC_SMPTE240M:
				return Space::SMPTE240M;
			case AVCOL_SPC_YCGCO:
				return Space::YCgCo;
			case AVCOL_SPC_BT2020_NCL:
				return Space::BT2020NCL;
			case AVCOL_SPC_BT2020_CL:
				return Space::BT2020CL;
			case AVCOL_SPC_SMPTE2085:
				return Space::SMPTE2085;
			case AVCOL_SPC_CHROMA_DERIVED_NCL:
				return Space::ChromaDerivedNCL;
			case AVCOL_SPC_CHROMA_DERIVED_CL:
				return Space::ChromaDerivedCL;
			case AVCOL_SPC_ICTCP:
				return Space::ICtCp;
			case AVCOL_SPC_UNSPECIFIED:
				return Space::Unspecified;
			default:
				return Space::Unknown;
		}
	}

	Primaries MapPrimaries(int primaries) noexcept {
		switch (primaries) {
			case AVCOL_PRI_BT709:
				return Primaries::BT709;
			case AVCOL_PRI_BT470M:
				return Primaries::BT470M;
			case AVCOL_PRI_BT470BG:
				return Primaries::BT470BG;
			case AVCOL_PRI_SMPTE170M:
				return Primaries::SMPTE170M;
			case AVCOL_PRI_SMPTE240M:
				return Primaries::SMPTE240M;
			case AVCOL_PRI_FILM:
				return Primaries::Film;
			case AVCOL_PRI_BT2020:
				return Primaries::BT2020;
			case AVCOL_PRI_SMPTE428:
				return Primaries::SMPTE428;
			case AVCOL_PRI_SMPTE431:
				return Primaries::SMPTE431;
			case AVCOL_PRI_SMPTE432:
				return Primaries::SMPTE432;
			case AVCOL_PRI_EBU3213:
				return Primaries::EBU3213;
			case AVCOL_PRI_UNSPECIFIED:
				return Primaries::Unspecified;
			default:
				return Primaries::Unknown;
		}
	}

	Transfer MapTransfer(int transfer) noexcept {
		switch (transfer) {
			case AVCOL_TRC_BT709:
				return Transfer::BT709;
			case AVCOL_TRC_GAMMA22:
				return Transfer::Gamma22;
			case AVCOL_TRC_GAMMA28:
				return Transfer::Gamma28;
			case AVCOL_TRC_SMPTE170M:
				return Transfer::SMPTE170M;
			case AVCOL_TRC_SMPTE240M:
				return Transfer::SMPTE240M;
			case AVCOL_TRC_LINEAR:
				return Transfer::Linear;
			case AVCOL_TRC_LOG:
				return Transfer::Log;
			case AVCOL_TRC_LOG_SQRT:
				return Transfer::LogSqrt;
			case AVCOL_TRC_IEC61966_2_4:
				return Transfer::IEC61966_2_4;
			case AVCOL_TRC_BT1361_ECG:
				return Transfer::BT1361;
			case AVCOL_TRC_IEC61966_2_1:
				return Transfer::IEC61966_2_1;
			case AVCOL_TRC_BT2020_10:
				return Transfer::BT2020_10;
			case AVCOL_TRC_BT2020_12:
				return Transfer::BT2020_12;
			case AVCOL_TRC_SMPTE2084:
				return Transfer::SMPTE2084;
			case AVCOL_TRC_SMPTE428:
				return Transfer::SMPTE428;
			case AVCOL_TRC_ARIB_STD_B67:
				return Transfer::ARIB_B67;
			case AVCOL_TRC_UNSPECIFIED:
				return Transfer::Unspecified;
			default:
				return Transfer::Unknown;
		}
	}

	ChannelLayout MapChannelLayout(const ::AVChannelLayout* layout) noexcept {
		if (!layout || layout->order != AV_CHANNEL_ORDER_NATIVE)
			return ChannelLayout::Unknown;
		switch (layout->u.mask) {
			case AV_CH_LAYOUT_MONO:
				return ChannelLayout::Mono;
			case AV_CH_LAYOUT_STEREO:
				return ChannelLayout::Stereo;
			case AV_CH_LAYOUT_2POINT1:
				return ChannelLayout::TwoPointOne;
			case AV_CH_LAYOUT_SURROUND:
				return ChannelLayout::ThreePointZero;
			case AV_CH_LAYOUT_4POINT0:
				return ChannelLayout::FourPointZero;
			case AV_CH_LAYOUT_QUAD:
				return ChannelLayout::Quad;
			case AV_CH_LAYOUT_5POINT0:
			case AV_CH_LAYOUT_5POINT0_BACK:
				return ChannelLayout::FivePointZero;
			case AV_CH_LAYOUT_5POINT1:
			case AV_CH_LAYOUT_5POINT1_BACK:
				return ChannelLayout::FivePointOne;
			case AV_CH_LAYOUT_6POINT1:
				return ChannelLayout::SixPointOne;
			case AV_CH_LAYOUT_7POINT1:
				return ChannelLayout::SevenPointOne;
			case AV_CH_LAYOUT_7POINT1_WIDE:
			case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
				return ChannelLayout::SevenPointOneW;
			case AV_CH_LAYOUT_OCTAGONAL:
				return ChannelLayout::Octagonal;
			case AV_CH_LAYOUT_22POINT2:
				return ChannelLayout::TwentyTwoPointTwo;
			default:
				return ChannelLayout::Unknown;
		}
	}
}

StormByte::Multimedia::Stream::Properties FFmpeg::MapProperties(const AVStream& stream) noexcept {
	const auto params = stream.CodecParameters();
	switch (stream.Type()) {
		case AVMEDIA_TYPE_VIDEO: {
			if (params.Width() <= 0 || params.Height() <= 0)
				return {};
			const auto pix = MapPixelFormat(params.Format());
			const auto range = MapRange(params.ColorRange());
			const auto space = MapSpace(params.ColorSpace());
			const auto primaries = MapPrimaries(params.ColorPrimaries());
			const auto transfer = MapTransfer(params.ColorTransfer());

			return {Video{
				Color{pix, range, space, primaries, transfer},
				Resolution{
					static_cast<std::uint32_t>(params.Width()),
					static_cast<std::uint32_t>(params.Height())
				},
				MapHDR10(stream.Raw(), transfer, primaries),
				IfValid(stream.FrameRateRational()),
				IfValid(stream.SampleAspectRatio())
			}, std::nullopt};
		}

		case AVMEDIA_TYPE_AUDIO: {
			const int sampleRate = params.SampleRate();
			const int channels = params.Channels();
			if (sampleRate <= 0 && channels <= 0)
				return {};
			StormByte::Safe::Optional<StormByte::Safe::String> profile;
			const char* name = avcodec_profile_name(static_cast<AVCodecID>(params.CodecId()), params.Profile());
			if (name && name[0] != '\0')
				profile = name;
			const auto bitRate = params.BitRate();
			return {std::nullopt, Audio{
				MapChannelLayout(FFmpeg::ToRaw(params.ChannelLayout())),
				static_cast<std::uint32_t>(sampleRate > 0 ? sampleRate : 0),
				static_cast<std::uint8_t>(channels > 0 ? channels : 0),
				bitRate > 0 ? static_cast<std::uint64_t>(bitRate) : 0,
				std::move(profile)
			}};
		}

		default:
			return {};
	}
}
