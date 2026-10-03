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
#include <StormByte/multimedia/backend/pipeline/detail/encoder/video.hxx>
#include <StormByte/multimedia/backend/pipeline/frame.hxx>
#include <StormByte/multimedia/features.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/property/color.hxx>
#include <StormByte/multimedia/property/hdr10.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/pixfmt.h>
	#include <libavutil/rational.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Encoder;
namespace FFmpeg = StormByte::Multimedia::FFmpeg;

namespace {
	int ToAVPixelFormat(Property::PixelFormat format) noexcept {
		switch (format) {
			case Property::PixelFormat::YUV420P:	return AV_PIX_FMT_YUV420P;
			case Property::PixelFormat::YUV422P:	return AV_PIX_FMT_YUV422P;
			case Property::PixelFormat::YUV444P:	return AV_PIX_FMT_YUV444P;
			case Property::PixelFormat::YUV420P10:	return AV_PIX_FMT_YUV420P10LE;
			case Property::PixelFormat::YUV422P10:	return AV_PIX_FMT_YUV422P10LE;
			case Property::PixelFormat::YUV444P10:	return AV_PIX_FMT_YUV444P10LE;
			case Property::PixelFormat::YUV420P12:	return AV_PIX_FMT_YUV420P12LE;
			case Property::PixelFormat::YUV422P12:	return AV_PIX_FMT_YUV422P12LE;
			case Property::PixelFormat::YUV444P12:	return AV_PIX_FMT_YUV444P12LE;
			case Property::PixelFormat::NV12:		return AV_PIX_FMT_NV12;
			case Property::PixelFormat::NV21:		return AV_PIX_FMT_NV21;
			case Property::PixelFormat::P010:		return AV_PIX_FMT_P010LE;
			case Property::PixelFormat::RGB24:		return AV_PIX_FMT_RGB24;
			case Property::PixelFormat::BGR24:		return AV_PIX_FMT_BGR24;
			case Property::PixelFormat::RGBA:		return AV_PIX_FMT_RGBA;
			case Property::PixelFormat::BGRA:		return AV_PIX_FMT_BGRA;
			case Property::PixelFormat::GRAY8:		return AV_PIX_FMT_GRAY8;
			case Property::PixelFormat::GRAY10:		return AV_PIX_FMT_GRAY10LE;
			case Property::PixelFormat::GRAY16:		return AV_PIX_FMT_GRAY16LE;
			default:								return AV_PIX_FMT_NONE;
		}
	}

	int ToAVRange(Property::Range range) noexcept {
		switch (range) {
			case Property::Range::TV:	return AVCOL_RANGE_MPEG;
			case Property::Range::Full:	return AVCOL_RANGE_JPEG;
			default:					return AVCOL_RANGE_UNSPECIFIED;
		}
	}

	int ToAVSpace(Property::Space space) noexcept {
		switch (space) {
			case Property::Space::RGB:					return AVCOL_SPC_RGB;
			case Property::Space::BT709:				return AVCOL_SPC_BT709;
			case Property::Space::FCC:					return AVCOL_SPC_FCC;
			case Property::Space::BT470BG:				return AVCOL_SPC_BT470BG;
			case Property::Space::SMPTE170M:			return AVCOL_SPC_SMPTE170M;
			case Property::Space::SMPTE240M:			return AVCOL_SPC_SMPTE240M;
			case Property::Space::YCgCo:				return AVCOL_SPC_YCGCO;
			case Property::Space::BT2020NCL:			return AVCOL_SPC_BT2020_NCL;
			case Property::Space::BT2020CL:				return AVCOL_SPC_BT2020_CL;
			case Property::Space::SMPTE2085:			return AVCOL_SPC_SMPTE2085;
			case Property::Space::ChromaDerivedNCL:		return AVCOL_SPC_CHROMA_DERIVED_NCL;
			case Property::Space::ChromaDerivedCL:		return AVCOL_SPC_CHROMA_DERIVED_CL;
			case Property::Space::ICtCp:				return AVCOL_SPC_ICTCP;
			default:									return AVCOL_SPC_UNSPECIFIED;
		}
	}

	int ToAVPrimaries(Property::Primaries primaries) noexcept {
		switch (primaries) {
			case Property::Primaries::BT709:		return AVCOL_PRI_BT709;
			case Property::Primaries::BT470M:		return AVCOL_PRI_BT470M;
			case Property::Primaries::BT470BG:		return AVCOL_PRI_BT470BG;
			case Property::Primaries::SMPTE170M:	return AVCOL_PRI_SMPTE170M;
			case Property::Primaries::SMPTE240M:	return AVCOL_PRI_SMPTE240M;
			case Property::Primaries::Film:			return AVCOL_PRI_FILM;
			case Property::Primaries::BT2020:		return AVCOL_PRI_BT2020;
			case Property::Primaries::SMPTE428:		return AVCOL_PRI_SMPTE428;
			case Property::Primaries::SMPTE431:		return AVCOL_PRI_SMPTE431;
			case Property::Primaries::SMPTE432:		return AVCOL_PRI_SMPTE432;
			case Property::Primaries::EBU3213:		return AVCOL_PRI_EBU3213;
			default:								return AVCOL_PRI_UNSPECIFIED;
		}
	}

	int ToAVTransfer(Property::Transfer transfer) noexcept {
		switch (transfer) {
			case Property::Transfer::BT709:			return AVCOL_TRC_BT709;
			case Property::Transfer::Gamma22:		return AVCOL_TRC_GAMMA22;
			case Property::Transfer::Gamma28:		return AVCOL_TRC_GAMMA28;
			case Property::Transfer::SMPTE170M:		return AVCOL_TRC_SMPTE170M;
			case Property::Transfer::SMPTE240M:		return AVCOL_TRC_SMPTE240M;
			case Property::Transfer::Linear:		return AVCOL_TRC_LINEAR;
			case Property::Transfer::Log:			return AVCOL_TRC_LOG;
			case Property::Transfer::LogSqrt:		return AVCOL_TRC_LOG_SQRT;
			case Property::Transfer::IEC61966_2_4:	return AVCOL_TRC_IEC61966_2_4;
			case Property::Transfer::BT1361:		return AVCOL_TRC_BT1361_ECG;
			case Property::Transfer::IEC61966_2_1:	return AVCOL_TRC_IEC61966_2_1;
			case Property::Transfer::BT2020_10:		return AVCOL_TRC_BT2020_10;
			case Property::Transfer::BT2020_12:		return AVCOL_TRC_BT2020_12;
			case Property::Transfer::SMPTE2084:		return AVCOL_TRC_SMPTE2084;
			case Property::Transfer::SMPTE428:		return AVCOL_TRC_SMPTE428;
			case Property::Transfer::ARIB_B67:		return AVCOL_TRC_ARIB_STD_B67;
			default:								return AVCOL_TRC_UNSPECIFIED;
		}
	}

	FFmpeg::AVRational VideoTimeBase(const StormByte::Multimedia::Pipeline::Frame& frame) noexcept {
		if (frame.Video() && frame.Video()->FrameRate() && frame.Video()->FrameRate()->Valid()) {
			const auto& fps = *frame.Video()->FrameRate();
			return FFmpeg::AVRational{fps.den, fps.num};
		}

		return FFmpeg::AVRational{1, 24};
	}

	StormByte::Multimedia::FFmpeg::AVCodecParameters FillVideoParams(
		const StormByte::Multimedia::Pipeline::Frame& frame,
		const std::optional<std::int64_t>& bitRate,
		const StormByte::Multimedia::FFmpeg::AVFrame* handle) noexcept {
		StormByte::Multimedia::FFmpeg::AVCodecParameters params(nullptr);
		if (bitRate)
			params.BitRate(*bitRate);
		if (frame.Video()) {
			const auto& video = *frame.Video();
			params.Width(static_cast<int>(video.Resolution().Width()));
			params.Height(static_cast<int>(video.Resolution().Height()));
			params.Format(ToAVPixelFormat(video.Color().PixelFormat()));
			params.ColorRange(ToAVRange(video.Color().Range()));
			params.ColorSpace(ToAVSpace(video.Color().Space()));
			params.ColorPrimaries(ToAVPrimaries(video.Color().Primaries()));
			params.ColorTransfer(ToAVTransfer(video.Color().Transfer()));
			if (video.HDR10())
				params.WriteHdr10(*video.HDR10());
			if (video.SampleAspectRatio() && video.SampleAspectRatio()->Valid())
				params.SampleAspectRatio(*video.SampleAspectRatio());
		}

		if (handle && *handle) {
			if (handle->Format() != AV_PIX_FMT_NONE)
				params.Format(handle->Format());
			if (handle->Width() > 0)
				params.Width(handle->Width());
			if (handle->Height() > 0)
				params.Height(handle->Height());
			const auto sar = handle->SampleAspectRatio();
			if (sar.Valid())
				params.SampleAspectRatio(sar);
		}

		return params;
	}
}

Video::Video() noexcept
: m_owner(nullptr), m_timeBase{0, 1}, m_index(0), m_flushed(false), m_tsOffset(0), m_tsOffsetSet(false) {}

bool Video::IsOpen() const noexcept {
	return m_encoder.has_value();
}

const AVCodecContext* Video::Context() const noexcept {
	return m_encoder ? m_encoder->Context() : nullptr;
}

FFmpeg::AVRational Video::TimeBase() const noexcept {
	return m_timeBase;
}

void Video::StampOutgoing() noexcept {
	std::int64_t pts = m_scratch.Pts();
	std::int64_t dts = m_scratch.Dts();
	std::int64_t dur = m_scratch.Duration();

	if (!m_tsOffsetSet) {
		m_tsOffset = (pts != AV_NOPTS_VALUE) ? -pts : 0;
		m_tsOffsetSet = true;
	}

	if (pts != AV_NOPTS_VALUE)
		pts += m_tsOffset;
	if (dts != AV_NOPTS_VALUE)
		dts += m_tsOffset;

	if (dts == AV_NOPTS_VALUE)
		dts = (pts != AV_NOPTS_VALUE) ? pts : 0;
	if (pts == AV_NOPTS_VALUE)
		pts = dts;
	if (dur <= 0)
		dur = 1;
	m_scratch.Timestamps(pts, dts, dur);
}

bool Video::Open(StormByte::Multimedia::Pipeline::Encoder& owner,
	const StormByte::Multimedia::Pipeline::Frame& frame) noexcept {
	if (m_encoder)
		return true;
	if (frame.Type() != Type::Video) {
		owner.Fail("encoder destination is video but frame is not");
		return false;
	}

	const auto* handle = FrameHandle(owner, frame);
	if (!handle || !*handle) {
		owner.Fail("frame has no backend buffer");
		return false;
	}

	Features need = owner.Require();
	if (frame.Video() && frame.Video()->HDR10())
		need.Add(Feature::HDR10);

	auto opened = StormByte::Multimedia::Backend::Pipeline::Encoder::OpenCodec(
		owner, FillVideoParams(frame, owner.BitRate(), handle), VideoTimeBase(frame), need);
	if (!opened)
		return false;

	CommitOpen(owner, *opened);
	m_timeBase = opened->TimeBase();
	if (m_timeBase.num <= 0 || m_timeBase.den <= 0)
		m_timeBase = FFmpeg::AVRational{1, 24};
	m_encoder = std::move(opened->Handle());
	m_index = owner.Index();
	m_owner = &owner;
	return true;
}

bool Video::Push(StormByte::Multimedia::Pipeline::Encoder& owner,
	const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Frame>& frame) noexcept {
	if (!frame) {
		owner.Fail("empty frame");
		return false;
	}

	if (!m_encoder && !Open(owner, *frame))
		return false;
	if (!m_encoder)
		return false;
	auto* handle = FrameHandle(owner, *frame);
	if (!handle || !*handle) {
		owner.Fail("frame has no backend buffer");
		return false;
	}

	const AVCodecContext* ctx = m_encoder->Context();
	const int ctxW = ctx ? ctx->width : 0;
	const int ctxH = ctx ? ctx->height : 0;
	const int ctxFmt = ctx ? static_cast<int>(ctx->pix_fmt) : -1;

	if (ctx && (handle->Width() != ctxW
			|| handle->Height() != ctxH
			|| handle->Format() != ctxFmt)) {
		owner.Fail(std::format(
			"encoder frame mismatch ctx={}x{} fmt={} in={}x{} fmt={}",
			ctxW, ctxH, ctxFmt,
			handle->Width(), handle->Height(), handle->Format()));
		return false;
	}

	if (frame->Video() && frame->Video()->HDR10())
		handle->WriteHdr10(*frame->Video()->HDR10());
	if (frame->Video() && frame->Video()->SampleAspectRatio()
		&& frame->Video()->SampleAspectRatio()->Valid())
		handle->SampleAspectRatio(*frame->Video()->SampleAspectRatio());
	handle->WriteSideData(frame->Attachments());

	if (frame->Pts())
		handle->Pts(StormByte::Multimedia::Backend::Pipeline::Encoder::NsToTicks(
			frame->Pts()->Nanoseconds().count(), m_timeBase));
	else
		handle->Pts(AV_NOPTS_VALUE);
	if (frame->Duration()) {
		auto duration = StormByte::Multimedia::Backend::Pipeline::Encoder::NsToTicks(
			frame->Duration()->Nanoseconds().count(), m_timeBase);
		if (duration <= 0)
			duration = 1;
		handle->DurationTicks(duration);
	}
	else
		handle->DurationTicks(1);

	const auto result = m_encoder->SendFrame(*handle);
	if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain)
		return false;
	if (result == StormByte::Multimedia::FFmpeg::OperationResult::Error) {
		owner.Fail(std::format(
			"failed to send frame ctx={}x{} fmt={} in={}x{} fmt={} pts={}",
			ctxW, ctxH, ctxFmt,
			handle->Width(), handle->Height(), handle->Format(),
			handle->Pts()));
		return false;
	}

	return true;
}

bool Video::DrainOne(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept {
	if (owner.Failed() || !m_encoder)
		return false;
	const auto result = m_encoder->ReceivePacket(m_scratch);
	if (result == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain
		|| result == StormByte::Multimedia::FFmpeg::OperationResult::EndOfFile)
		return false;
	if (result != StormByte::Multimedia::FFmpeg::OperationResult::Success) {
		owner.Fail("failed to receive packet");
		return false;
	}

	StampOutgoing();
	const bool keepPacketHdrPlus = m_encoder->CodecId() != AV_CODEC_ID_HEVC;
	m_pending.push_back(StormByte::Multimedia::Backend::Pipeline::Encoder::MakePacket(
		owner, Type::Video, owner.Index(), m_scratch, m_timeBase, keepPacketHdrPlus));
	m_scratch.Unref();
	return true;
}

void Video::Flush(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept {
	if (owner.Failed() || !m_encoder || m_flushed)
		return;
	for (;;) {
		const auto sent = m_encoder->SetEof();
		if (sent == StormByte::Multimedia::FFmpeg::OperationResult::Success
			|| sent == StormByte::Multimedia::FFmpeg::OperationResult::EndOfFile)
			break;
		if (sent == StormByte::Multimedia::FFmpeg::OperationResult::TryAgain) {
			if (!DrainOne(owner))
				break;
			continue;
		}

		owner.Fail("failed to signal encoder EOF");
		return;
	}

	while (DrainOne(owner))
		;
	m_flushed = true;
}

StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> Video::Take() noexcept {
	if (m_pending.empty() && m_encoder && m_owner) {
		const auto result = m_encoder->ReceivePacket(m_scratch);
		if (result == StormByte::Multimedia::FFmpeg::OperationResult::Success) {
			StampOutgoing();
			const bool keepPacketHdrPlus = m_encoder->CodecId() != AV_CODEC_ID_HEVC;
			m_pending.push_back(StormByte::Multimedia::Backend::Pipeline::Encoder::MakePacket(
				*m_owner, Type::Video, m_index, m_scratch, m_timeBase, keepPacketHdrPlus));
			m_scratch.Unref();
		}
	}

	if (m_pending.empty())
		return {};
	auto packet = std::move(m_pending.front());
	m_pending.pop_front();
	return packet;
}
