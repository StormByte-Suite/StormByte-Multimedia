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
#include <StormByte/multimedia/ffmpeg/AVEncoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>

#include <cstring>
#include <string>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
	#include <libavutil/hdr_dynamic_metadata.h>
	#include <libavutil/mem.h>
	#include <libavutil/opt.h>
}

using namespace StormByte::Multimedia;

namespace {
	constexpr const char DefaultAssHeader[] =
		"[Script Info]\n"
		"ScriptType: v4.00+\n"
		"\n"
		"[V4+ Styles]\n"
		"Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
		"Style: Default,Arial,16,&Hffffff,&Hffffff,&H0,&H0,0,0,0,0,100,100,0,0,1,1,0,2,10,10,10,0\n"
		"\n"
		"[Events]\n"
		"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

	std::string AvError(int err) noexcept {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(err, buf, sizeof(buf));
		return buf;
	}

	AVFrameSideDataType FrameTypeFromPacket(enum AVPacketSideDataType type) noexcept {
		switch (type) {
			case AV_PKT_DATA_MASTERING_DISPLAY_METADATA:
				return AV_FRAME_DATA_MASTERING_DISPLAY_METADATA;
			case AV_PKT_DATA_CONTENT_LIGHT_LEVEL:
				return AV_FRAME_DATA_CONTENT_LIGHT_LEVEL;
			case AV_PKT_DATA_DYNAMIC_HDR10_PLUS:
				return AV_FRAME_DATA_DYNAMIC_HDR_PLUS;
			default:
				return AV_FRAME_DATA_SEI_UNREGISTERED;
		}
	}

	void PromoteCodedSideData(::AVCodecContext* ctx) noexcept {
		if (!ctx || !ctx->coded_side_data)
			return;
		for (int i = 0; i < ctx->nb_coded_side_data; ++i) {
			const AVPacketSideData& src = ctx->coded_side_data[i];
			if (!src.data || src.size <= 0)
				continue;
			const auto frameType = FrameTypeFromPacket(src.type);
			if (frameType == AV_FRAME_DATA_SEI_UNREGISTERED
				&& src.type != AV_PKT_DATA_MASTERING_DISPLAY_METADATA
				&& src.type != AV_PKT_DATA_CONTENT_LIGHT_LEVEL
				&& src.type != AV_PKT_DATA_DYNAMIC_HDR10_PLUS)
				continue;
			AVFrameSideData* dst = av_frame_side_data_new(&ctx->decoded_side_data,
				&ctx->nb_decoded_side_data, frameType, src.size, 0);
			if (!dst || !dst->data)
				continue;
			std::memcpy(dst->data, src.data, static_cast<std::size_t>(src.size));
		}
	}

	void PushSeiLength(std::vector<std::uint8_t>& out, std::size_t value) noexcept {
		while (value >= 255) {
			out.push_back(0xFF);
			value -= 255;
		}

		out.push_back(static_cast<std::uint8_t>(value));
	}

	void AppendRbsp(std::vector<std::uint8_t>& out, const std::uint8_t* src, std::size_t size) noexcept {
		int zeros = 0;
		for (std::size_t i = 0; i < size; ++i) {
			if (zeros >= 2 && src[i] <= 0x03) {
				out.push_back(0x03);
				zeros = 0;
			}

			out.push_back(src[i]);
			zeros = (src[i] == 0) ? zeros + 1 : 0;
		}
	}

	std::vector<std::uint8_t> MakeHevcPrefixSeiT35(const std::vector<std::uint8_t>& t35) noexcept {
		std::vector<std::uint8_t> payload;
		payload.push_back(4);
		PushSeiLength(payload, t35.size());
		payload.insert(payload.end(), t35.begin(), t35.end());
		payload.push_back(0x80);

		std::vector<std::uint8_t> nal;
		nal.push_back(0x4E);
		nal.push_back(0x01);
		AppendRbsp(nal, payload.data(), payload.size());
		return nal;
	}

	bool PacketLooksAnnexB(const std::uint8_t* data, int size) noexcept {
		if (!data || size < 3)
			return false;
		if (data[0] == 0 && data[1] == 0 && data[2] == 1)
			return true;
		return size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1;
	}

	bool PrependHevcSei(FFmpeg::AVPacket& pkt, const std::vector<std::uint8_t>& t35) noexcept {
		const auto* src = pkt.Data();
		const int srcSize = pkt.Size();
		if (!src || srcSize <= 0 || t35.empty())
			return true;

		const auto nal = MakeHevcPrefixSeiT35(t35);
		std::vector<std::uint8_t> out;
		if (PacketLooksAnnexB(src, srcSize)) {
			out.insert(out.end(), {0x00, 0x00, 0x00, 0x01});
			out.insert(out.end(), nal.begin(), nal.end());
			out.insert(out.end(), src, src + srcSize);
		}

		else {
			const auto len = static_cast<std::uint32_t>(nal.size());
			out.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
			out.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
			out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
			out.push_back(static_cast<std::uint8_t>(len & 0xFF));
			out.insert(out.end(), nal.begin(), nal.end());
			out.insert(out.end(), src, src + srcSize);
		}

		const int stream = pkt.StreamIndex() >= 0 ? pkt.StreamIndex() : 0;
		const bool key = (pkt.Flags() & AV_PKT_FLAG_KEY) != 0;
		const std::int64_t pts = pkt.Pts();
		const std::int64_t dts = pkt.Dts();
		const std::int64_t duration = pkt.Duration();
		if (!pkt.Load(out.data(), static_cast<int>(out.size()), stream, key))
			return false;
		pkt.Timestamps(pts, dts, duration);
		return true;
	}

	std::vector<std::uint8_t> T35FromFrame(const FFmpeg::AVFrame& frame) noexcept {
		std::vector<std::uint8_t> out;
		const AVFrameSideData* sd = frame.SideData(AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
		if (!sd || !sd->data || sd->size <= 0)
			return out;
		const auto* plus = reinterpret_cast<const AVDynamicHDRPlus*>(sd->data);
		uint8_t* data = nullptr;
		size_t size = 0;
		if (av_dynamic_hdr_plus_to_t35(plus, &data, &size) < 0 || !data || size == 0)
			return out;
		out = {
			0xB5,
			0x00, 0x3C,
			0x00, 0x01,
			0x04
		};
		out.insert(out.end(), data, data + size);
		av_free(data);
		return out;
	}
}

FFmpeg::AVEncoder::AVEncoder(::AVCodecContext* ctx) noexcept
:AVPointer(ctx) {}

FFmpeg::AVEncoder::AVEncoder(AVEncoder&& other) noexcept = default;

FFmpeg::AVEncoder& FFmpeg::AVEncoder::operator=(AVEncoder&& other) noexcept = default;

FFmpeg::AVEncoder::~AVEncoder() noexcept {
	Free();
}

FFmpeg::ExpectedAVEncoder FFmpeg::AVEncoder::Open(std::string_view codec_name, const AVCodecParameters& params, int stream_index,
	const StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>& options, FFmpeg::AVRational time_base) noexcept {
	const std::string codec_name_copy{codec_name};
	const AVCodec* codec = avcodec_find_encoder_by_name(codec_name_copy.c_str());
	if (!codec || !params.Get())
		return Unexpected<FFmpeg::EncoderError>("Invalid codec or parameters");

	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	if (!ctx)
		return Unexpected<FFmpeg::EncoderError>("Out of memory allocating codec context");

	if (avcodec_parameters_to_context(ctx, params.Get()) < 0) {
		avcodec_free_context(&ctx);
		return Unexpected<FFmpeg::EncoderError>("Failed to copy codec parameters to encoder context");
	}

	PromoteCodedSideData(ctx);

	ctx->thread_count = 0;
	ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
	ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (time_base.num > 0 && time_base.den > 0) {
		ctx->time_base = FFmpeg::ToRaw(time_base);
		ctx->pkt_timebase = FFmpeg::ToRaw(time_base);
		if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
			ctx->framerate.num = time_base.den;
			ctx->framerate.den = time_base.num;
		}
	}

	if (ctx->codec_type == AVMEDIA_TYPE_SUBTITLE && !ctx->subtitle_header) {
		const auto n = std::strlen(DefaultAssHeader);
		ctx->subtitle_header = static_cast<std::uint8_t*>(av_malloc(n + 1));
		if (!ctx->subtitle_header) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("Out of memory allocating subtitle header");
		}

		std::memcpy(ctx->subtitle_header, DefaultAssHeader, n + 1);
		ctx->subtitle_header_size = static_cast<int>(n);
	}

	for (const auto& [key, value] : options) {
		if (key.empty())
			continue;
		if (av_opt_set(ctx, key.data(), value.empty() ? "" : value.data(), AV_OPT_SEARCH_CHILDREN) < 0) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("failed to set encoder option '{}'", static_cast<std::string_view>(key));
		}
	}

	const int opened = avcodec_open2(ctx, codec, nullptr);
	if (opened < 0) {
		const std::string why = AvError(opened);
		avcodec_free_context(&ctx);
		return Unexpected<FFmpeg::EncoderError>("Failed to open encoder: " + why);
	}

	if (ctx->time_base.num <= 0 || ctx->time_base.den <= 0) {
		if (time_base.num > 0 && time_base.den > 0)
			ctx->time_base = FFmpeg::ToRaw(time_base);
		else
			ctx->time_base = ::AVRational{1, 1000};
	}

	if (ctx->pkt_timebase.num <= 0 || ctx->pkt_timebase.den <= 0)
		ctx->pkt_timebase = ctx->time_base;
	if (ctx->codec_type == AVMEDIA_TYPE_VIDEO
		&& (ctx->framerate.num <= 0 || ctx->framerate.den <= 0)
		&& ctx->time_base.num > 0 && ctx->time_base.den > 0) {
		ctx->framerate.num = ctx->time_base.den;
		ctx->framerate.den = ctx->time_base.num;
	}

	AVEncoder enc(ctx);
	enc.m_stream_index = stream_index;
	return enc;
}

FFmpeg::ExpectedAVEncoder FFmpeg::AVEncoder::Open(std::string_view codec_name, const AVCodecParameters& params, const FFmpeg::AVFormatContext& fmt, int stream_index) noexcept {
	auto opened = Open(codec_name, params, stream_index, {}, FFmpeg::AVRational{0, 1});
	if (!opened.has_value())
		return opened;
	auto bsf = fmt.Mp4ToAnnexB(params.CodecId(), stream_index, params);
	if (bsf)
		opened->m_bsf_pipeline.Add(std::move(*bsf));
	return opened;
}

FFmpeg::OperationResult FFmpeg::AVEncoder::SendFrame(AVFrame& frame) noexcept {
	if (m_ptr && m_ptr->codec_id == AV_CODEC_ID_HEVC) {
		auto t35 = T35FromFrame(frame);
		if (!t35.empty()) {
			const std::int64_t pts = frame.Pts();
			if (pts != AV_NOPTS_VALUE)
				m_hdrPlusT35.insert_or_assign(pts, Safe::Binary{std::move(t35)});
		}
	}

	int ret = avcodec_send_frame(m_ptr, frame.Get());
	switch (ret) {
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

FFmpeg::OperationResult FFmpeg::AVEncoder::EncodeSubtitle(AVSubtitle& sub, AVPacket& pkt) noexcept {
	if (!m_ptr || m_ptr->codec_type != AVMEDIA_TYPE_SUBTITLE)
		return OperationResult::Error;

	std::uint8_t buf[65536];
	const int n = avcodec_encode_subtitle(m_ptr, buf, static_cast<int>(sizeof(buf)), sub.Get());
	if (n < 0)
		return OperationResult::Error;
	if (n == 0)
		return OperationResult::TryAgain;
	if (!pkt.Load(buf, n, m_stream_index, false))
		return OperationResult::Error;

	const std::int64_t pts = sub.Pts();
	const ::AVRational tb = (m_ptr->time_base.num > 0) ? m_ptr->time_base : ::AVRational{1, AV_TIME_BASE};
	const std::int64_t duration = av_rescale_q(static_cast<std::int64_t>(sub.DisplayDurationMs()),
		::AVRational{1, 1000}, tb);
	const std::int64_t scaled = (pts == AV_NOPTS_VALUE)
		? AV_NOPTS_VALUE
		: av_rescale_q(pts, ::AVRational{1, AV_TIME_BASE}, tb);
	pkt.Timestamps(scaled, scaled, duration);
	return OperationResult::Success;
}

FFmpeg::OperationResult FFmpeg::AVEncoder::ReceivePacket(AVPacket& pkt) noexcept {
	AVPacket tmp;
	int ret = avcodec_receive_packet(m_ptr, tmp.Get());
	if (ret == AVERROR(EAGAIN))
		return OperationResult::TryAgain;
	if (ret == AVERROR_EOF) {
		m_bsf_pipeline.SetEof();
		if (!m_bsf_pipeline.Empty()) {
			const auto filtered = m_bsf_pipeline.Process(tmp);
			if (filtered == OperationResult::Success) {
				pkt = std::move(tmp);
				return OperationResult::Success;
			}

			if (filtered == OperationResult::TryAgain)
				return OperationResult::EndOfFile;
			return filtered;
		}

		return OperationResult::EndOfFile;
	}

	if (ret < 0)
		return OperationResult::Error;

	if (!m_bsf_pipeline.Empty()) {
		const auto filtered = m_bsf_pipeline.Process(tmp);
		if (filtered != OperationResult::Success)
			return filtered;
	}

	if (m_ptr && m_ptr->codec_id == AV_CODEC_ID_HEVC && tmp) {
		const std::int64_t pts = tmp.Pts();
		auto found = m_hdrPlusT35.find(pts);
		if (found != m_hdrPlusT35.end()) {
			std::vector<std::uint8_t> t35;
			t35.reserve(static_cast<std::size_t>(found->second.size()));
			for (const std::byte value : found->second)
				t35.push_back(std::to_integer<std::uint8_t>(value));
			if (!PrependHevcSei(tmp, t35))
				return OperationResult::Error;
			m_hdrPlusT35.erase(found);
		}
	}

	pkt = std::move(tmp);
	return OperationResult::Success;
}

int FFmpeg::AVEncoder::StreamIndex() const noexcept {
	return m_stream_index;
}

FFmpeg::AVRational FFmpeg::AVEncoder::TimeBase() const noexcept {
	if (!m_ptr)
		return FFmpeg::AVRational{0, 1};
	return FFmpeg::FromRaw(m_ptr->time_base);
}

bool FFmpeg::AVEncoder::IsSubtitle() const noexcept {
	return m_ptr && m_ptr->codec_type == AVMEDIA_TYPE_SUBTITLE;
}

void FFmpeg::AVEncoder::Flush() noexcept {
	avcodec_flush_buffers(m_ptr);
	m_bsf_pipeline.Flush();
	m_hdrPlusT35.clear();
}

FFmpeg::OperationResult FFmpeg::AVEncoder::SetEof() noexcept {
	if (!m_ptr || m_ptr->codec_type == AVMEDIA_TYPE_SUBTITLE) {
		m_bsf_pipeline.SetEof();
		return OperationResult::EndOfFile;
	}

	const int ret = avcodec_send_frame(m_ptr, nullptr);
	if (ret == 0 || ret == AVERROR_EOF) {
		m_bsf_pipeline.SetEof();
		return (ret == 0) ? OperationResult::Success : OperationResult::EndOfFile;
	}

	if (ret == AVERROR(EAGAIN))
		return OperationResult::TryAgain;
	return OperationResult::Error;
}

FFmpeg::AVEncoder::operator bool() const noexcept {
	return m_ptr != nullptr;
}

int FFmpeg::AVEncoder::CodecId() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->codec_id) : AV_CODEC_ID_NONE;
}

int FFmpeg::AVEncoder::CodecType() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->codec_type) : AVMEDIA_TYPE_UNKNOWN;
}

int FFmpeg::AVEncoder::SampleFmt() const noexcept {
	return m_ptr ? static_cast<int>(m_ptr->sample_fmt) : AV_SAMPLE_FMT_NONE;
}

int FFmpeg::AVEncoder::FrameSize() const noexcept {
	return m_ptr ? m_ptr->frame_size : 0;
}

int FFmpeg::AVEncoder::Channels() const noexcept {
	return m_ptr ? m_ptr->ch_layout.nb_channels : 0;
}

int FFmpeg::AVEncoder::SampleRate() const noexcept {
	return m_ptr ? m_ptr->sample_rate : 0;
}

FFmpeg::AVChannelLayout FFmpeg::AVEncoder::ChannelLayout() const noexcept {
	return m_ptr ? FFmpeg::FromRaw(m_ptr->ch_layout) : FFmpeg::AVChannelLayout{};
}

bool FFmpeg::AVEncoder::HasBFrames() const noexcept {
	return m_ptr && m_ptr->has_b_frames;
}

FFmpeg::AVRational FFmpeg::AVEncoder::FrameRate() const noexcept {
	return m_ptr ? FFmpeg::FromRaw(m_ptr->framerate) : FFmpeg::AVRational{0, 1};
}

const ::AVCodecContext* FFmpeg::AVEncoder::Context() const noexcept {
	return m_ptr;
}

void FFmpeg::AVEncoder::Free() noexcept {
	if (m_ptr) {
		avcodec_free_context(&m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVCodecContext>;
