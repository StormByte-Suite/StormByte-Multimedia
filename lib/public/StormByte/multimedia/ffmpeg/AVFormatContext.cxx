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

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/multimedia/ffmpeg/AVBSF.hxx>
#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVDecoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVStream.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/mem.h>
}

#include <StormByte/multimedia/ffmpeg/convert.hxx>

using namespace StormByte::Multimedia;
using StormByte::Buffer::Consumer;
using StormByte::BinaryData;
using StormByte::Buffer::Position;

namespace {
	void AddParSideData(::AVCodecParameters* par, enum AVPacketSideDataType type,
		const uint8_t* data, size_t size) noexcept {
		if (!par || !data || size == 0)
			return;
		if (av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data, type))
			return;
		AVPacketSideData* sd = av_packet_side_data_new(
			&par->coded_side_data, &par->nb_coded_side_data, type, size, 0);
		if (sd && sd->data)
			std::memcpy(sd->data, data, size);
	}

	void PromotePacket(const FFmpeg::AVPacket& pkt, ::AVCodecParameters* par) noexcept {
		const int n = pkt.SideDataCount();
		for (int i = 0; i < n; ++i) {
			int size = 0;
			const std::uint8_t* data = pkt.SideData(i, size);
			if (!data || size <= 0)
				continue;
			const int type = pkt.SideDataType(i);
			if (type == AV_PKT_DATA_MASTERING_DISPLAY_METADATA
				|| type == AV_PKT_DATA_CONTENT_LIGHT_LEVEL
				|| type == AV_PKT_DATA_DYNAMIC_HDR10_PLUS)
				AddParSideData(par, static_cast<AVPacketSideDataType>(type),
					data, static_cast<std::size_t>(size));
		}
	}

	void PromoteFrame(const FFmpeg::AVFrame& frame, ::AVCodecParameters* par) noexcept {
		if (const AVFrameSideData* sd = frame.SideData(AV_FRAME_DATA_MASTERING_DISPLAY_METADATA))
			AddParSideData(par, AV_PKT_DATA_MASTERING_DISPLAY_METADATA, sd->data, sd->size);
		if (const AVFrameSideData* sd = frame.SideData(AV_FRAME_DATA_CONTENT_LIGHT_LEVEL))
			AddParSideData(par, AV_PKT_DATA_CONTENT_LIGHT_LEVEL, sd->data, sd->size);
		if (const AVFrameSideData* sd = frame.SideData(AV_FRAME_DATA_DYNAMIC_HDR_PLUS))
			AddParSideData(par, AV_PKT_DATA_DYNAMIC_HDR10_PLUS, sd->data, sd->size);
	}

	bool VideoHasMastering(::AVCodecParameters* par) noexcept {
		return par && av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data,
			AV_PKT_DATA_MASTERING_DISPLAY_METADATA);
	}
}

struct FFmpeg::AVFormatContext::ConsumerIO {
	Consumer consumer;
	std::int64_t position = 0;

	static int Read(void* opaque, std::uint8_t* buf, int bufSize) noexcept {
		auto* io = static_cast<ConsumerIO*>(opaque);
		if (!io->consumer.IsReadable() || io->consumer.EoF())
			return AVERROR_EOF;
		const std::size_t avail = io->consumer.Available();
		if (avail == 0)
			return AVERROR_EOF;
		const std::size_t want = std::min(avail, static_cast<std::size_t>(bufSize));
		BinaryData chunk;
		if (!io->consumer.Extract(want, chunk) || chunk.empty())
			return AVERROR_EOF;
		std::memcpy(buf, chunk.data(), chunk.size());
		io->position += static_cast<std::int64_t>(chunk.size());
		return static_cast<int>(chunk.size());
	}

	static std::int64_t Seek(void* opaque, std::int64_t offset, int whence) noexcept {
		auto* io = static_cast<ConsumerIO*>(opaque);
		(void)offset;
		if (whence == AVSEEK_SIZE)
			return static_cast<std::int64_t>(io->consumer.Size());
		return AVERROR(ESPIPE);
	}
};

FFmpeg::AVFormatContext::AVFormatContext(::AVFormatContext* ctx, std::unique_ptr<ConsumerIO> io,
	bool avioBorrowed) noexcept
: AVPointer(ctx), m_io(std::move(io)), m_avioBorrowed(avioBorrowed) {}

FFmpeg::AVFormatContext FFmpeg::AVFormatContext::WrapBorrowed(::AVFormatContext* ctx) noexcept {
	return WrapBorrowed(ctx, true);
}

FFmpeg::AVFormatContext FFmpeg::AVFormatContext::WrapBorrowed(::AVFormatContext* ctx, bool harvest) noexcept {
	AVFormatContext out(ctx, nullptr, true);
	if (harvest)
		out.HarvestSideData();
	else if (ctx)
		av_seek_frame(ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
	return out;
}

FFmpeg::AVFormatContext::AVFormatContext(AVFormatContext&& other) noexcept
: AVPointer(std::move(other)), m_io(std::move(other.m_io)), m_avioBorrowed(other.m_avioBorrowed) {
	other.m_avioBorrowed = false;
}

FFmpeg::AVFormatContext::~AVFormatContext() noexcept {
	Free();
}

FFmpeg::AVFormatContext& FFmpeg::AVFormatContext::operator=(AVFormatContext&& other) noexcept {
	if (this != &other) {
		AVPointer::operator=(std::move(other));
		m_io = std::move(other.m_io);
		m_avioBorrowed = other.m_avioBorrowed;
		other.m_avioBorrowed = false;
	}

	return *this;
}

FFmpeg::ExpectedAVFormatContext FFmpeg::AVFormatContext::Open(const std::filesystem::path& path) {
	::AVFormatContext* raw_ctx = nullptr;
	int ret;

	av_log_set_level(AV_LOG_ERROR);

	if ((ret = avformat_open_input(&raw_ctx, path.string().c_str(), nullptr, nullptr)) < 0)
		return Unexpected<DecoderError>("Could not open file {}: {}", path.string(), ErrorToString(ret));

	::AVFormatContext* fmt_ctx = raw_ctx;

	if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0) {
		avformat_close_input(&fmt_ctx);
		return Unexpected<DecoderError>("Could not find stream information: {}", ErrorToString(ret));
	}

	AVFormatContext ctx(fmt_ctx, nullptr);
	ctx.HarvestSideData();
	return ctx;
}

FFmpeg::ExpectedAVFormatContext FFmpeg::AVFormatContext::Open(Consumer consumer) {
	av_log_set_level(AV_LOG_ERROR);

	auto io = std::make_unique<ConsumerIO>(ConsumerIO{std::move(consumer), 0});
	io->consumer.Clean();
	io->position = 0;

	constexpr int ioSize = 4096;
	auto* ioBuf = static_cast<unsigned char*>(av_malloc(ioSize));
	if (!ioBuf)
		return Unexpected<DecoderError>("Could not allocate AVIO buffer");

	AVIOContext* avio = avio_alloc_context(ioBuf, ioSize, 0, io.get(),
		&ConsumerIO::Read, nullptr, &ConsumerIO::Seek);
	if (!avio) {
		av_free(ioBuf);
		return Unexpected<DecoderError>("Could not allocate AVIO context");
	}

	::AVFormatContext* fmt_ctx = avformat_alloc_context();
	if (!fmt_ctx) {
		av_free(avio->buffer);
		avio_context_free(&avio);
		return Unexpected<DecoderError>("Could not allocate format context");
	}

	fmt_ctx->pb = avio;
	fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

	int ret = avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr);
	if (ret < 0) {
		av_free(fmt_ctx->pb->buffer);
		avio_context_free(&fmt_ctx->pb);
		avformat_free_context(fmt_ctx);
		return Unexpected<DecoderError>("Could not open buffer: {}", ErrorToString(ret));
	}

	if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0) {
		AVIOContext* pb = fmt_ctx->pb;
		fmt_ctx->pb = nullptr;
		avformat_close_input(&fmt_ctx);
		if (pb) {
			av_free(pb->buffer);
			avio_context_free(&pb);
		}

		return Unexpected<DecoderError>("Could not find stream information: {}", ErrorToString(ret));
	}

	io->consumer.Clean();
	io->position = 0;
	AVFormatContext ctx(fmt_ctx, std::move(io));
	ctx.HarvestSideData();
	if (ctx.m_io) {
		ctx.m_io->consumer.Clean();
		ctx.m_io->position = 0;
	}
	return ctx;
}

void FFmpeg::AVFormatContext::HarvestSideData() noexcept {
	if (!m_ptr)
		return;

	constexpr int kMaxPackets = 128;
	AVPacket packet;
	AVFrame frame;
	std::vector<std::unique_ptr<AVDecoder>> decoders(m_ptr->nb_streams);

	for (unsigned i = 0; i < m_ptr->nb_streams; ++i) {
		::AVCodecParameters* par = m_ptr->streams[i]->codecpar;
		if (!par || par->codec_type != AVMEDIA_TYPE_VIDEO)
			continue;
		const AVCodec* codec = avcodec_find_decoder(par->codec_id);
		if (!codec)
			continue;
		auto opened = AVDecoder::Open(const_cast<AVCodec*>(codec),
			AVCodecParameters(par), *this, static_cast<int>(i));
		if (opened.has_value())
			decoders[i] = std::make_unique<AVDecoder>(std::move(opened.value()));
	}

	for (int n = 0; n < kMaxPackets; ++n) {
		if (ReadPacket(packet) != OperationResult::Success)
			break;

		const int idx = packet.StreamIndex();
		if (idx < 0 || static_cast<unsigned>(idx) >= m_ptr->nb_streams) {
			packet.Unref();
			continue;
		}

		::AVCodecParameters* par = m_ptr->streams[static_cast<unsigned>(idx)]->codecpar;
		PromotePacket(packet, par);

		if (decoders[static_cast<unsigned>(idx)]) {
			if (decoders[static_cast<unsigned>(idx)]->SendPacket(packet) == OperationResult::Success) {
				while (decoders[static_cast<unsigned>(idx)]->ReceiveFrame(frame) == OperationResult::Success) {
					PromoteFrame(frame, par);
					frame.Unref();
				}
			}
		}

		packet.Unref();

		bool done = true;
		for (unsigned i = 0; i < m_ptr->nb_streams; ++i) {
			if (decoders[i] && !VideoHasMastering(m_ptr->streams[i]->codecpar)) {
				done = false;
				break;
			}
		}

		if (done)
			break;
	}

	av_seek_frame(m_ptr, -1, 0, AVSEEK_FLAG_BACKWARD);
	if (m_io) {
		m_io->consumer.Clean();
		m_io->position = 0;
	}
}

const char* FFmpeg::AVFormatContext::FormatName() const noexcept {
	if (!m_ptr || !m_ptr->iformat)
		return nullptr;
	return m_ptr->iformat->name;
}

const char* FFmpeg::AVFormatContext::Tag(const char* key) const noexcept {
	if (!m_ptr || !m_ptr->metadata || !key)
		return nullptr;
	const AVDictionaryEntry* entry = av_dict_get(m_ptr->metadata, key, nullptr, 0);
	return entry ? entry->value : nullptr;
}

std::optional<std::chrono::nanoseconds> FFmpeg::AVFormatContext::Duration() const noexcept {
	if (!m_ptr || m_ptr->duration == AV_NOPTS_VALUE)
		return std::nullopt;
	const std::int64_t ns = av_rescale_q(m_ptr->duration, ::AVRational{1, AV_TIME_BASE}, ::AVRational{1, 1000000000});
	if (ns < 0)
		return std::nullopt;
	return std::chrono::nanoseconds{ns};
}

FFmpeg::OperationResult FFmpeg::AVFormatContext::ReadPacket(AVPacket& packet) noexcept {
	packet.Unref();
	int ret = av_read_frame(m_ptr, packet.Get());
	switch(ret) {
		case AVERROR(EAGAIN):
			return OperationResult::TryAgain;
		case AVERROR_EOF:
			return OperationResult::EndOfFile;
		case 0:
			return OperationResult::Success;
		default:
			return OperationResult::Error;
	}
}

FFmpeg::OperationResult FFmpeg::AVFormatContext::SeekStart() noexcept {
	if (!m_ptr)
		return OperationResult::Error;
	if (m_io) {
		m_io->consumer.Clean();
		m_io->position = 0;
	}
	const int ret = av_seek_frame(m_ptr, -1, 0, AVSEEK_FLAG_BACKWARD);
	if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(ESPIPE)) {
		if (!m_io)
			return OperationResult::Error;
	}
	avformat_flush(m_ptr);
	return OperationResult::Success;
}

FFmpeg::Streams FFmpeg::AVFormatContext::Streams() const noexcept {
	FFmpeg::Streams out;

	if (!m_ptr || m_ptr->nb_streams == 0)
		return out;

	for (unsigned i = 0; i < m_ptr->nb_streams; ++i)
		out.emplace(AVStream(m_ptr->streams[i]));

	return out;
}

std::optional<FFmpeg::AVBSF> FFmpeg::AVFormatContext::Mp4ToAnnexB(int codec_id, int stream_index, const AVCodecParameters& params) const noexcept {
	if (!m_ptr || !m_ptr->iformat || !m_ptr->iformat->name)
		return std::nullopt;

	const std::string fmt_name = m_ptr->iformat->name;

	bool is_mp4_like =
		fmt_name.find("mp4") != std::string::npos ||
		fmt_name.find("isom") != std::string::npos ||
		fmt_name.find("mov") != std::string::npos;

	if (!is_mp4_like)
		return std::nullopt;

	std::string bsf_name;
	switch (codec_id) {
		case AV_CODEC_ID_HEVC: bsf_name = "hevc_mp4toannexb"; break;
		case AV_CODEC_ID_H264: bsf_name = "h264_mp4toannexb"; break;
		case AV_CODEC_ID_AV1:  bsf_name = "av1_mp4toannexb"; break;
		default:               return std::nullopt;
	}

	auto expected_bsf = FFmpeg::AVBSF::Create(
		bsf_name,
		params,
		FFmpeg::FromRaw(m_ptr->streams[stream_index]->time_base)
	);

	if (expected_bsf)
		return std::move(expected_bsf.value());
	return std::nullopt;
}

FFmpeg::AVFormatContext::operator bool() const noexcept {
	return m_ptr != nullptr;
}

void FFmpeg::AVFormatContext::DiscardUnwanted(const std::unordered_set<int>& wanted) noexcept {
	if (!m_ptr)
		return;
	for (unsigned i = 0; i < m_ptr->nb_streams; ++i) {
		::AVStream* avs = m_ptr->streams[i];
		if (!avs)
			continue;
		avs->discard = wanted.contains(avs->index) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
	}
}

void FFmpeg::AVFormatContext::Free() noexcept {
	if (!m_ptr)
		return;

	if (m_avioBorrowed && m_ptr->pb)
		m_ptr->pb = nullptr;

	AVIOContext* pb = m_io ? m_ptr->pb : nullptr;
	if (m_io)
		m_ptr->pb = nullptr;
	avformat_close_input(&m_ptr);
	if (pb) {
		av_free(pb->buffer);
		avio_context_free(&pb);
	}

	m_io.reset();
	m_avioBorrowed = false;
	m_ptr = nullptr;
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVFormatContext>;
