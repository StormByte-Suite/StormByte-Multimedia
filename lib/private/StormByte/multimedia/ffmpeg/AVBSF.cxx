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
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/bsf.h>
}

using namespace StormByte::Multimedia;

FFmpeg::AVBSF::AVBSF(AVBSFContext* ctx) noexcept
:AVPointer(ctx) {}

FFmpeg::AVBSF::~AVBSF() noexcept {
	Free();
}

FFmpeg::ExpectedAVBSF FFmpeg::AVBSF::Create(std::string_view name, const AVCodecParameters& params, FFmpeg::AVRational time_base) {
	if (name.empty() || !params.Get())
		return Unexpected<BSFError>("Invalid BSF name or parameters");

	const StormByte::Safe::String terminatedName(name);
	const AVBitStreamFilter* filter = av_bsf_get_by_name(terminatedName.data());
	if (!filter)
		return Unexpected<BSFError>("Bitstream filter not found");
	AVBSFContext* ctx = nullptr;
	int result = av_bsf_alloc(filter, &ctx);
	if (result < 0)
		return Unexpected<BSFError>("cannot allocate bitstream filter '{}': {}",
			name, static_cast<std::string_view>(FFmpeg::ErrorToString(result)));

	result = avcodec_parameters_copy(ctx->par_in, params.Get());
	if (result < 0) {
		av_bsf_free(&ctx);
		return Unexpected<BSFError>("cannot copy codec parameters into bitstream filter '{}': {}",
			name, static_cast<std::string_view>(FFmpeg::ErrorToString(result)));
	}

	ctx->time_base_in = FFmpeg::ToRaw(time_base);

	result = av_bsf_init(ctx);
	if (result < 0) {
		av_bsf_free(&ctx);
		return Unexpected<BSFError>("cannot initialize bitstream filter '{}': {}",
			name, static_cast<std::string_view>(FFmpeg::ErrorToString(result)));
	}

	return AVBSF(ctx);
}

FFmpeg::OperationResult FFmpeg::AVBSF::SendPacket(AVPacket& pkt) noexcept {
	int ret = av_bsf_send_packet(m_ptr, pkt.Get());
	m_error = ret < 0 ? ret : 0;

	if (ret == 0)                 return OperationResult::Success;
	if (ret == AVERROR(EAGAIN))   return OperationResult::TryAgain;
	if (ret == AVERROR_EOF)       return OperationResult::EndOfFile;

	return OperationResult::Error;
}

FFmpeg::OperationResult FFmpeg::AVBSF::ReceivePacket(AVPacket& pkt) noexcept {
	int ret = av_bsf_receive_packet(m_ptr, pkt.m_ptr);
	m_error = ret < 0 ? ret : 0;

	if (ret == 0)                 return OperationResult::Success;
	if (ret == AVERROR(EAGAIN))   return OperationResult::TryAgain;
	if (ret == AVERROR_EOF)       return OperationResult::EndOfFile;

	return OperationResult::Error;
}

int FFmpeg::AVBSF::ErrorCode() const noexcept {
	return m_error;
}

void FFmpeg::AVBSF::Flush() noexcept {
	av_bsf_flush(m_ptr);
}

void FFmpeg::AVBSF::SetEof() noexcept {
	const int result = av_bsf_send_packet(m_ptr, nullptr);
	m_error = result < 0 && result != AVERROR_EOF ? result : 0;
}

void FFmpeg::AVBSF::Free() noexcept {
	if (m_ptr) {
		av_bsf_free(&m_ptr);
		m_ptr = nullptr;
	}
}

// Explicit template instantiation
template class StormByte::Multimedia::FFmpeg::AVPointer<::AVBSFContext>;
