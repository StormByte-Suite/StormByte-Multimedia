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

#include <StormByte/multimedia/ffmpeg/AVCodecParameters.hxx>
#include <StormByte/multimedia/ffmpeg/AVEncoder.hxx>
#include <StormByte/multimedia/ffmpeg/AVFormatContext.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/AVPacket.hxx>
#include <StormByte/multimedia/ffmpeg/AVSubtitle.hxx>
#include <StormByte/multimedia/ffmpeg/convert.hxx>
#include <StormByte/multimedia/ffmpeg/video_input.hxx>
#include <StormByte/multimedia/property/dovi.hxx>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/error.h>
	#include <libavutil/dovi_meta.h>
	#include <libavutil/frame.h>
	#include <libavutil/hdr_dynamic_metadata.h>
	#include <libavutil/dict.h>
	#include <libavutil/hwcontext.h>
	#include <libavutil/log.h>
	#include <libavutil/mem.h>
	#include <libavutil/opt.h>
}

using namespace StormByte::Multimedia;

namespace {
	// Wrappers such as libx264, libx265 and libsvtav1 only warn about unknown keys or
	// invalid values inside their packed parameter dictionaries, and that depends on the
	// FFmpeg build (system or bundled). Capture what the encoder logs while it opens so a
	// rejected parameter fails instead of being silently ignored.
	struct OpenCapture {
		const void* context = nullptr;
		const void* priv = nullptr;
		std::vector<std::string> lines;
	};

	thread_local OpenCapture* t_capture = nullptr;

	void CaptureLog(void* avcl, int level, const char* fmt, va_list vl) {
		if (t_capture && level <= AV_LOG_WARNING && avcl
			&& (avcl == t_capture->context || avcl == t_capture->priv)) {
			va_list copy;
			va_copy(copy, vl);
			char line[1024];
			std::vsnprintf(line, sizeof(line), fmt, copy);
			va_end(copy);
			try {
				t_capture->lines.emplace_back(line);
			}
			catch (...) {}
		}
		av_log_default_callback(avcl, level, fmt, vl);
	}

	void InstallCapture() noexcept {
		static std::once_flag once;
		std::call_once(once, [] { av_log_set_callback(CaptureLog); });
	}

	bool IsKeyChar(char c) noexcept {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
	}

	bool MentionsKey(std::string_view line, std::string_view key) noexcept {
		for (auto at = line.find(key); at != std::string_view::npos; at = line.find(key, at + 1)) {
			const bool before = at == 0 || !IsKeyChar(line[at - 1]);
			const bool after = at + key.size() >= line.size() || !IsKeyChar(line[at + key.size()]);
			if (before && after)
				return true;
		}
		return false;
	}

	// Keys of every dictionary-typed option (x265-params, x264-params, svtav1-params, ...).
	std::set<std::string> PackedKeys(AVCodecContext* ctx,
		const StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>& options) {
		std::set<std::string> keys;
		for (const auto& [key, value] : options) {
			if (key.empty() || value.empty())
				continue;
			const auto* option = av_opt_find(ctx, key.data(), nullptr, 0, AV_OPT_SEARCH_CHILDREN);
			if (!option || option->type != AV_OPT_TYPE_DICT)
				continue;
			AVDictionary* parsed = nullptr;
			if (av_dict_parse_string(&parsed, value.data(), "=", ":", 0) >= 0) {
				const AVDictionaryEntry* entry = nullptr;
				while ((entry = av_dict_iterate(parsed, entry)))
					keys.emplace(entry->key);
			}
			av_dict_free(&parsed);
		}
		return keys;
	}

	bool ValidDoviMetadata(const AVFrameSideData* side) noexcept {
		if (!side || !side->data || side->size < sizeof(AVDOVIMetadata))
			return false;

		Property::DOVI validated;
		return validated.LoadMetadata(std::as_bytes(std::span{side->data, side->size}));
	}

	bool HasEnhancementLayer(const AVDOVIMetadata* metadata) noexcept {
		const auto* header = av_dovi_get_header(metadata);
		const auto* mapping = av_dovi_get_mapping(metadata);
		return !header->disable_residual_flag || header->el_spatial_resampling_filter_flag
			|| mapping->nlq_method_idc != AV_DOVI_NLQ_NONE;
	}

	// Same rewrite as dovi_tool mode 2 (profile 7 -> 8.1): the enhancement layer cannot follow a
	// re-encoded base layer, so the RPU is marked single-layer and its dynamic metadata kept.
	void ToSingleLayer(AVDOVIMetadata* metadata) noexcept {
		auto* header = av_dovi_get_header(metadata);
		auto* mapping = av_dovi_get_mapping(metadata);
		auto* color = av_dovi_get_color(metadata);
		const std::int64_t one = std::int64_t{1} << header->coef_log2_denom;
		bool mel = mapping->nlq_method_idc != AV_DOVI_NLQ_NONE;
		for (const auto& nlq : mapping->nlq)
			mel = mel && nlq.nlq_offset == 0 && nlq.vdr_in_max == static_cast<std::uint64_t>(one)
				&& nlq.linear_deadzone_slope == 0 && nlq.linear_deadzone_threshold == 0;
		const bool fel = mapping->nlq_method_idc != AV_DOVI_NLQ_NONE && !mel;

		header->el_spatial_resampling_filter_flag = 0;
		header->disable_residual_flag = 1;
		mapping->nlq_method_idc = AV_DOVI_NLQ_NONE;
		mapping->num_x_partitions = 1;
		mapping->num_y_partitions = 1;
		std::memset(mapping->nlq, 0, sizeof(mapping->nlq));
		std::memset(mapping->nlq_pivots, 0, sizeof(mapping->nlq_pivots));

		// FEL reshaping only makes sense together with its residual: use the identity curve.
		if (fel) {
			for (auto& curve : mapping->curves) {
				curve = {};
				curve.num_pivots = 2;
				curve.pivots[0] = 0;
				curve.pivots[1] = static_cast<std::uint16_t>((1u << header->bl_bit_depth) - 1);
				curve.mapping_idc[0] = AV_DOVI_MAPPING_POLYNOMIAL;
				curve.poly_order[0] = 1;
				curve.poly_coef[0][1] = one;
			}
		}

		constexpr int YccToRgb[9] = {9574, 0, 13802, 9574, -1540, -5348, 9574, 17610, 0};
		constexpr int RgbToLms[9] = {7222, 8771, 390, 2654, 12430, 1300, 0, 422, 15962};
		for (int i = 0; i < 9; ++i) {
			color->ycc_to_rgb_matrix[i] = av_make_q(YccToRgb[i], 1 << 13);
			color->rgb_to_lms_matrix[i] = av_make_q(RgbToLms[i], 1 << 14);
		}
		color->ycc_to_rgb_offset[0] = av_make_q(1, 16);
		color->ycc_to_rgb_offset[1] = av_make_q(1, 2);
		color->ycc_to_rgb_offset[2] = av_make_q(1, 2);
		color->signal_color_space = 0;
	}

	AVBufferRef* SingleLayerCopy(const AVFrameSideData* side) noexcept {
		AVBufferRef* copy = av_buffer_alloc(side->size);
		if (!copy)
			return nullptr;
		std::memcpy(copy->data, side->data, side->size);
		ToSingleLayer(reinterpret_cast<AVDOVIMetadata*>(copy->data));
		return copy;
	}

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

	constexpr const char TtmlAssHeader[] =
		"[Script Info]\n"
		"ScriptType: v4.00+\n"
		"PlayResX: 384\n"
		"PlayResY: 288\n"
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

	void PushLeb128(std::vector<std::uint8_t>& out, std::size_t value) noexcept {
		do {
			std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7F);
			value >>= 7;
			if (value)
				byte |= 0x80;
			out.push_back(byte);
		} while (value);
	}

	bool ReadLeb128(const std::uint8_t* data, std::size_t size, std::size_t& pos, std::uint64_t& value) noexcept {
		value = 0;
		for (int i = 0; i < 8; ++i) {
			if (pos >= size)
				return false;
			const std::uint8_t byte = data[pos++];
			value |= static_cast<std::uint64_t>(byte & 0x7F) << (7 * i);
			if (!(byte & 0x80))
				return true;
		}
		return false;
	}

	// Inserts an ITU-T T.35 metadata OBU after the temporal delimiter and any
	// sequence header, which is where AV1 requires metadata to precede frames.
	bool InsertAv1Metadata(FFmpeg::AVPacket& pkt, const std::vector<std::uint8_t>& t35) noexcept {
		const auto* src = pkt.Data();
		const int srcSize = pkt.Size();
		if (!src || srcSize <= 0 || t35.empty())
			return true;
		const auto size = static_cast<std::size_t>(srcSize);
		std::size_t insert = 0;
		while (insert < size) {
			std::size_t pos = insert;
			const std::uint8_t header = src[pos++];
			const int type = (header >> 3) & 0x0F;
			if (type != 1 && type != 2)
				break;
			if (header & 0x04)
				++pos;
			std::uint64_t length = 0;
			if (!(header & 0x02) || !ReadLeb128(src, size, pos, length) || length > size - std::min(pos, size))
				return true;
			insert = pos + static_cast<std::size_t>(length);
		}

		std::vector<std::uint8_t> payload;
		PushLeb128(payload, 4);
		payload.insert(payload.end(), t35.begin(), t35.end());
		payload.push_back(0x80);
		std::vector<std::uint8_t> out(src, src + insert);
		out.push_back(0x2A);
		PushLeb128(out, payload.size());
		out.insert(out.end(), payload.begin(), payload.end());
		out.insert(out.end(), src + insert, src + size);

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

	// libaom forwards HDR10+ itself; every other HEVC/AV1 encoder relies on us.
	bool InjectsHdrPlus(const ::AVCodecContext* ctx) noexcept {
		if (!ctx)
			return false;
		if (ctx->codec_id == AV_CODEC_ID_HEVC)
			return true;
		return ctx->codec_id == AV_CODEC_ID_AV1
			&& !(ctx->codec && ctx->codec->name && std::string_view{ctx->codec->name} == "libaom-av1");
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
	const StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>& options, FFmpeg::AVRational time_base,
	const AVFrame* firstFrame, int thread_count) noexcept {
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
	if (firstFrame) {
		for (const auto type : {AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL}) {
			if (av_frame_side_data_get(ctx->decoded_side_data, ctx->nb_decoded_side_data, type))
				continue;
			const auto* metadata = firstFrame->SideData(type);
			if (metadata && av_frame_side_data_clone(&ctx->decoded_side_data,
				&ctx->nb_decoded_side_data, metadata, 0) < 0) {
				avcodec_free_context(&ctx);
				return Unexpected<FFmpeg::EncoderError>("failed to preserve first-frame HDR10 metadata");
			}
		}
	}
	const auto* dovi = av_packet_side_data_get(ctx->coded_side_data,
		ctx->nb_coded_side_data, AV_PKT_DATA_DOVI_CONF);
	const bool needsDovi = dovi && dovi->data && dovi->size >= sizeof(AVDOVIDecoderConfigurationRecord)
		&& reinterpret_cast<const AVDOVIDecoderConfigurationRecord*>(dovi->data)->rpu_present_flag;
	if (needsDovi) {
		const auto* metadata = firstFrame ? firstFrame->SideData(AV_FRAME_DATA_DOVI_METADATA) : nullptr;
		if (!ValidDoviMetadata(metadata)) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("native Dolby Vision encoding requires parsed frame metadata");
		}
		const bool layered = HasEnhancementLayer(reinterpret_cast<const AVDOVIMetadata*>(metadata->data));
		AVBufferRef* single = layered ? SingleLayerCopy(metadata) : nullptr;
		const bool kept = layered
			? single && av_frame_side_data_add(&ctx->decoded_side_data, &ctx->nb_decoded_side_data,
				AV_FRAME_DATA_DOVI_METADATA, &single, 0)
			: av_frame_side_data_clone(&ctx->decoded_side_data, &ctx->nb_decoded_side_data, metadata, 0) >= 0;
		av_buffer_unref(&single);
		if (!kept) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("failed to preserve parsed Dolby Vision encoder metadata");
		}
		if (!ctx->priv_data || !av_opt_find(ctx->priv_data, "dolbyvision", nullptr, 0, 0)) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("encoder '{}' does not support native Dolby Vision", codec_name);
		}
	}

	ctx->thread_count = thread_count;
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
		const bool ttml = codec->name && std::string_view{codec->name} == "ttml";
		const char* header = ttml ? TtmlAssHeader : DefaultAssHeader;
		const auto n = std::strlen(header);
		ctx->subtitle_header = static_cast<std::uint8_t*>(av_malloc(n + 1));
		if (!ctx->subtitle_header) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("Out of memory allocating subtitle header");
		}

		std::memcpy(ctx->subtitle_header, header, n + 1);
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

	if (needsDovi && av_opt_set(ctx, "dolbyvision", "1", AV_OPT_SEARCH_CHILDREN) < 0) {
		avcodec_free_context(&ctx);
		return Unexpected<FFmpeg::EncoderError>("failed to enable native Dolby Vision encoding");
	}

	if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		std::string error;
		if (!VideoInput::Configure(ctx, codec, firstFrame ? firstFrame->Get() : nullptr, error)) {
			avcodec_free_context(&ctx);
			return Unexpected<FFmpeg::EncoderError>("encoder '{}' input: {}", codec_name, error);
		}
	}

	std::set<std::string> packed;
	try {
		packed = PackedKeys(ctx, options);
	}
	catch (...) {
		avcodec_free_context(&ctx);
		return Unexpected<FFmpeg::EncoderError>("Out of memory reading encoder parameters");
	}
	OpenCapture capture{ctx, ctx->priv_data, {}};
	if (!packed.empty()) {
		InstallCapture();
		t_capture = &capture;
	}
	const int opened = avcodec_open2(ctx, codec, nullptr);
	t_capture = nullptr;
	if (opened < 0) {
		const std::string why = AvError(opened);
		avcodec_free_context(&ctx);
		return Unexpected<FFmpeg::EncoderError>("Failed to open encoder: " + why);
	}
	for (const auto& line : capture.lines) {
		for (const auto& key : packed) {
			if (MentionsKey(line, key)) {
				avcodec_free_context(&ctx);
				std::string text = line;
				while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
					text.pop_back();
				return Unexpected<FFmpeg::EncoderError>("encoder '{}' rejected parameter '{}': {}", codec_name, key, text);
			}
		}
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

bool FFmpeg::AVEncoder::PrepareFrame(const AVFrame& source, AVFrame& destination) const noexcept {
	if (!m_ptr || m_ptr->codec_type != AVMEDIA_TYPE_VIDEO || &source == &destination
		|| !source || !destination || source.Width() != m_ptr->width || source.Height() != m_ptr->height)
		return false;
	const auto* sourceFrames = VideoInput::Frames(source.Get());
	if (VideoInput::Hardware(source.Format()) && !sourceFrames)
		return false;
	if (sourceFrames && m_ptr->hw_frames_ctx
		&& source.Get()->hw_frames_ctx->data == m_ptr->hw_frames_ctx->data
		&& source.Format() == m_ptr->pix_fmt)
		return destination.Ref(source);
	AVFrame downloaded;
	const AVFrame* input = &source;
	if (sourceFrames) {
		if (!VideoInput::Download(source.Get(), downloaded.Get()))
			return false;
		input = &downloaded;
	}
	if (!VideoInput::Hardware(m_ptr->pix_fmt))
		return VideoInput::Prepare(*input, m_ptr->pix_fmt, destination);
	if (!m_ptr->hw_frames_ctx || !m_ptr->hw_frames_ctx->data)
		return false;
	const auto* pool = reinterpret_cast<const AVHWFramesContext*>(m_ptr->hw_frames_ctx->data);
	AVFrame upload;
	if (!VideoInput::Prepare(*input, pool->sw_format, upload))
		return false;
	return VideoInput::Upload(m_ptr, upload.Get(), destination.Get());
}

FFmpeg::OperationResult FFmpeg::AVEncoder::SendFrame(AVFrame& frame) noexcept {
	if (m_ptr && frame.Get()) {
		const auto* configuration = av_packet_side_data_get(m_ptr->coded_side_data,
			m_ptr->nb_coded_side_data, AV_PKT_DATA_DOVI_CONF);
		const bool configured = configuration && configuration->data
			&& configuration->size >= sizeof(AVDOVIDecoderConfigurationRecord)
			&& reinterpret_cast<const AVDOVIDecoderConfigurationRecord*>(configuration->data)->rpu_present_flag;
		const auto* parsed = frame.SideData(AV_FRAME_DATA_DOVI_METADATA);
		if (parsed && !ValidDoviMetadata(parsed))
			return OperationResult::Error;
		const bool metadata = parsed != nullptr;
		if ((configured && !metadata) || (!configured && (metadata || frame.SideData(AV_FRAME_DATA_DOVI_RPU_BUFFER))))
			return OperationResult::Error;
		auto* side = av_frame_get_side_data(frame.Get(), AV_FRAME_DATA_DOVI_METADATA);
		if (side && HasEnhancementLayer(reinterpret_cast<const AVDOVIMetadata*>(side->data))) {
			if (av_buffer_make_writable(&side->buf) < 0)
				return OperationResult::Error;
			side->data = side->buf->data;
			ToSingleLayer(reinterpret_cast<AVDOVIMetadata*>(side->data));
		}
	}
	if (InjectsHdrPlus(m_ptr)) {
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

	if (InjectsHdrPlus(m_ptr) && tmp) {
		const std::int64_t pts = tmp.Pts();
		auto found = m_hdrPlusT35.find(pts);
		if (found != m_hdrPlusT35.end()) {
			std::vector<std::uint8_t> t35;
			t35.reserve(static_cast<std::size_t>(found->second.size()));
			for (const std::byte value : found->second)
				t35.push_back(std::to_integer<std::uint8_t>(value));
			const bool injected = m_ptr->codec_id == AV_CODEC_ID_HEVC
				? PrependHevcSei(tmp, t35) : InsertAv1Metadata(tmp, t35);
			if (!injected)
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
		// Cancellation skips the pipeline flush. SVT still requires EOS before
		// deinit; its close routine drains the remaining encoder work itself.
		if (avcodec_is_open(m_ptr) && m_ptr->codec
			&& std::strcmp(m_ptr->codec->name, "libsvtav1") == 0) {
			int result = avcodec_send_frame(m_ptr, nullptr);
			if (result == AVERROR(EAGAIN)) {
				auto* packet = av_packet_alloc();
				if (!packet)
					av_log(m_ptr, AV_LOG_ERROR, "Cannot allocate a packet while closing SVT-AV1.\n");
				else {
					while (result == AVERROR(EAGAIN)) {
						const int received = avcodec_receive_packet(m_ptr, packet);
						av_packet_unref(packet);
						if (received < 0) {
							av_log(m_ptr, AV_LOG_ERROR, "Cannot drain SVT-AV1 before EOS: %d.\n", received);
							break;
						}
						result = avcodec_send_frame(m_ptr, nullptr);
					}
					av_packet_free(&packet);
				}
			}
			if (result < 0 && result != AVERROR_EOF)
				av_log(m_ptr, AV_LOG_ERROR, "Cannot signal EOS while closing SVT-AV1: %d.\n", result);
		}
		avcodec_free_context(&m_ptr);
		m_ptr = nullptr;
	}
}

template class StormByte::Multimedia::FFmpeg::AVPointer<::AVCodecContext>;
