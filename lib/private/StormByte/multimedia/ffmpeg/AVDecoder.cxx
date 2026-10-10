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

#include <StormByte/multimedia/ffmpeg/AVBSF.hxx>
#include <StormByte/multimedia/backend/ffmpeg/AVPool.hxx>
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVDecoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>

extern "C" {
	#include <libavcodec/avcodec.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVDecoder::AVDecoder(::AVCodecContext* ctx) noexcept:
AVPointer(ctx) {}

FFmpeg::AVDecoder::~AVDecoder() noexcept {
	Free();
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::OpenRaw(AVCodec* codec, const AVCodecParameters& params, int stream_index) noexcept {
	if (!codec || !params.Get())
		return Unexpected<DecoderError>("Invalid codec or parameters");

	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	if (!ctx)
		return Unexpected<DecoderError>("Out of memory allocating codec context");

	if (avcodec_parameters_to_context(ctx, params.Get()) < 0) {
		avcodec_free_context(&ctx);
		return Unexpected<DecoderError>("Failed to copy codec parameters");
	}

	ctx->thread_count = 0;
	ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

	if (Backend::FFmpeg::AVPool::Attach(*ctx) < 0) {
		avcodec_free_context(&ctx);
		return Unexpected<DecoderError>("Failed to allocate shared video pool owner");
	}

	if (avcodec_open2(ctx, codec, nullptr) < 0) {
		Backend::FFmpeg::AVPool::Release(ctx);
		return Unexpected<DecoderError>("Failed to open decoder");
	}

	AVDecoder dec(ctx);
	dec.m_stream_index = stream_index;
	return dec;
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::Open(AVCodec* codec, const AVCodecParameters& params, const AVFormatContext&, int stream_index) noexcept {
	return OpenRaw(codec, params, stream_index);
}

FFmpeg::ExpectedAVDecoder FFmpeg::AVDecoder::Open(AVCodec* codec, const AVCodecParameters& params, int stream_index) noexcept {
	return OpenRaw(codec, params, stream_index);
}

FFmpeg::OperationResult FFmpeg::AVDecoder::SendPacket(AVPacket& pkt) noexcept {
	if (pkt.StreamIndex() != m_stream_index)
		return OperationResult::Error;

	AVPacket filtered_pkt = pkt.Ref();
	int ret = m_bsf_pipeline.Process(filtered_pkt);
	if (ret < 0)
		return OperationResult::Error;

	ret = avcodec_send_packet(m_ptr, filtered_pkt.Get());
	if (ret == AVERROR(EAGAIN))
		return OperationResult::TryAgain;
	if (ret == AVERROR_EOF)
		return OperationResult::EndOfFile;
	if (ret < 0)
		return OperationResult::Error;
	return OperationResult::Success;
}

FFmpeg::OperationResult FFmpeg::AVDecoder::ReceiveFrame(AVFrame& frame) noexcept {
	int ret = avcodec_receive_frame(m_ptr, frame.Get());
	switch(ret) {
		case 0:
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
	if (!m_ptr) {
		m_bsf_pipeline.SetEof();
		return OperationResult::EndOfFile;
	}

	if (IsSubtitle()) {
		m_bsf_pipeline.SetEof();
		return OperationResult::EndOfFile;
	}

	const int ret = avcodec_send_packet(m_ptr, nullptr);
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
	if (!m_ptr || !IsSubtitle())
		return OperationResult::Error;
	if (pkt.StreamIndex() != m_stream_index)
		return OperationResult::Error;

	int got = 0;
	const int ret = avcodec_decode_subtitle2(m_ptr, out.Get(), &got, pkt.Get());
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
