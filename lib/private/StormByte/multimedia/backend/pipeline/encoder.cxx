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
#include <StormByte/multimedia/backend/pipeline/encoder.hxx>
#include <StormByte/multimedia/backend/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/item.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <tables/encoder/table.hxx>

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavutil/avutil.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/rational.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Backend::Pipeline;
namespace FFmpeg = StormByte::Multimedia::FFmpeg;
using StormByte::Multimedia::Pipeline::Producer;
using StormByte::Multimedia::Pipeline::SideData;
using StormByte::Multimedia::Pipeline::SideDataKind;

namespace {
	std::optional<Property::Duration> TicksToPts(std::int64_t ticks, FFmpeg::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || !timeBase.Valid())
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, FFmpeg::Nanosecond);
		if (ns < 0)
			return std::nullopt;
		return Property::Duration{std::chrono::nanoseconds{ns}};
	}

	std::optional<Property::Duration> TicksToDuration(std::int64_t ticks, FFmpeg::AVRational timeBase) noexcept {
		if (ticks == AV_NOPTS_VALUE || ticks <= 0 || !timeBase.Valid())
			return std::nullopt;
		const std::int64_t ns = timeBase.Rescale(ticks, FFmpeg::Nanosecond);
		if (ns <= 0)
			return std::nullopt;
		return Property::Duration{std::chrono::nanoseconds{ns}};
	}

	const Tables::Encoder::EncoderDef* ScanRows(
		std::span<const Tables::Encoder::EncoderDef> rows,
		std::string_view codec, std::string_view pin, bool matchNeed,
		const Features& need) noexcept {
		const Tables::Encoder::EncoderDef* best = nullptr;
		for (const auto& row : rows) {
			if (codec != row.codec)
				continue;
			if (!pin.empty() && pin != row.name)
				continue;
			if (matchNeed && !row.features.Has(need))
				continue;
			if (matchNeed && avcodec_find_encoder_by_name(row.name) == nullptr)
				continue;
			if (!matchNeed)
				return &row;
			if (!best || row.preference < best->preference)
				best = &row;
		}

		return best;
	}

	const Tables::Encoder::EncoderDef* FindRow(std::string_view codec, std::string_view pin) noexcept {
		if (const auto* row = ScanRows(Tables::Encoder::Video(), codec, pin, false, {}))
			return row;
		if (const auto* row = ScanRows(Tables::Encoder::Audio(), codec, pin, false, {}))
			return row;
		return ScanRows(Tables::Encoder::Subtitle(), codec, pin, false, {});
	}

	const Tables::Encoder::EncoderDef* PickEncoder(
		std::string_view codec, std::string_view pin, const Features& need) noexcept {
		if (const auto* row = ScanRows(Tables::Encoder::Video(), codec, pin, true, need))
			return row;
		if (const auto* row = ScanRows(Tables::Encoder::Audio(), codec, pin, true, need))
			return row;
		return ScanRows(Tables::Encoder::Subtitle(), codec, pin, true, need);
	}

	std::vector<std::pair<std::string, std::string>> SplitBlob(std::string_view blob) noexcept {
		std::vector<std::pair<std::string, std::string>> out;
		std::string_view rest = blob;
		while (!rest.empty()) {
			const auto cut = rest.find(':');
			const auto piece = rest.substr(0, cut);
			const auto eq = piece.find('=');
			if (eq != std::string_view::npos && eq > 0)
				out.emplace_back(std::string(piece.substr(0, eq)), std::string(piece.substr(eq + 1)));
			if (cut == std::string_view::npos)
				break;
			rest = rest.substr(cut + 1);
		}

		return out;
	}

	std::string JoinBlob(const std::map<std::string, std::string>& values) noexcept {
		std::string out;
		for (const auto& [key, value] : values) {
			if (key.empty())
				continue;
			if (!out.empty())
				out += ':';
			out += key;
			out += '=';
			out += value;
		}

		return out;
	}

	bool HasKey(const char* key) noexcept {
		return key && key[0] != '\0';
	}

	std::int64_t BufSizeBits(const StormByte::Multimedia::Pipeline::Encoder& encoder) noexcept {
		if (encoder.MaxBitRate())
			return *encoder.MaxBitRate() * 2;
		if (encoder.BitRate())
			return *encoder.BitRate() * 2;
		return 0;
	}
}

bool Encoder::PrepareForHeader(StormByte::Multimedia::Pipeline::Encoder&) noexcept {
	return true;
}

bool Encoder::PrepareForMux(StormByte::Multimedia::Pipeline::Encoder& owner) noexcept {
	return owner.m_backend && owner.m_backend->PrepareForHeader(owner);
}

std::int64_t Encoder::NsToTicks(std::int64_t ns, FFmpeg::AVRational timeBase) noexcept {
	if (ns < 0 || !timeBase.Valid())
		return FFmpeg::NoPts;
	return FFmpeg::Nanosecond.Rescale(ns, timeBase);
}

StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> Encoder::MakePacket(
	StormByte::Multimedia::Pipeline::Encoder& owner,
	enum Type type, int index, const StormByte::Multimedia::FFmpeg::AVPacket& raw,
	AVRational timeBase, bool keepPacketHdrPlus) noexcept {
	StormByte::Safe::Binary bytes;
	const auto* data = raw.Data();
	const int size = raw.Size();
	if (data && size > 0) {
		const auto* rawBytes = reinterpret_cast<const std::byte*>(data);
		bytes.assign(rawBytes, rawBytes + size);
	}

	StormByte::Safe::Vector<SideData> attachments;
	for (int i = 0; i < raw.SideDataCount(); ++i) {
		int size = 0;
		const auto* data = raw.SideData(i, size);
		if (!data || size <= 0)
			continue;
		const int type = raw.SideDataType(i);
		if (type == AV_PKT_DATA_DYNAMIC_HDR10_PLUS && !keepPacketHdrPlus)
			continue;
		if (type != AV_PKT_DATA_DYNAMIC_HDR10_PLUS
			&& type != AV_PKT_DATA_MASTERING_DISPLAY_METADATA
			&& type != AV_PKT_DATA_CONTENT_LIGHT_LEVEL)
			continue;
		StormByte::Safe::Binary blob;
		blob.assign(
			reinterpret_cast<const std::byte*>(data),
			reinterpret_cast<const std::byte*>(data) + size);
		switch (type) {
			case AV_PKT_DATA_DYNAMIC_HDR10_PLUS:
				attachments.emplace_back(SideDataKind::HdrPlus,
					StormByte::Buffer::FIFO{std::move(blob)});
				break;
			case AV_PKT_DATA_MASTERING_DISPLAY_METADATA:
				attachments.emplace_back(SideDataKind::MasteringDisplay,
					StormByte::Buffer::FIFO{std::move(blob)});
				break;
			case AV_PKT_DATA_CONTENT_LIGHT_LEVEL:
				attachments.emplace_back(SideDataKind::ContentLight,
					StormByte::Buffer::FIFO{std::move(blob)});
				break;
			default:
				break;
		}
	}

	auto holder = StormByte::Safe::MakeUnique<Packet>();
	holder->Handle(raw.Ref());
	if (owner.m_backend) {
		if (const auto* ctx = owner.m_backend->Context(); ctx) {
			::AVCodecParameters* par = avcodec_parameters_alloc();
			if (par && avcodec_parameters_from_context(par, ctx) >= 0)
				holder->Parameters(StormByte::Multimedia::FFmpeg::AVCodecParameters(par));
			if (par)
				avcodec_parameters_free(&par);
		}
	}

	return owner.Wrap(
		type, index,
		StormByte::Buffer::FIFO{std::move(bytes)},
		TicksToPts(raw.Pts(), timeBase),
		TicksToPts(raw.Dts(), timeBase),
		TicksToDuration(raw.Duration(), timeBase),
		(raw.Flags() & AV_PKT_FLAG_KEY) != 0,
		std::move(attachments),
		std::move(holder));
}

std::optional<Encoder::Opened> Encoder::OpenCodec(StormByte::Multimedia::Pipeline::Encoder& owner,
	StormByte::Multimedia::FFmpeg::AVCodecParameters params,
	AVRational timeBase,
	Features need) noexcept {
	if (!owner.Destination().HasAccess(Operation::Write)) {
		owner.Fail("codec is not writable");
		return std::nullopt;
	}

	const std::string stormName{owner.Destination().Name()};
	const auto implementation = owner.Implementation();
	const std::string_view pin = implementation
		? std::string_view{*implementation} : std::string_view{};

	if (!pin.empty()) {
		if (avcodec_find_encoder_by_name(std::string(pin).c_str()) == nullptr) {
			owner.Fail("encoder implementation is unavailable");
			return std::nullopt;
		}

		const auto* listed = FindRow(stormName, pin);
		if (!listed || !listed->features.Has(need)) {
			owner.Fail("encoder implementation lacks required features");
			return std::nullopt;
		}
	}

	const auto* row = PickEncoder(stormName, pin, need);
	const ::AVCodec* codec = nullptr;
	if (row)
		codec = avcodec_find_encoder_by_name(row->name);
	if (!codec) {
		owner.Fail("no encoder for destination codec");
		return std::nullopt;
	}

	if (owner.CRF() && (!row || !HasKey(row->crf_key))) {
		owner.Fail("encoder implementation does not support CRF");
		return std::nullopt;
	}

	if (owner.BitRate() && (!row || !HasKey(row->bitrate_key))) {
		owner.Fail("encoder implementation does not support BitRate");
		return std::nullopt;
	}

	if (owner.MaxBitRate() && (!row || !HasKey(row->maxrate_key))) {
		owner.Fail("encoder implementation does not support MaxBitRate");
		return std::nullopt;
	}

	if (owner.Preset() && (!row || !HasKey(row->preset_key))) {
		owner.Fail("encoder implementation does not support Preset");
		return std::nullopt;
	}

	if (owner.Tune() && (!row || !HasKey(row->style_key))) {
		owner.Fail("encoder implementation does not support Tune");
		return std::nullopt;
	}

	const auto fineSnapshot = owner.FineTune();
	std::map<std::string, std::string> fine;
	for (const auto& [key, value] : fineSnapshot)
		fine.emplace(std::string{key}, std::string{value});
	for (const auto& [key, value] : fine) {
		(void)value;
		if (key == "bufsize" || key == "vbv-bufsize") {
			owner.Fail("FineTune cannot set bufsize");
			return std::nullopt;
		}
	}

	std::map<std::string, std::string> blob;
	std::map<std::string, std::string> opts;
	if (row && need.Has(Feature::HDR10))
		for (auto& pair : SplitBlob(row->signal_hdr10 ? row->signal_hdr10 : ""))
			blob.insert(std::move(pair));

	if (owner.CRF() && row && HasKey(row->crf_key))
		opts.emplace(row->crf_key, std::to_string(*owner.CRF()));
	if (owner.BitRate() && row && HasKey(row->bitrate_key))
		opts.emplace(row->bitrate_key, std::to_string(*owner.BitRate()));
	else if (owner.CRF() && !owner.BitRate() && row && HasKey(row->bitrate_key)
		&& (std::string_view(row->name) == "libvpx" || std::string_view(row->name) == "libvpx-vp9"))
		opts.emplace(row->bitrate_key, "0");
	if (owner.MaxBitRate() && row && HasKey(row->maxrate_key))
		opts.emplace(row->maxrate_key, std::to_string(*owner.MaxBitRate()));
	if (row && HasKey(row->bufsize_key) && (owner.BitRate() || owner.MaxBitRate()))
		opts.emplace(row->bufsize_key, std::to_string(BufSizeBits(owner)));
	if (owner.Preset() && row && HasKey(row->preset_key))
		opts.emplace(row->preset_key, std::string{*owner.Preset()});
	if (owner.Tune() && row && HasKey(row->style_key))
		opts.emplace(row->style_key, std::string{*owner.Tune()});

	const bool pack = row && HasKey(row->tune_key);
	if (pack) {
		if (!blob.contains("wpp") && !fine.contains("wpp"))
			blob.emplace("wpp", "1");
		if (!blob.contains("pools") && !blob.contains("numa-pools")
			&& !fine.contains("pools") && !fine.contains("numa-pools")) {
			unsigned workers = std::thread::hardware_concurrency();
			if (workers < 1)
				workers = 1;
			blob.emplace("pools", std::to_string(workers));
		}
	}

	if (row && std::string_view(row->name) == "libvpx-vp9") {
		if (!opts.contains("row-mt") && !fine.contains("row-mt"))
			opts.emplace("row-mt", "1");
	}

	for (const auto& [key, value] : fine) {
		if (key.empty())
			continue;
		if (blob.contains(key) && blob[key] != value) {
			owner.Fail("FineTune conflicts with HDR signaling key '" + key + "'");
			return std::nullopt;
		}

		if (opts.contains(key) && opts[key] != value) {
			owner.Fail("FineTune conflicts with encoder setter key '" + key + "'");
			return std::nullopt;
		}

		if (pack)
			blob.emplace(key, value);
		else
			opts.emplace(key, value);
	}

	if (pack) {
		const auto packed = JoinBlob(blob);
		if (!packed.empty())
			opts.emplace(row->tune_key, packed);
	}

	else {
		for (const auto& [key, value] : blob)
			opts.emplace(key, value);
	}

	if (codec) {
		params.CodecId(static_cast<int>(codec->id));
		params.CodecType(static_cast<int>(codec->type));
		if (codec->type == AVMEDIA_TYPE_AUDIO) {
			if (codec->id == AV_CODEC_ID_MP3 && params.Channels() > 2) {
				owner.Fail("MP3 encoder has unsupported channel layout; use an explicit downmix filter");
				return std::nullopt;
			}
			if (params.Format() == AV_SAMPLE_FMT_NONE) {
				const void* configs = nullptr;
				int count = 0;
				if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT,
						0, &configs, &count) >= 0 && configs && count > 0) {
					const auto* fmts = static_cast<const AVSampleFormat*>(configs);
					params.Format(static_cast<int>(fmts[0]));
				}
			}

			if (params) {
				const void* layouts = nullptr;
				int count = 0;
				if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT,
						0, &layouts, &count) >= 0 && layouts && count > 0) {
					const auto* list = static_cast<const ::AVChannelLayout*>(layouts);
					const auto have = params.ChannelLayout();
					bool supported = false;
					if (have) {
						const auto* haveRaw = FFmpeg::ToRaw(have);
						for (int i = 0; i < count; ++i) {
							if (haveRaw && av_channel_layout_compare(haveRaw, &list[i]) == 0) {
								supported = true;
								break;
							}
						}
					}

					if (!supported) {
						// Automatic 7.1 to 5.1 conversion is allowed for AC-3/E-AC3 without a manual downmix filter.
						const ::AVChannelLayout* best = &list[0];
						const int n = have.NbChannels();
						for (int i = 0; i < count; ++i) {
							if (list[i].nb_channels <= n
								&& list[i].nb_channels >= best->nb_channels)
								best = &list[i];
						}

						params.CopyChannelLayout(FFmpeg::FromRaw(*best));
					}
				}
			}
		}
	}

	StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
	for (const auto& [key, value] : opts)
		options.emplace(StormByte::Safe::String{key}, StormByte::Safe::String{value});
	auto opened = StormByte::Multimedia::FFmpeg::AVEncoder::Open(
		row->name, params, owner.Index(), options, timeBase);
	if (!opened.has_value()) {
		owner.Fail(opened.error() ? opened.error()->what() : "Failed to open encoder");
		return std::nullopt;
	}

	Opened out{std::move(opened.value()), timeBase, {}, {}};
	auto tb = out.Handle().TimeBase();
	if (tb.num <= 0 || tb.den <= 0)
		tb = timeBase;
	out.TimeBase(tb);
	if (row) {
		out.Implementation(row->name);
		out.Capabilities(row->features);
	}

	return out;
}

StormByte::Multimedia::FFmpeg::AVFrame* Encoder::FrameHandle(
	StormByte::Multimedia::Pipeline::Encoder& owner,
	StormByte::Multimedia::Pipeline::Frame& frame) noexcept {
	return static_cast<StormByte::Multimedia::FFmpeg::AVFrame*>(owner.FrameHandle(frame));
}

const StormByte::Multimedia::FFmpeg::AVFrame* Encoder::FrameHandle(
	StormByte::Multimedia::Pipeline::Encoder& owner,
	const StormByte::Multimedia::Pipeline::Frame& frame) noexcept {
	return static_cast<const StormByte::Multimedia::FFmpeg::AVFrame*>(owner.FrameHandle(frame));
}

void Encoder::CommitOpen(StormByte::Multimedia::Pipeline::Encoder& owner,
	const Opened& opened) noexcept {
	if (!opened.Implementation().empty())
		owner.Implementation(StormByte::Safe::String{opened.Implementation()});
	owner.m_capabilities = opened.Capabilities();
}
