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

#include <StormByte/multimedia/backend/pipeline/detail/muxer/matroska/attachment.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/container.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/encoder.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/side_data.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>
#include <StormByte/multimedia/type.hxx>

#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/dict.h>
	#include <libavutil/error.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/mem.h>
	#include <libavutil/rational.h>
}

namespace Config = StormByte::Multimedia::Pipeline::Config;
namespace FFmpeg = StormByte::Multimedia::FFmpeg;

namespace {
	const FFmpeg::AVRational NanoTimeBase{1, 1000000000};
	constexpr const char WritingApp[] = "StormByte-Multimedia " STORMBYTE_MULTIMEDIA_VERSION;
	constexpr std::int64_t InterleaveSlotUs = 40LL * 1000;
	constexpr std::int64_t InterleaveFloorUs = 250000;
	constexpr std::int64_t InterleaveMixedUs = 60LL * 1000 * 1000;

	bool Encodes(const StormByte::Multimedia::Pipeline::Track& track) noexcept {
		const auto* cfg = track.Config();
		if (!cfg)
			return false;
		if (const auto* video = dynamic_cast<const Config::Video*>(cfg))
			return video->Codec() != nullptr;
		if (const auto* audio = dynamic_cast<const Config::Audio*>(cfg))
			return audio->Codec() != nullptr;
		if (const auto* sub = dynamic_cast<const Config::Subtitle*>(cfg))
			return sub->Codec() != nullptr;
		return false;
	}

	bool Muxable(StormByte::Multimedia::Type type) noexcept {
		return type == StormByte::Multimedia::Type::Video
			|| type == StormByte::Multimedia::Type::Audio
			|| type == StormByte::Multimedia::Type::Subtitle;
	}

	std::int64_t InterleaveDeltaUs(const StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		std::size_t remux = 0;
		std::size_t encodeVideo = 0;
		std::size_t encodeOther = 0;
		const auto& plan = owner.Plan();
		if (plan) {
			for (const auto& held : plan->Tracks()) {
				if (!held)
					continue;
				const auto& track = *held;
				if (!Muxable(track.Type()))
					continue;
				if (!Encodes(track))
					++remux;
				else if (track.Type() == StormByte::Multimedia::Type::Video)
					++encodeVideo;
				else
					++encodeOther;
			}
		}
		if (encodeVideo != 0 && remux != 0)
			return InterleaveMixedUs;

		std::int64_t us = InterleaveFloorUs;
		us += static_cast<std::int64_t>(encodeVideo) * 500000;
		us += static_cast<std::int64_t>(remux) * 100000;
		us += static_cast<std::int64_t>(encodeOther) * 80000;
		if (us < InterleaveFloorUs)
			us = InterleaveFloorUs;
		if (us > InterleaveMixedUs)
			us = InterleaveMixedUs;
		return us;
	}

	std::int64_t NsToTicks(std::int64_t ns, FFmpeg::AVRational time_base) noexcept {
		if (time_base.num <= 0 || time_base.den <= 0)
			return AV_NOPTS_VALUE;
		return NanoTimeBase.Rescale(ns, time_base);
	}

	std::span<const std::byte> UnreadSpan(const StormByte::Buffer::FIFO& fifo) noexcept {
		const auto& stored = fifo.Data();
		const auto avail = fifo.Available();
		if (avail == 0 || avail > stored.size())
			return {};
		return std::span<const std::byte>{stored.data() + (stored.size() - avail), avail};
	}

	AVPacket* MakeAvPacket(StormByte::Multimedia::Pipeline::Packet& packet) noexcept {
		AVPacket* raw = av_packet_alloc();
		if (!raw)
			return nullptr;

		auto& fifo = packet.Payload();
		const auto size = fifo.Available();
		if (size > 0) {
			if (av_new_packet(raw, static_cast<int>(size)) < 0) {
				av_packet_free(&raw);
				return nullptr;
			}

			const auto view = UnreadSpan(fifo);
			if (view.size() != size) {
				av_packet_free(&raw);
				return nullptr;
			}

			std::memcpy(raw->data, view.data(), size);
			if (!fifo.Drop(size)) {
				av_packet_free(&raw);
				return nullptr;
			}
		}

		if (packet.KeyFrame())
			raw->flags |= AV_PKT_FLAG_KEY;

		for (const StormByte::Multimedia::Pipeline::SideData side : packet.Attachments()) {
			const auto view = UnreadSpan(side.Payload());
			if (view.empty())
				continue;
			enum AVPacketSideDataType type = AV_PKT_DATA_NB;
			switch (side.Kind()) {
				case StormByte::Multimedia::Pipeline::SideDataKind::HdrPlus:
					type = AV_PKT_DATA_DYNAMIC_HDR10_PLUS;
					break;
				case StormByte::Multimedia::Pipeline::SideDataKind::MasteringDisplay:
					type = AV_PKT_DATA_MASTERING_DISPLAY_METADATA;
					break;
				case StormByte::Multimedia::Pipeline::SideDataKind::ContentLight:
					type = AV_PKT_DATA_CONTENT_LIGHT_LEVEL;
					break;
				default:
					break;
			}

			if (type == AV_PKT_DATA_NB)
				continue;
			uint8_t* dst = av_packet_new_side_data(raw, type, view.size());
			if (!dst) {
				av_packet_free(&raw);
				return nullptr;
			}

			std::memcpy(dst, view.data(), view.size());
		}

		return raw;
	}

	bool PlanWantsAttachments(const StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		const auto& plan = owner.Plan();
		if (!plan)
			return false;
		for (const auto& track : plan->Tracks()) {
			if (track && track->Type() == StormByte::Multimedia::Type::Attachment)
				return true;
		}

		return false;
	}

	StormByte::Buffer::FIFO CopyPayload(const StormByte::Buffer::FIFO& fifo) noexcept {
		const auto view = UnreadSpan(fifo);
		if (view.empty())
			return StormByte::Buffer::FIFO{};
		StormByte::Safe::Binary bytes{view};
		return StormByte::Buffer::FIFO{std::move(bytes)};
	}
}

namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg {
	namespace {
		bool MatroskaFamily(const StormByte::Multimedia::Container& container) noexcept {
			const auto name = container.Name();
			return name == "Matroska" || name == "WebM";
		}
	}

	Container::Container() noexcept
	: m_ctx(nullptr),
	m_header(false), m_trailer(false), m_customIo(false) {}

	Container::~Container() noexcept {
		FreeParams();
		Close();
	}

	void Container::FreeParams() noexcept {
		for (auto& [index, track] : m_tracks) {
			if (track.params) {
				avcodec_parameters_free(&track.params);
				track.params = nullptr;
			}
		}
	}

	bool Container::IsOpen() const noexcept {
		return m_ctx != nullptr;
	}

	bool Container::HeaderWritten() const noexcept {
		return m_header;
	}

	int Container::Resolve(int track) const noexcept {
		if (m_tracks.contains(track))
			return track;
		const auto it = m_inToOut.find(track);
		if (it != m_inToOut.end())
			return it->second;
		return -1;
	}

	bool Container::BindSink(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		if (m_ctx) {
			owner.Fail("muxer destination is already bound");
			return false;
		}
		if (!owner.Plan()) {
			owner.Fail("muxer has no plan");
			return false;
		}

		const auto ext = owner.Destination().Extension();
		if (ext.empty()) {
			owner.Fail("destination container has no extension");
			return false;
		}

		const std::string dummy = "out." + std::string(ext);
		const AVOutputFormat* oformat = av_guess_format(nullptr, dummy.c_str(), nullptr);
		if (!oformat) {
			owner.Fail("could not guess output format from container");
			return false;
		}

		AVFormatContext* ctx = nullptr;
		if (avformat_alloc_output_context2(&ctx, const_cast<AVOutputFormat*>(oformat),
				nullptr, nullptr) < 0 || !ctx) {
			owner.Fail("avformat_alloc_output_context2 failed");
			return false;
		}

		m_avio.emplace(owner.Plan()->Writer());
		if (!m_avio->Arm() || !m_avio->Context()) {
			owner.Fail("file AVIO alloc failed");
			m_avio.reset();
			avformat_free_context(ctx);
			return false;
		}

		m_ctx = ctx;
		if (!(ctx->oformat->flags & AVFMT_NOFILE)) {
			ctx->pb = m_avio->Context();
			ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
			m_customIo = true;
		}

		return WriteHeaderIfReady(owner);
	}

	bool Container::ReserveEncoder(StormByte::Multimedia::Pipeline::Muxer& owner,
		StormByte::Multimedia::Pipeline::Encoder& encoder) noexcept {
		if (m_header) {
			owner.Fail("cannot add a track after the header");
			return false;
		}

		if (encoder.Index() < 0) {
			owner.Fail("encoder output index is invalid");
			return false;
		}

		if (m_tracks.contains(encoder.Index())) {
			owner.Fail("duplicate mux output index");
			return false;
		}

		Track track;
		track.encoder = &encoder;
		track.language = owner.Language(encoder.Index());
		track.title = owner.Title(encoder.Index());
		m_tracks.emplace(encoder.Index(), std::move(track));
		return true;
	}

	bool Container::ReserveRemux(StormByte::Multimedia::Pipeline::Muxer& owner,
		int inIndex) noexcept {
		if (m_header) {
			owner.Fail("cannot add a track after the header");
			return false;
		}

		if (inIndex < 0) {
			owner.Fail("remux source stream is invalid");
			return false;
		}

		const int out = static_cast<int>(m_tracks.size());
		if (m_tracks.contains(out)) {
			owner.Fail("duplicate mux output index");
			return false;
		}

		Track track;
		track.inIndex = inIndex;
		track.language = owner.Language(out);
		track.title = owner.Title(out);
		if (!track.language || !track.title) {
			const auto plan = owner.Plan();
			if (plan) {
				for (const auto stream : plan->Snapshot().Streams()) {
					if (stream.Index() != inIndex)
						continue;
					if (!track.language)
						track.language = stream.Metadata().Language();
					if (!track.title)
						track.title = stream.Metadata().Title();
					break;
				}
			}
		}
		m_tracks.emplace(out, std::move(track));
		m_inToOut.emplace(inIndex, out);
		return true;
	}

	bool Container::BindAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
		const StormByte::Multimedia::Attachments& attachments) noexcept {
		if (m_header) {
			owner.Fail("cannot bind attachments after the header");
			return false;
		}

		if (!PlanWantsAttachments(owner)) {
			m_attachments.clear();
			return true;
		}
		if (!owner.Destination().HasAccess(StormByte::Multimedia::Access{
				StormByte::Multimedia::Operation::Attach})) {
			owner.Fail("destination container does not support attachments");
			return false;
		}

		if (!owner.Destination().HasAccess(Access{Operation::Attach})) {
			owner.Fail("destination container does not support attachments");
			return false;
		}

		m_attachments.clear();
		m_attachments.reserve(attachments.size());
		for (const auto& item : attachments) {
			m_attachments.emplace_back(
				item.FileName(),
				item.MimeType(),
				CopyPayload(item.Payload()));
		}
		return true;
	}

	bool Container::Push(StormByte::Multimedia::Pipeline::Muxer& owner,
		const StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet>& packet) noexcept {
		if (!packet) {
			owner.Fail("empty packet");
			return false;
		}

		if (Resolve(packet->Track()) < 0) {
			owner.Fail("packet stream index is not a mux track");
			return false;
		}

		if (!m_ctx) {
			m_queue.push_back(packet);
			return true;
		}

		if (!WriteHeaderIfReady(owner))
			return false;
		if (m_header) {
			while (!m_queue.empty()) {
				StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> leftover = std::move(m_queue.front());
				m_queue.pop_front();
				if (!leftover || !WritePacket(owner, *leftover))
					return false;
			}

			return WritePacket(owner, *packet);
		}

		m_queue.push_back(packet);
		return true;
	}

	bool Container::WriteHeaderIfReady(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		if (owner.Failed() || m_header)
			return !owner.Failed();
		if (!m_ctx)
			return true;
		if (!owner.Armed())
			return true;
		if (m_tracks.empty())
			return true;
		for (const auto& [index, track] : m_tracks) {
			if (!track.encoder)
				continue;
			if (track.encoder->Failed()) {
				owner.Fail("encoder failed");
				return false;
			}

			if (!track.encoder->Opened())
				return true;
		}

		for (auto& [index, track] : m_tracks) {
			if (track.encoder || track.params)
				continue;
			if (!owner.RemuxCodec(track.inIndex,
					reinterpret_cast<void*&>(track.params),
					&track.srcTb))
				return true;
		}

		int expected = 0;
		bool haveDefaultVideo = false;
		bool haveDefaultAudio = false;
		for (auto& [index, track] : m_tracks) {
			if (index != expected) {
				owner.Fail("mux output indexes must be contiguous from 0");
				return false;
			}

			++expected;
			AVStream* stream = avformat_new_stream(m_ctx, nullptr);
			if (!stream) {
				owner.Fail("avformat_new_stream failed");
				return false;
			}

			if (track.encoder) {
				if (!BindEncoderStream(owner, *track.encoder, stream)) {
					owner.Fail("encoder context is empty");
					return false;
				}

				if (owner.Language(index))
					track.language = owner.Language(index);
				if (owner.Title(index))
					track.title = owner.Title(index);
				const auto encoderTag = track.encoder->EncoderTag();
				av_dict_set(&stream->metadata, "ENCODER", encoderTag.data(), 0);
			}

			else {
				if (!track.params) {
					owner.Fail("remux track has no codec parameters");
					return false;
				}

				if (avcodec_parameters_copy(stream->codecpar, track.params) < 0) {
					owner.Fail("avcodec_parameters_copy failed");
					return false;
				}

				if (track.srcTb.num > 0 && track.srcTb.den > 0)
					stream->time_base = ::AVRational{track.srcTb.num, track.srcTb.den};
			}

			if (stream->codecpar) {
				const int supported = avformat_query_codec(m_ctx->oformat,
					stream->codecpar->codec_id, FF_COMPLIANCE_NORMAL);
				if (supported == 0) {
					owner.Fail(std::format("destination {} does not support codec {}",
						owner.Destination().Name(), avcodec_get_name(stream->codecpar->codec_id)));
					return false;
				}
			}

			if (track.language)
				av_dict_set(&stream->metadata, "language", track.language->data(), 0);
			if (track.title)
				av_dict_set(&stream->metadata, "title", track.title->data(), 0);

			if (stream->codecpar) {
				if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !haveDefaultVideo) {
					stream->disposition |= AV_DISPOSITION_DEFAULT;
					haveDefaultVideo = true;
				}

				if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !haveDefaultAudio) {
					stream->disposition |= AV_DISPOSITION_DEFAULT;
					haveDefaultAudio = true;
				}
			}

			track.avIndex = stream->index;
			track.timeBase = FFmpeg::AVRational{stream->time_base.num, stream->time_base.den};
		}

		if (!m_attachments.empty()) {
			if (!MatroskaFamily(owner.Destination())) {
				owner.Fail("file attachments are only supported in Matroska/WebM output");
				return false;
			}
			if (!Matroska::Attachment::Write(owner, m_ctx, m_attachments))
				return false;
		}

		if (MatroskaFamily(owner.Destination()))
			av_dict_set(&m_ctx->metadata, "encoding_tool", WritingApp, 0);

		std::int64_t deltaUs = InterleaveDeltaUs(owner);
		const auto cap = owner.InputCeiling();
		const auto depth = cap == 0 ? 1 : cap;
		const std::int64_t byDepth = InterleaveSlotUs * static_cast<std::int64_t>(depth);
		if (byDepth > deltaUs)
			deltaUs = byDepth;

		std::int64_t minPts = -1;
		std::int64_t maxPts = -1;
		for (const auto& queued : m_queue) {
			if (!queued || !queued->Pts())
				continue;
			const std::int64_t ns = queued->Pts()->Nanoseconds().count();
			if (minPts < 0 || ns < minPts)
				minPts = ns;
			if (ns > maxPts)
				maxPts = ns;
		}
		if (minPts >= 0 && maxPts > minPts) {
			const std::int64_t spanUs = (maxPts - minPts) / 1000 + InterleaveSlotUs;
			if (spanUs > deltaUs)
				deltaUs = spanUs;
		}
		m_ctx->max_interleave_delta = deltaUs;

		AVDictionary* opts = nullptr;
		if (MatroskaFamily(owner.Destination()))
			av_dict_set(&opts, "default_mode", "passthrough", 0);
		const int rc = avformat_write_header(m_ctx, &opts);
		av_dict_free(&opts);
		if (rc < 0) {
			owner.Fail(std::format("avformat_write_header failed: {}",
				StormByte::Multimedia::FFmpeg::ErrorToString(rc)));
			return false;
		}

		for (auto& [index, track] : m_tracks) {
			if (track.avIndex < 0 || track.avIndex >= static_cast<int>(m_ctx->nb_streams))
				continue;
			AVStream* stream = m_ctx->streams[track.avIndex];
			const FFmpeg::AVRational tb{stream->time_base.num, stream->time_base.den};
			if (tb.num > 0 && tb.den > 0)
				track.timeBase = tb;
			if (track.encoder) {
				const auto encoderTag = track.encoder->EncoderTag();
				av_dict_set(&stream->metadata, "ENCODER", encoderTag.data(), 0);
			}
		}

		m_header = true;
		while (!m_queue.empty()) {
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> queued = std::move(m_queue.front());
			m_queue.pop_front();
			if (!queued || !WritePacket(owner, *queued))
				return false;
		}

		return true;
	}

	bool Container::WritePacket(StormByte::Multimedia::Pipeline::Muxer& owner,
		StormByte::Multimedia::Pipeline::Packet& packet) noexcept {
		if (owner.Failed() || !m_header)
			return false;
		const int out = Resolve(packet.Track());
		const auto it = m_tracks.find(out);
		if (it == m_tracks.end() || it->second.avIndex < 0) {
			owner.Fail("packet stream index is not a mux track");
			return false;
		}

		AVPacket* raw = MakeAvPacket(packet);
		if (!raw) {
			owner.Fail("could not allocate output packet");
			return false;
		}

		auto& track = it->second;
		raw->stream_index = track.avIndex;
		const FFmpeg::AVRational tb = track.timeBase;

		if (packet.Pts())
			raw->pts = NsToTicks(packet.Pts()->Nanoseconds().count(), tb);
		else
			raw->pts = AV_NOPTS_VALUE;
		if (packet.Dts())
			raw->dts = NsToTicks(packet.Dts()->Nanoseconds().count(), tb);
		else
			raw->dts = AV_NOPTS_VALUE;
		if (packet.Duration()) {
			const auto ticks = NsToTicks(packet.Duration()->Nanoseconds().count(), tb);
			raw->duration = (ticks == AV_NOPTS_VALUE || ticks < 0) ? 0 : ticks;
		}

		const bool isSub = packet.Type() == StormByte::Multimedia::Type::Subtitle;
		if (isSub) {
			if (raw->dts == AV_NOPTS_VALUE)
				raw->dts = raw->pts;
		}

		else {
			if (raw->dts == AV_NOPTS_VALUE) {
				if (track.lastDts != AV_NOPTS_VALUE)
					raw->dts = track.lastDts + 1;
				else if (raw->pts != AV_NOPTS_VALUE)
					raw->dts = raw->pts;
			}

			else if (track.lastDts != AV_NOPTS_VALUE && raw->dts <= track.lastDts)
				raw->dts = track.lastDts + 1;
			if (raw->pts != AV_NOPTS_VALUE && raw->dts != AV_NOPTS_VALUE && raw->pts < raw->dts)
				raw->pts = raw->dts;
			if (raw->dts != AV_NOPTS_VALUE)
				track.lastDts = raw->dts;
		}

		const int rc = av_interleaved_write_frame(m_ctx, raw);
		av_packet_free(&raw);
		if (rc < 0) {
			owner.Fail(std::format("av_interleaved_write_frame failed: {}",
				StormByte::Multimedia::FFmpeg::ErrorToString(rc)));
			return false;
		}

		return true;
	}

	void Container::Flush(StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		if (owner.Failed() || m_trailer)
			return;
		for (const auto& [index, track] : m_tracks) {
			if (track.encoder && track.encoder->Failed()) {
				owner.Fail(track.encoder->Error().value_or(StormByte::Safe::String{"encoder failed"}));
				return;
			}
		}

		for (auto& [index, track] : m_tracks) {
			if (!track.encoder || *track.encoder)
				continue;
			AVCodecParameters* params = avcodec_parameters_alloc();
			if (!params) {
				owner.Fail("avcodec_parameters_alloc failed");
				return;
			}

			params->codec_type = AVMEDIA_TYPE_SUBTITLE;
			params->codec_id = AV_CODEC_ID_SUBRIP;
			track.params = params;
			track.srcTb = FFmpeg::AVRational{1, 1000};
			track.encoder = nullptr;
		}

		if (!WriteHeaderIfReady(owner))
			return;
		while (!m_queue.empty()) {
			StormByte::Safe::Shared<StormByte::Multimedia::Pipeline::Packet> leftover = std::move(m_queue.front());
			m_queue.pop_front();
			if (m_header) {
				if (!leftover || !WritePacket(owner, *leftover))
					return;
			}
		}

		if (!m_header)
			WriteHeaderIfReady(owner);
		if (m_header && !m_trailer && m_ctx) {
			av_dict_set(&m_ctx->metadata, "ENCODER", WritingApp, 0);
			av_write_trailer(m_ctx);
			m_trailer = true;
		}
	}

	void Container::Close() noexcept {
		if (!m_ctx)
			return;
		if (m_header && !m_trailer) {
			av_write_trailer(m_ctx);
			m_trailer = true;
		}

		if (m_customIo && m_ctx->pb)
			m_ctx->pb = nullptr;

		avformat_free_context(m_ctx);
		m_ctx = nullptr;
		m_avio.reset();
		m_customIo = false;
	}
}
