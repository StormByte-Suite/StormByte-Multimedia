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

#include <StormByte/multimedia/ffmpeg/AVBSF.hxx>
#include <StormByte/multimedia/backend/ffmpeg/AVPool.hxx>
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVDecoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/multimedia/ffmpeg/hardware_decode.hxx>

extern "C" {
	#include <libavcodec/avcodec.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVDecoder::AVDecoder(::AVCodecContext* ctx) noexcept:
AVPointer(ctx) {}

FFmpeg::AVDecoder::~AVDecoder() noexcept {
	Free();
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::OpenRaw(AVCodec* codec, const AVCodecParameters& params,
	int stream_index, int thread_count, DecodeAcceleration acceleration) noexcept {
	if (!codec || !params.Get())
		return Unexpected<DecoderError>("cannot open decoder: codec or codec parameters are missing");

	auto create = [&]() noexcept -> AVCodecContext* {
		AVCodecContext* context = avcodec_alloc_context3(codec);
		if (!context || avcodec_parameters_to_context(context, params.Get()) < 0) {
			avcodec_free_context(&context);
			return nullptr;
		}
		context->thread_count = thread_count;
		context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
		if (Backend::FFmpeg::AVPool::Attach(*context) < 0) {
			Backend::FFmpeg::AVPool::Release(context);
			return nullptr;
		}
		return context;
	};

	const bool video = params.Get()->codec_type == AVMEDIA_TYPE_VIDEO;
	bool tried_hardware = false;
	AVCodecContext* ctx = create();
	if (!ctx)
		return Unexpected<DecoderError>("cannot allocate decoder context or attach the shared video frame pool");

	if (video && acceleration == DecodeAcceleration::Auto)
		tried_hardware = HardwareDecode::Configure(*ctx, codec);

	int open_result = avcodec_open2(ctx, codec, nullptr);
	const int hardware_open_error = open_result;
	if (open_result < 0 && tried_hardware) {
		Backend::FFmpeg::AVPool::Release(ctx);
		ctx = create();
		if (!ctx)
			return Unexpected<DecoderError>("hardware decoder initialization failed ({}); cannot allocate software fallback context",
				static_cast<std::string_view>(FFmpeg::ErrorToString(hardware_open_error)));
		open_result = avcodec_open2(ctx, codec, nullptr);
	}
	if (open_result < 0) {
		Backend::FFmpeg::AVPool::Release(ctx);
		const auto reason = FFmpeg::ErrorToString(open_result);
		if (tried_hardware)
			return Unexpected<DecoderError>("cannot open decoder '{}': software fallback failed: {} (hardware attempt: {})",
				codec->name ? codec->name : "unknown", static_cast<std::string_view>(reason),
				static_cast<std::string_view>(FFmpeg::ErrorToString(hardware_open_error)));
		return Unexpected<DecoderError>("cannot open decoder '{}': {}",
			codec->name ? codec->name : "unknown", static_cast<std::string_view>(reason));
	}

	AVDecoder dec(ctx);
	dec.m_stream_index = stream_index;
	dec.m_hardware_fallback = video && acceleration == DecodeAcceleration::Auto && !ctx->hw_device_ctx;
	return dec;
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::Open(AVCodec* codec, const AVCodecParameters& params,
	const AVFormatContext&, int stream_index, int thread_count, DecodeAcceleration acceleration) noexcept {
	return OpenRaw(codec, params, stream_index, thread_count, acceleration);
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::Open(AVCodec* codec, const AVCodecParameters& params,
	const AVFormatContext& fmt, int stream_index, int thread_count) noexcept {
	return Open(codec, params, fmt, stream_index, thread_count, DecodeAcceleration::Software);
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::Open(AVCodec* codec, const AVCodecParameters& params,
	int stream_index, int thread_count, DecodeAcceleration acceleration) noexcept {
	return OpenRaw(codec, params, stream_index, thread_count, acceleration);
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::Open(AVCodec* codec, const AVCodecParameters& params,
	int stream_index, int thread_count) noexcept {
	return Open(codec, params, stream_index, thread_count, DecodeAcceleration::Software);
}

bool FFmpeg::AVDecoder::HardwareAccelerated() const noexcept {
	return m_ptr && m_ptr->hw_device_ctx;
}

bool FFmpeg::AVDecoder::HardwareFallback() const noexcept {
	return m_hardware_fallback;
}

FFmpeg::OperationResult FFmpeg::AVDecoder::SendPacket(AVPacket& pkt) noexcept {
	m_packet_error = 0;
	m_operation_error = 0;
	if (pkt.StreamIndex() != m_stream_index) {
		m_packet_error = AVERROR(EINVAL);
		m_operation_error = m_packet_error;
		return OperationResult::Error;
	}

	AVPacket filtered_pkt = pkt.Ref();
	const auto filtered = m_bsf_pipeline.Process(filtered_pkt);
	if (filtered != OperationResult::Success) {
		m_operation_error = m_bsf_pipeline.ErrorCode();
		m_packet_error = m_operation_error;
		return filtered;
	}

	const int ret = avcodec_send_packet(m_ptr, filtered_pkt.Get());
	m_packet_error = ret < 0 ? ret : 0;
	m_operation_error = m_packet_error;
	if (ret == AVERROR(EAGAIN))
		return OperationResult::TryAgain;
	if (ret == AVERROR_EOF)
		return OperationResult::EndOfFile;
	if (ret < 0)
		return OperationResult::Error;
	return OperationResult::Success;
}

int FFmpeg::AVDecoder::PacketError() const noexcept {
	return m_packet_error;
}

int FFmpeg::AVDecoder::OperationError() const noexcept {
	return m_operation_error;
}

FFmpeg::OperationResult FFmpeg::AVDecoder::ReceiveFrame(AVFrame& frame) noexcept {
	int ret = avcodec_receive_frame(m_ptr, frame.Get());
	m_operation_error = ret < 0 ? ret : 0;
	switch(ret) {
		case 0:
			if (frame.Hardware() && !frame.Download()) {
				m_operation_error = AVERROR(EIO);
				return OperationResult::Error;
			}
			return OperationResult::Success;
		case AVERROR(EAGAIN):
			return OperationResult::TryAgain;
		case AVERROR_EOF:
			return OperationResult::EndOfFile;
		default:
			return OperationResult::Error;
	}
}

int FFmpeg::AVDecoder::StreamIndex() const noexcept {
	return m_stream_index;
}

FFmpeg::AVRational FFmpeg::AVDecoder::TimeBase() const noexcept {
	if (!m_ptr)
		return FFmpeg::AVRational{0, 1};
	if (m_ptr->pkt_timebase.num > 0 && m_ptr->pkt_timebase.den > 0)
		return FFmpeg::FromRaw(m_ptr->pkt_timebase);
	return FFmpeg::FromRaw(m_ptr->time_base);
}

void FFmpeg::AVDecoder::Flush() noexcept {
	avcodec_flush_buffers(m_ptr);
	m_bsf_pipeline.Flush();
}

FFmpeg::OperationResult FFmpeg::AVDecoder::SetEof() noexcept {
	m_operation_error = 0;
	if (!m_ptr) {
		m_bsf_pipeline.SetEof();
		return OperationResult::EndOfFile;
	}

	if (IsSubtitle()) {
		m_bsf_pipeline.SetEof();
		return OperationResult::EndOfFile;
	}

	const int ret = avcodec_send_packet(m_ptr, nullptr);
	m_operation_error = ret < 0 ? ret : 0;
	if (ret == 0 || ret == AVERROR_EOF) {
		m_bsf_pipeline.SetEof();
		return (ret == 0) ? OperationResult::Success : OperationResult::EndOfFile;
	}

	if (ret == AVERROR(EAGAIN))
		return OperationResult::TryAgain;
	return OperationResult::Error;
}

bool FFmpeg::AVDecoder::IsSubtitle() const noexcept {
	return m_ptr && m_ptr->codec_type == AVMEDIA_TYPE_SUBTITLE;
}

FFmpeg::OperationResult FFmpeg::AVDecoder::DecodeSubtitle(AVPacket& pkt, FFmpeg::AVSubtitle& out) noexcept {
	m_operation_error = 0;
	if (!m_ptr || !IsSubtitle()) {
		m_operation_error = AVERROR(EINVAL);
		return OperationResult::Error;
	}
	if (pkt.StreamIndex() != m_stream_index) {
		m_operation_error = AVERROR(EINVAL);
		return OperationResult::Error;
	}

	int got = 0;
	const int ret = avcodec_decode_subtitle2(m_ptr, out.Get(), &got, pkt.Get());
	m_operation_error = ret < 0 ? ret : 0;
	if (ret < 0)
		return OperationResult::Error;
	if (!got)
		return OperationResult::TryAgain;
	return OperationResult::Success;
}

void FFmpeg::AVDecoder::Free() noexcept {
	if (m_ptr) {
		Backend::FFmpeg::AVPool::Release(m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVCodecContext>;
