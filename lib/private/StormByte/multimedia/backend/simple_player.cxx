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

#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/exception.hxx>
#include <StormByte/multimedia/backend/file_avio.hxx>
#include <StormByte/multimedia/backend/simple_player.hxx>
#include <StormByte/multimedia/ffmpeg/hardware_decode.hxx>
#include <StormByte/multimedia/ffmpeg/AVFilterGraph.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/property.hxx>
#include <StormByte/multimedia/log.hxx>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <thread>
#include <utility>

extern "C" {
	#include <libavcodec/ac3_parser.h>
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/error.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/samplefmt.h>
	#include <libswscale/swscale.h>
}

using namespace StormByte::Multimedia;
using StormByte::ByteSize;
using StormByte::Exception;
namespace Buffer = StormByte::Buffer;
namespace Logger = StormByte::Logger;
namespace Multimedia = StormByte::Multimedia;
namespace Safe = StormByte::Safe;

namespace {
	using PlaybackClock = std::chrono::steady_clock;
	constexpr std::size_t MaxBytes = 64 * 1024 * 1024;
	constexpr std::size_t MaxEvents = 256;
	constexpr double AudioAhead = 0.150;

	/**
	 * @brief Resolve a selectable media kind to its decoder slot.
	 * @param type Media kind.
	 * @return Slot, or -1 for unsupported kinds.
	 */
	int Slot(enum StormByte::Multimedia::Type type) {
		switch (type) {
			case StormByte::Multimedia::Type::Video: return 0;
			case StormByte::Multimedia::Type::Audio: return 1;
			case StormByte::Multimedia::Type::Subtitle: return 2;
			default: return -1;
		}
	}

	/**
	 * @brief Translate a private codec media kind.
	 * @param type Native kind.
	 * @return Public kind, or Unknown.
	 */
	enum StormByte::Multimedia::Type MediaType(AVMediaType type) {
		switch (type) {
			case AVMEDIA_TYPE_VIDEO: return StormByte::Multimedia::Type::Video;
			case AVMEDIA_TYPE_AUDIO: return StormByte::Multimedia::Type::Audio;
			case AVMEDIA_TYPE_SUBTITLE: return StormByte::Multimedia::Type::Subtitle;
			default: return StormByte::Multimedia::Type::Unknown;
		}
	}

	/**
	 * @brief Surface a failed private FFmpeg operation.
	 * @param result Native result.
	 * @param operation Operation description.
	 * @throws StormByte::Exception If the native result is negative.
	 */
	void Check(int result, std::string_view operation) {
		if (result >= 0)
			return;
		char text[AV_ERROR_MAX_STRING_SIZE]{};
		av_strerror(result, text, sizeof(text));
		Safe::String message(operation);
		message += ": ";
		message += text;
		throw Exception(message);
	}

	/**
	 * @brief Calculate bounded RGBA storage.
	 * @param width Pixel width.
	 * @param height Pixel height.
	 * @return Required bytes.
	 * @throws StormByte::Exception If geometry is invalid or exceeds the limit.
	 */
	std::size_t ImageBytes(int width, int height) {
		if (width <= 0 || height <= 0 || width > 32768 || height > 32768
			|| static_cast<std::uint64_t>(width) * height * 4 > MaxBytes)
			throw Exception("preview image exceeds bounded storage");
		return static_cast<std::size_t>(width) * height * 4;
	}

	/**
	 * @brief Calculate bounded RGBA16 storage.
	 * @param width Pixel width.
	 * @param height Pixel height.
	 * @return Required bytes.
	 * @throws StormByte::Exception If geometry is invalid or exceeds the limit.
	 */
	std::size_t HDRImageBytes(int width, int height) {
		if (width <= 0 || height <= 0 || width > 32768 || height > 32768
			|| static_cast<std::uint64_t>(width) * height * 8 > MaxBytes)
			throw Exception("HDR preview image exceeds bounded storage");
		return static_cast<std::size_t>(width) * height * 8;
	}

	/**
	 * @struct Event
	 * @brief One bounded scheduled presentation unit.
	 */
	struct Event {
		/**
		 * @brief Construct an empty event.
		 */
		Event() = default;

		/**
		 * @brief Share an event.
		 * @param other Source.
		 */
		Event(const Event& other) = default;

		/**
		 * @brief Take an event.
		 * @param other Source.
		 */
		Event(Event&& other) noexcept = default;

		/**
		 * @brief Release presentation owners.
		 */
		~Event() noexcept = default;

		/**
		 * @brief Share an event.
		 * @param other Source.
		 * @return This event.
		 */
		Event& operator=(const Event& other) = default;

		/**
		 * @brief Take an event.
		 * @param other Source.
		 * @return This event.
		 */
		Event& operator=(Event&& other) noexcept = default;

		Safe::Shared<PreviewVideo> m_video;			///< Optional image.
		Safe::Shared<PreviewHDRVideo> m_hdr_video;	///< Optional HDR image.
		Safe::Shared<PreviewAudio> m_audio;			///< Optional samples.
		Safe::Shared<PreviewSubtitle> m_subtitle;	///< Optional subtitle.
		double m_start = 0;							///< Delivery time.
		double m_end = 0;							///< Presentation deadline.
		std::size_t m_bytes = 0;					///< Charged queue storage.
	};

	/**
	 * @struct Decoder
	 * @brief Private codec and preview conversion owners.
	 */
	struct Decoder {
		/**
		 * @brief Construct an empty decoder.
		 */
		Decoder() = default;

		Decoder(const Decoder&) = delete;

		Decoder(Decoder&&) = delete;

		/**
		 * @brief Release codec and conversion state.
		 */
		~Decoder() noexcept {
			Reset();
		}

		Decoder& operator=(const Decoder&) = delete;

		Decoder& operator=(Decoder&&) = delete;

		/**
		 * @brief Release all private FFmpeg allocations.
		 */
		void Reset() noexcept {
			avcodec_free_context(&m_codec);
			av_frame_free(&m_download);
			sws_freeContext(m_scale);
			m_scale = nullptr;
			m_tonemap.reset();
			m_color_scale.reset();
			av_channel_layout_uninit(&m_input_layout);
			m_input_format = AV_SAMPLE_FMT_NONE;
			m_input_rate = 0;
			m_next = 0;
			m_audio_timed = false;
		}

		AVCodecContext* m_codec = nullptr;					///< Owned private codec context.
		AVFrame* m_download = nullptr;						///< Reusable software transfer for hardware video.
		SwsContext* m_scale = nullptr;						///< Owned pixel converter.
		std::optional<FFmpeg::AVFilterGraph> m_tonemap;		///< HDR-to-linear-light tone-map curve.
		std::optional<FFmpeg::AVFrame::ScaleContext> m_color_scale;	///< Reusable color-managed scaling graphs.
		AVChannelLayout m_input_layout{};					///< Owned source channel layout.
		AVSampleFormat m_input_format = AV_SAMPLE_FMT_NONE;	///< Current audio input format.
		int m_input_rate = 0;								///< Current audio input rate.
		double m_next = 0;									///< Next output timestamp.
		bool m_audio_timed = false;							///< Audio has a sample-continuous timeline.
	};

	/**
	 * @struct Scratch
	 * @brief Reusable FFmpeg frame and packet, released before worker exit.
	 */
	struct Scratch {
		/**
		 * @brief Allocate reusable decode scratch.
		 * @throws StormByte::Exception If allocation fails.
		 */
		Scratch() {
			if (!m_frame || !m_packet) {
				av_frame_free(&m_frame);
				av_packet_free(&m_packet);
				throw Exception("preview scratch allocation failed");
			}
		}

		Scratch(const Scratch&) = delete;

		Scratch(Scratch&&) = delete;

		/**
		 * @brief Release reusable decode scratch.
		 */
		~Scratch() noexcept {
			av_frame_free(&m_frame);
			av_packet_free(&m_packet);
		}

		Scratch& operator=(const Scratch&) = delete;

		Scratch& operator=(Scratch&&) = delete;

		AVFrame* m_frame = av_frame_alloc();	///< Private owned decode scratch.
		AVPacket* m_packet = av_packet_alloc();	///< Private owned demux scratch.
	};
}

/**
 * @class StormByte::Multimedia::Backend::SimplePlayer::Context
 * @brief Serialized session owner and bounded decode/presentation workers.
 */
class Backend::SimplePlayer::Context {
	public:
		/**
		 * @brief Retain the application sink and initialize scoped logging.
		 * @param display Presentation owner.
		 * @param logger Optional logger.
		 */
		Context(Safe::Shared<Display> display, Safe::Shared<Logger::Log> logger):
			m_display(std::move(display)), m_hdr_display(dynamic_cast<HDRDisplay*>(m_display.get())),
			m_logger(UseLog(std::move(logger), "SimplePlayer")) {}

		Context(const Context&) = delete;

		Context(Context&&) = delete;

		/**
		 * @brief Join workers and release remaining origin resources.
		 */
		~Context() noexcept {
			Halt();
			ResetFormat();
			if (m_reader) {
				try {
					if (m_reader->Close().status != Buffer::IO::Status::Ok)
						LogFailure("reader close failed during preview cleanup");
				}
				catch (...) {
					LogFailure("reader close threw during preview cleanup");
				}
			}
		}

		Context& operator=(const Context&) = delete;

		Context& operator=(Context&&) = delete;

		/**
		 * @brief Report a failure even when a logger rejects the diagnostic.
		 * @param message Diagnostic text.
		 */
		void LogFailure(std::string_view message) noexcept {
			if (m_logger) {
				try {
					*m_logger << Logger::Level::Error << message << std::endl;
					return;
				}
				catch (...) {
					std::fputs("SimplePlayer: logging failed; ", stderr);
				}
			}
			std::fwrite(message.data(), 1, message.size(), stderr);
			std::fputc('\n', stderr);
		}

		void LogWarning(std::string_view message) noexcept {
			if (m_logger) {
				try {
					*m_logger << Logger::Level::Warning << message << std::endl;
					return;
				}
				catch (...) {}
			}
			std::fputs("SimplePlayer: ", stderr);
			std::fwrite(message.data(), 1, message.size(), stderr);
			std::fputc('\n', stderr);
		}

		/**
		 * @brief Publish a diagnostic and interrupt both workers.
		 * @param message Failure description.
		 * @return False.
		 */
		bool Fail(std::string_view message) noexcept {
			{
				std::lock_guard lock(m_mutex);
				m_state = PlaybackState::Error;
				try { m_error = message; }
				catch (...) {
					LogFailure("unable to store preview failure text");
				}
			}
			m_cancel = true;
			m_ready.notify_all();
			LogFailure(message);
			return false;
		}

		/**
		 * @brief Publish the currently handled exception.
		 * @return False.
		 */
		bool Failure() noexcept {
			try {
				throw;
			}
			catch (const Exception& error) {
				return Fail(error.what());
			}
			catch (const std::exception& error) {
				return Fail(error.what());
			}
			catch (...) {
				return Fail("preview or display worker caught a non-standard exception");
			}
		}

		/**
		 * @brief Interrupt and join both workers without closing input.
		 */
		void Halt() noexcept {
			m_cancel = true;
			m_decode_stop = true;
			m_ready.notify_all();
			if (m_decode.joinable())
				m_decode.join();
			if (m_present.joinable())
				m_present.join();
		}

		/**
		 * @brief Release codecs and custom AVIO without closing its reader.
		 */
		void ResetFormat() noexcept {
			for (auto& decoder : m_decoders)
				decoder.Reset();
			if (m_format) {
				m_format->pb = nullptr;
				avformat_close_input(&m_format);
			}
			m_avio.reset();
			m_fonts.clear();
			m_font_bytes = 0;
		}

		/**
		 * @brief Discard scheduled units and reset per-run progress.
		 */
		void Clear() {
			std::lock_guard lock(m_mutex);
			m_queue.clear();
			m_bytes = 0;
			m_end = m_position;
			m_audio_end = m_position;
			m_eof = false;
			m_draining = false;
			m_preview_done = false;
			m_error.clear();
			m_open_cue = 0;
			m_open_cue_start = 0;
		}

		/**
		 * @brief Flush the sink and publish a new delivery generation.
		 */
		void Invalidate() {
			++m_epoch;
			m_cue_id = 0;
			m_display->Invalidate(m_epoch, m_selected[0] >= 0);
		}

		/**
		 * @brief Rewind only a healthy seekable reader, preserving remote tokens.
		 * @return Whether logical rewind succeeded.
		 */
		bool Rewind() {
			// Faulted buffered Rewind closes and reopens its origin, destroying
			// a remote reader's mount token. Never attempt that recovery here.
			if (!m_reader || !m_reader->IsOpen()
				|| !m_reader->IsSeekable() || m_reader->State() != Buffer::IO::State::Idle)
				return Fail("reader rewind rejected: input is closed or faulted");
			if (!m_reader->Rewind())
				return Fail("reader rewind failed; ownership retained");
			return true;
		}

		/**
		 * @brief Interrupt private demux operations on worker cancellation.
		 * @param opaque Session.
		 * @return Nonzero when cancelled.
		 */
		static int Interrupt(void* opaque) noexcept {
			return static_cast<Context*>(opaque)->m_cancel ? 1 : 0;
		}

		/**
		 * @brief Replace the codec for one selected media kind.
		 * @param slot Decoder slot.
		 * @param index Stream index, or -1 to disable.
		 * @throws StormByte::Exception If codec configuration fails.
		 */
		void OpenDecoder(int slot, int index) {
			auto& decoder = m_decoders[slot];
			decoder.Reset();
			if (index < 0)
				return;
			const auto* parameters = m_format->streams[index]->codecpar;
			const auto* codec = avcodec_find_decoder(parameters->codec_id);
			if (!codec)
				throw Exception("selected preview decoder is unavailable");
			auto create = [&]() -> AVCodecContext* {
				AVCodecContext* context = avcodec_alloc_context3(codec);
				if (!context)
					return nullptr;
				if (avcodec_parameters_to_context(context, parameters) < 0) {
					avcodec_free_context(&context);
					return nullptr;
				}
				context->pkt_timebase = m_format->streams[index]->time_base;
				context->thread_count = 2;
				context->max_pixels = MaxBytes / 4;
				context->max_samples = MaxBytes / 8;
				return context;
			};
			decoder.m_codec = create();
			if (!decoder.m_codec)
				throw Exception("preview decoder allocation or parameter copy failed");
			const bool video = parameters->codec_type == AVMEDIA_TYPE_VIDEO;
			const bool hardware = video && m_acceleration == DecodeAcceleration::Auto
				&& FFmpeg::HardwareDecode::Configure(*decoder.m_codec, codec);
			int opened = avcodec_open2(decoder.m_codec, codec, nullptr);
			if (opened < 0 && hardware) {
				avcodec_free_context(&decoder.m_codec);
				decoder.m_codec = create();
				if (!decoder.m_codec)
					throw Exception("preview software decoder allocation failed");
				opened = avcodec_open2(decoder.m_codec, codec, nullptr);
			}
			Check(opened, "open preview decoder");
			if (video && m_acceleration == DecodeAcceleration::Auto && !decoder.m_codec->hw_device_ctx)
				LogWarning("hardware video decoder unavailable; continuing with software decode");
			if (hardware && decoder.m_codec->hw_device_ctx) {
				decoder.m_download = av_frame_alloc();
				if (!decoder.m_download)
					throw Exception("preview hardware transfer frame allocation failed");
			}
			decoder.m_next = m_target;
		}

		/**
		 * @brief Probe the buffered origin and configure selected decoders.
		 * @param discover Whether to refresh the track list and default selection.
		 * @throws StormByte::Exception If probing or configuration fails.
		 */
		void OpenFormat(bool discover) {
			m_cancel = false;
			m_avio.emplace(*m_reader);
			if (!m_avio->Arm())
				throw Exception("preview AVIO allocation failed");
			m_format = avformat_alloc_context();
			if (!m_format)
				throw Exception("preview format allocation failed");
			m_format->pb = m_avio->Context();
			m_format->flags |= AVFMT_FLAG_CUSTOM_IO;
			m_format->probesize = 8 * 1024 * 1024;
			m_format->interrupt_callback = {Interrupt, this};
			Check(avformat_open_input(&m_format, nullptr, nullptr, nullptr), "open preview input");
			Check(avformat_find_stream_info(m_format, nullptr), "probe preview streams");
			m_origin = m_format->start_time == AV_NOPTS_VALUE ? 0
				: static_cast<double>(m_format->start_time) / AV_TIME_BASE;
			for (unsigned i = 0; i < m_format->nb_streams; ++i) {
				const auto* stream = m_format->streams[i];
				const auto* parameters = stream->codecpar;
				if (parameters->codec_id != AV_CODEC_ID_TTF && parameters->codec_id != AV_CODEC_ID_OTF)
					continue;
				if (parameters->extradata_size <= 0 || !parameters->extradata)
					continue;
				const auto bytes = static_cast<std::size_t>(parameters->extradata_size);
				if (m_font_bytes + bytes > MaxBytes)
					throw Exception("embedded subtitle fonts exceed bounded storage");
				Safe::Optional<Safe::String> name;
				Safe::Optional<Safe::String> mime;
				if (const auto* tag = av_dict_get(stream->metadata, "filename", nullptr, 0))
					name = Safe::String(std::string_view(tag->value));
				if (const auto* tag = av_dict_get(stream->metadata, "mimetype", nullptr, 0))
					mime = Safe::String(std::string_view(tag->value));
				Buffer::FIFO payload(Safe::Binary(reinterpret_cast<const std::byte*>(parameters->extradata), ByteSize{bytes}));
				m_fonts.push_back(Safe::MakeShared<Attachment>(std::move(name), std::move(mime), std::move(payload)));
				m_font_bytes += bytes;
			}
			if (discover) {
				Safe::Vector<PreviewTrack> tracks;
				m_selected = {-1, -1, -1};
				for (unsigned i = 0; i < m_format->nb_streams; ++i) {
					const auto* stream = m_format->streams[i];
					const auto type = MediaType(stream->codecpar->codec_type);
					const int slot = Slot(type);
					if (slot < 0 || (stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
						continue;
					PreviewTrack track;
					track.m_index = static_cast<int>(i);
					track.m_type = type;
					track.m_codec = std::string_view(avcodec_get_name(stream->codecpar->codec_id));
					if (const auto* entry = av_dict_get(stream->metadata, "language", nullptr, 0))
						track.m_language = std::string_view(entry->value);
					if (const auto* entry = av_dict_get(stream->metadata, "title", nullptr, 0))
						track.m_title = std::string_view(entry->value);
					tracks.push_back(std::move(track));
					if (m_selected[slot] < 0 && slot != 2
						&& avcodec_find_decoder(stream->codecpar->codec_id))
						m_selected[slot] = static_cast<int>(i);
				}
				std::lock_guard lock(m_mutex);
				m_tracks = std::move(tracks);
				m_duration = m_format->duration > 0
					? static_cast<double>(m_format->duration) / AV_TIME_BASE : 0;
			}
			for (int i = 0; i < 3; ++i)
				OpenDecoder(i, m_selected[i]);
		}

		/**
		 * @brief Schedule a decoded unit with bounded queue storage.
		 * @param event Unit to take.
		 * @return Whether the unit was accepted before cancellation.
		 * @throws StormByte::Exception If the unit or paused preview exceeds storage.
		 */
		bool Enqueue(Event event) {
			if (event.m_bytes > MaxBytes)
				throw Exception("decoded preview unit exceeds bounded storage");
			std::unique_lock lock(m_mutex);
			if (m_preview && (m_queue.size() >= MaxEvents || m_bytes + event.m_bytes > MaxBytes))
				throw Exception("paused preview exceeds bounded presentation storage");
			m_ready.wait(lock, [&] {
				return m_cancel || (m_decode_stop && event.m_audio)
					|| (m_queue.size() < MaxEvents && m_bytes + event.m_bytes <= MaxBytes);
			});
			if (m_cancel)
				return false;
			if (m_decode_stop && event.m_audio)
				return false;
			if (std::isfinite(event.m_end))
				m_end = std::max(m_end, event.m_end);
			m_bytes += event.m_bytes;
			const auto at = std::upper_bound(m_queue.begin(), m_queue.end(), event.m_start,
				[](double start, const Event& queued) { return start < queued.m_start; });
			m_queue.insert(at, std::move(event));
			m_ready.notify_all();
			return !m_decode_stop;
		}

		/**
		 * @brief Normalize a timestamp to the media timeline.
		 * @param slot Selected decoder slot.
		 * @param pts Native timestamp or unknown sentinel.
		 * @return Media seconds, using decoder continuation when unknown.
		 */
		double Timestamp(int slot, std::int64_t pts) const {
			if (pts == AV_NOPTS_VALUE)
				return m_decoders[slot].m_next;
			return pts * av_q2d(m_format->streams[m_selected[slot]]->time_base) - m_origin;
		}

		/**
		 * @brief Convert a decoded image and schedule its presentation.
		 * @param frame Borrowed private image.
		 * @return Whether decoding may continue.
		 */
		bool Video(AVFrame* frame) {
			auto& decoder = m_decoders[0];
			if (FFmpeg::HardwareDecode::Hardware(static_cast<AVPixelFormat>(frame->format))) {
				if (!decoder.m_download)
					throw Exception("preview hardware frame has no transfer destination");
				av_frame_unref(decoder.m_download);
				if (!FFmpeg::HardwareDecode::Download(*frame, *decoder.m_download))
					throw Exception("preview hardware frame transfer failed");
				frame = decoder.m_download;
			}
			const double start = Timestamp(0, frame->best_effort_timestamp);
			const auto rate = av_guess_frame_rate(m_format, m_format->streams[m_selected[0]], frame);
			const double duration = frame->duration > 0
				? frame->duration * av_q2d(m_format->streams[m_selected[0]]->time_base)
				: (rate.num > 0 && rate.den > 0 ? av_q2d(av_inv_q(rate)) : 1.0 / 25);
			decoder.m_next = start + duration;
			if (start + duration <= m_target)
				return true;
			if (m_preview && m_preview_video_ready)
				return true;

			using PreviewFrame = FFmpeg::AVFrame;
			const int transfer = frame->color_trc;
			if (m_color_mode == PreviewColorMode::HDR
				&& (transfer == AVCOL_TRC_SMPTE2084 || transfer == AVCOL_TRC_ARIB_STD_B67)) {
				if (!m_hdr_display)
					throw Exception("HDR preview requires an HDRDisplay sink");
				PreviewFrame source;
				source.Reset(av_frame_clone(frame));
				if (!source)
					throw Exception("preview HDR frame reference failed");
				PreviewFrame hdr_rgb;
				hdr_rgb.Format(AV_PIX_FMT_GBRPF32LE);
				PreviewFrame::ColorTarget hdr_target{
					.transfer = transfer,
					.primaries = frame->color_primaries,
					.matrix = AVCOL_SPC_RGB,
					.range = AVCOL_RANGE_JPEG,
					.peak = 0.0
				};
				if (!m_decoders[0].m_color_scale)
					m_decoders[0].m_color_scale.emplace();
				if (!source.ScaleTo(*m_decoders[0].m_color_scale, hdr_rgb, frame->width, frame->height,
						PreviewFrame::Resample::Default, PreviewFrame::Scaler::Zimg, hdr_target))
					throw Exception("preview HDR RGB conversion failed");
				hdr_rgb.ColorTransfer(transfer);
				hdr_rgb.ColorPrimaries(frame->color_primaries);
				hdr_rgb.ColorSpace(AVCOL_SPC_RGB);
				hdr_rgb.ColorRange(AVCOL_RANGE_JPEG);
				auto image = Safe::MakeShared<PreviewHDRVideo>();
				image->m_width = frame->width;
				image->m_height = frame->height;
				image->m_epoch = m_epoch;
				image->m_position = start;
				image->m_duration = duration;
				const auto color = FFmpeg::MapColor(source, {});
				image->m_primaries = color.Primaries();
				image->m_transfer = transfer == AVCOL_TRC_SMPTE2084
					? Property::Transfer::SMPTE2084 : Property::Transfer::ARIB_B67;
				const auto bytes = HDRImageBytes(frame->width, frame->height);
				const auto row_bytes = static_cast<std::size_t>(frame->width) * 8;
				if (!hdr_rgb.Data(0) || !hdr_rgb.Data(1) || !hdr_rgb.Data(2)
					|| hdr_rgb.Linesize(0) < frame->width * static_cast<int>(sizeof(float))
					|| hdr_rgb.Linesize(1) < frame->width * static_cast<int>(sizeof(float))
					|| hdr_rgb.Linesize(2) < frame->width * static_cast<int>(sizeof(float)))
					throw Exception("preview HDR RGB frame has an invalid stride");
				image->m_pixels.resize(ByteSize{bytes});
				auto* destination = static_cast<std::byte*>(image->m_pixels.data());
				const auto encode = [](const std::uint8_t* plane, int pixel) {
					float value = 0;
					std::memcpy(&value, plane + static_cast<std::size_t>(pixel) * sizeof(float), sizeof(value));
					if (!std::isfinite(value))
						throw Exception("preview HDR conversion produced a non-finite channel");
					return static_cast<std::uint16_t>(
						std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
				};
				for (int row = 0; row < frame->height; ++row) {
					// FFmpeg stores planar GBR in G, B, R plane order.
					const auto* green = hdr_rgb.Data(0) + static_cast<std::size_t>(row) * hdr_rgb.Linesize(0);
					const auto* blue = hdr_rgb.Data(1) + static_cast<std::size_t>(row) * hdr_rgb.Linesize(1);
					const auto* red = hdr_rgb.Data(2) + static_cast<std::size_t>(row) * hdr_rgb.Linesize(2);
					auto* output = destination + static_cast<std::size_t>(row) * row_bytes;
					for (int column = 0; column < frame->width; ++column) {
						const auto offset = static_cast<std::size_t>(column) * 8;
						const std::array channels{encode(red, column), encode(green, column), encode(blue, column)};
						for (std::size_t channel = 0; channel < channels.size(); ++channel) {
							output[offset + channel * 2] = static_cast<std::byte>(channels[channel] & 0xff);
							output[offset + channel * 2 + 1] = static_cast<std::byte>(channels[channel] >> 8);
						}
						output[offset + 6] = std::byte{0xff};
						output[offset + 7] = std::byte{0xff};
					}
				}
				if (const auto* side = av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
					side && side->size >= sizeof(AVMasteringDisplayMetadata)) {
					const auto* metadata = reinterpret_cast<const AVMasteringDisplayMetadata*>(side->data);
					if (metadata->has_primaries) {
						const auto read_chromaticity = [](AVRational value, double& output) {
							if (value.den == 0)
								return false;
							output = av_q2d(value);
							return std::isfinite(output) && output >= 0.0 && output <= 1.0;
						};
						bool valid = true;
						for (std::size_t primary = 0; primary < image->m_display_primaries.size(); ++primary) {
							valid = read_chromaticity(metadata->display_primaries[primary][0],
								image->m_display_primaries[primary][0]) && valid;
							valid = read_chromaticity(metadata->display_primaries[primary][1],
								image->m_display_primaries[primary][1]) && valid;
						}
						valid = read_chromaticity(metadata->white_point[0], image->m_white_point[0]) && valid;
						valid = read_chromaticity(metadata->white_point[1], image->m_white_point[1]) && valid;
						image->m_has_mastering_display = valid;
					}
					if (metadata->has_luminance) {
						const double minimum = metadata->min_luminance.den != 0
							? av_q2d(metadata->min_luminance) : -1.0;
						const double maximum = metadata->max_luminance.den != 0
							? av_q2d(metadata->max_luminance) : -1.0;
						if (std::isfinite(minimum) && std::isfinite(maximum)
							&& minimum >= 0.0 && maximum > 0.0 && maximum >= minimum) {
							image->m_min_luminance_nits = minimum;
							image->m_max_luminance_nits = maximum;
							image->m_has_mastering_display = true;
						}
					}
				}
				if (const auto* side = av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
					side && side->size >= sizeof(AVContentLightMetadata)) {
					const auto* metadata = reinterpret_cast<const AVContentLightMetadata*>(side->data);
					image->m_max_content_light_nits = metadata->MaxCLL;
					image->m_max_frame_average_light_nits = metadata->MaxFALL;
					image->m_has_content_light = true;
				}
				Event event;
				event.m_start = std::max(start, m_target);
				event.m_end = start + duration;
				event.m_bytes = bytes;
				event.m_hdr_video = std::move(image);
				const bool accepted = Enqueue(std::move(event));
				if (accepted && m_preview)
					m_preview_video_ready = true;
				return accepted;
			}
			PreviewFrame display_rgb;
			std::array<const std::uint8_t*, 4> source_data{};
			std::array<int, 4> source_linesize{};
			AVPixelFormat source_format = static_cast<AVPixelFormat>(frame->format);
			for (int plane = 0; plane < 4; ++plane) {
				source_data[plane] = frame->data[plane];
				source_linesize[plane] = frame->linesize[plane];
			}
			if (transfer == AVCOL_TRC_SMPTE2084 || transfer == AVCOL_TRC_ARIB_STD_B67) {
				PreviewFrame source;
				source.Reset(av_frame_clone(frame));
				if (!source)
					throw Exception("preview HDR frame reference failed");
				PreviewFrame linear;
				linear.Format(AV_PIX_FMT_GBRPF32LE);
				PreviewFrame::ColorTarget linear_target{
					.transfer = AVCOL_TRC_LINEAR,
					.primaries = AVCOL_PRI_BT709,
					.matrix = AVCOL_SPC_RGB,
					.range = AVCOL_RANGE_JPEG,
					.peak = 100.0
				};
				if (!m_decoders[0].m_color_scale)
					m_decoders[0].m_color_scale.emplace();
				if (!source.ScaleTo(*m_decoders[0].m_color_scale, linear, frame->width, frame->height,
						PreviewFrame::Resample::Default, PreviewFrame::Scaler::Zimg, linear_target))
					throw Exception("preview HDR linear-light conversion failed");
				linear.ColorTransfer(AVCOL_TRC_LINEAR);
				linear.ColorPrimaries(AVCOL_PRI_BT709);
				linear.ColorSpace(AVCOL_SPC_RGB);
				linear.ColorRange(AVCOL_RANGE_JPEG);
				constexpr std::string_view chain = "tonemap=tonemap=hable:desat=0";
				if (!m_decoders[0].m_tonemap) {
					auto graph = FFmpeg::AVFilterGraph::Open(linear, chain);
					if (!graph)
						throw Exception("preview HDR tone-mapping filter is unavailable");
					m_decoders[0].m_tonemap.emplace(std::move(graph));
				}
				else if (!m_decoders[0].m_tonemap->Ensure(linear, chain))
					throw Exception("preview HDR tone-mapping filter reset failed");
				PreviewFrame curved;
				if (!m_decoders[0].m_tonemap->Filter(linear, curved))
					throw Exception("preview HDR tone-mapping failed");
				curved.ColorTransfer(AVCOL_TRC_LINEAR);
				curved.ColorPrimaries(AVCOL_PRI_BT709);
				curved.ColorSpace(AVCOL_SPC_RGB);
				curved.ColorRange(AVCOL_RANGE_JPEG);
				display_rgb.Format(AV_PIX_FMT_GBRP);
				PreviewFrame::ColorTarget sdr_target{
					.transfer = AVCOL_TRC_BT709,
					.primaries = AVCOL_PRI_BT709,
					.matrix = AVCOL_SPC_RGB,
					.range = AVCOL_RANGE_JPEG,
					.peak = 0.0
				};
				if (!curved.ScaleTo(*m_decoders[0].m_color_scale, display_rgb, frame->width, frame->height,
						PreviewFrame::Resample::Default, PreviewFrame::Scaler::Zimg, sdr_target))
					throw Exception("preview HDR BT.709 conversion failed");
				source_format = static_cast<AVPixelFormat>(display_rgb.Format());
				for (int plane = 0; plane < 4; ++plane) {
					source_data[plane] = display_rgb.Data(plane);
					source_linesize[plane] = display_rgb.Linesize(plane);
				}
			}

			auto image = Safe::MakeShared<PreviewVideo>();
			image->m_width = frame->width;
			image->m_height = frame->height;
			image->m_epoch = m_epoch;
			image->m_position = start;
			image->m_duration = duration;
			image->m_pixels.resize(ByteSize{ImageBytes(frame->width, frame->height)});
			decoder.m_scale = sws_getCachedContext(decoder.m_scale, frame->width, frame->height,
				source_format, frame->width, frame->height,
				AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
			if (!decoder.m_scale)
				throw Exception("preview pixel conversion unavailable");
			std::uint8_t* output[] = {reinterpret_cast<std::uint8_t*>(image->m_pixels.data()), nullptr, nullptr, nullptr};
			int strides[] = {frame->width * 4, 0, 0, 0};
			Check(sws_scale(decoder.m_scale, source_data.data(), source_linesize.data(),
				0, frame->height, output, strides), "convert preview video");
			Event event;
			event.m_start = std::max(start, m_target);
			event.m_end = start + duration;
			event.m_bytes = static_cast<std::size_t>(image->m_pixels.size());
			event.m_video = std::move(image);
			const bool accepted = Enqueue(std::move(event));
			if (accepted && m_preview)
				m_preview_video_ready = true;
			return accepted;
		}

		/**
		 * @brief Copy exact decoded planes and trim samples preceding the seek target.
		 * @param decoder Audio timeline state.
		 * @param frame Borrowed decoded frame.
		 * @return Whether decoding may continue.
		 */
		bool Samples(Decoder& decoder, const AVFrame* frame) {
			if (frame->nb_samples <= 0)
				return true;
			const auto format = static_cast<AVSampleFormat>(frame->format);
			const int width = av_get_bytes_per_sample(format);
			const int channels = frame->ch_layout.nb_channels;
			if (width <= 0 || channels > 1024
				|| static_cast<std::uint64_t>(frame->nb_samples) * channels * width > MaxBytes)
				throw Exception("preview audio block exceeds bounded storage");
			auto audio = Safe::MakeShared<PreviewAudio>();
			audio->m_epoch = m_epoch;
			audio->m_rate = frame->sample_rate;
			audio->m_planar = av_sample_fmt_is_planar(format);
			switch (av_get_packed_sample_fmt(format)) {
				case AV_SAMPLE_FMT_U8: audio->m_format = AudioSample::Unsigned8; break;
				case AV_SAMPLE_FMT_S16: audio->m_format = AudioSample::Signed16; break;
				case AV_SAMPLE_FMT_S32: audio->m_format = AudioSample::Signed32; break;
				case AV_SAMPLE_FMT_S64: audio->m_format = AudioSample::Signed64; break;
				case AV_SAMPLE_FMT_FLT: audio->m_format = AudioSample::Float32; break;
				case AV_SAMPLE_FMT_DBL: audio->m_format = AudioSample::Float64; break;
				default: throw Exception("unsupported decoded PCM representation");
			}
			switch (frame->ch_layout.order) {
				case AV_CHANNEL_ORDER_NATIVE: audio->m_layout = AudioLayout::Speakers; break;
				case AV_CHANNEL_ORDER_CUSTOM: audio->m_layout = AudioLayout::Custom; break;
				case AV_CHANNEL_ORDER_AMBISONIC: audio->m_layout = AudioLayout::Ambisonic; break;
				default: audio->m_layout = AudioLayout::Unspecified; break;
			}
			for (int i = 0; i < channels; ++i) {
				char identity[64]{};
				Check(av_channel_name(identity, sizeof(identity),
					av_channel_layout_channel_from_index(&frame->ch_layout, i)), "describe audio channel");
				audio->m_channels.emplace_back(identity);
			}
			const double start = decoder.m_next;
			decoder.m_next += static_cast<double>(frame->nb_samples) / frame->sample_rate;
			const int skip = static_cast<int>(std::min<double>(frame->nb_samples,
				std::max(0.0, std::ceil((std::max(m_target, m_audio_target) - start) * frame->sample_rate))));
			if (skip == frame->nb_samples)
				return true;
			audio->m_frames = frame->nb_samples - skip;
			const auto stride = static_cast<std::size_t>(width) * (audio->m_planar ? 1 : channels);
			const auto plane_bytes = audio->m_frames * stride;
			const int planes = audio->m_planar ? channels : 1;
			for (int i = 0; i < planes; ++i) {
				if (!frame->extended_data[i])
					throw Exception("missing decoded audio plane");
				Safe::Binary plane;
				plane.resize(ByteSize{plane_bytes});
				std::memcpy(plane.data(), frame->extended_data[i] + skip * stride, plane_bytes);
				audio->m_planes.push_back(std::move(plane));
			}
			audio->m_position = start + static_cast<double>(skip) / frame->sample_rate;
			Event event;
			event.m_start = audio->m_position;
			event.m_end = decoder.m_next;
			event.m_bytes = plane_bytes * planes;
			event.m_audio = std::move(audio);
			return Enqueue(std::move(event));
		}

		/**
		 * @brief Deliver unchanged decoded audio, withholding it during paused preview.
		 * @param frame Borrowed private samples.
		 * @return Whether decoding may continue.
		 */
		bool Audio(AVFrame* frame) {
			if (m_preview)
				return true;
			auto& decoder = m_decoders[1];
			if (frame->sample_rate <= 0 || frame->ch_layout.nb_channels <= 0
				|| frame->ch_layout.nb_channels > 1024 || frame->nb_samples < 0)
				throw Exception("invalid decoded preview audio format");
			const auto format = static_cast<AVSampleFormat>(frame->format);
			const bool changed = decoder.m_input_format != format
				|| decoder.m_input_rate != frame->sample_rate
				|| av_channel_layout_compare(&decoder.m_input_layout, &frame->ch_layout) != 0;
			if (changed) {
				av_channel_layout_uninit(&decoder.m_input_layout);
				Check(av_channel_layout_copy(&decoder.m_input_layout, &frame->ch_layout), "copy audio layout");
				decoder.m_input_format = format;
				decoder.m_input_rate = frame->sample_rate;
				decoder.m_audio_timed = false;
			}
			const double timestamp = Timestamp(1, frame->best_effort_timestamp);
			const double next = frame->best_effort_timestamp == AV_NOPTS_VALUE && decoder.m_audio_timed
				? decoder.m_next : timestamp;
			// Rounded container timestamps must not interrupt contiguous decoded samples.
			const double precision = std::max(1.0 / frame->sample_rate,
				2 * av_q2d(m_format->streams[m_selected[1]]->time_base));
			if (!decoder.m_audio_timed || std::abs(next - decoder.m_next) > precision)
				decoder.m_next = next;
			decoder.m_audio_timed = true;
			return Samples(decoder, frame);
		}

		/**
		 * @brief Decode timed text or bitmap rectangles into owned presentation cues.
		 * @param packet Borrowed subtitle packet.
		 * @return Whether decoding may continue.
		 */
		bool Subtitle(AVPacket* packet) {
			auto& decoder = m_decoders[2];
			AVSubtitle subtitle{};
			auto release = [](AVSubtitle* value) noexcept { avsubtitle_free(value); };
			std::unique_ptr<AVSubtitle, decltype(release)> guard(&subtitle, release);
			int got = 0;
			Check(avcodec_decode_subtitle2(decoder.m_codec, &subtitle, &got, packet), "decode preview subtitle");
			if (!got)
				return true;
			const double base = subtitle.pts != AV_NOPTS_VALUE
				? static_cast<double>(subtitle.pts) / AV_TIME_BASE - m_origin
				: Timestamp(2, packet->pts);
			auto cue = Safe::MakeShared<PreviewSubtitle>();
			cue->m_epoch = m_epoch;
			cue->m_id = ++m_cue_id;
			cue->m_start = base + subtitle.start_display_time / 1000.0;
			cue->m_end = base + subtitle.end_display_time / 1000.0;
			if (cue->m_end <= cue->m_start && packet->duration > 0)
				cue->m_end = base + packet->duration * av_q2d(m_format->streams[m_selected[2]]->time_base);
			if (!subtitle.num_rects)
				cue->m_end = cue->m_start;
			else if (cue->m_end <= cue->m_start || subtitle.end_display_time == UINT32_MAX)
				cue->m_end = std::numeric_limits<double>::infinity();
			if (cue->m_end < m_target || (m_preview && cue->m_start > m_target))
				return true;
			cue->m_canvas_width = decoder.m_codec->width;
			cue->m_canvas_height = decoder.m_codec->height;
			cue->m_fonts = m_fonts;
			if (decoder.m_codec->subtitle_header && decoder.m_codec->subtitle_header_size > 0)
				cue->m_style_header = std::string_view(reinterpret_cast<const char*>(decoder.m_codec->subtitle_header),
					decoder.m_codec->subtitle_header_size);
			Event event;
			event.m_bytes = m_font_bytes;
			for (unsigned i = 0; i < subtitle.num_rects; ++i) {
				const auto* rectangle = subtitle.rects[i];
				SubtitleRegion region;
				region.m_x = rectangle->x;
				region.m_y = rectangle->y;
				region.m_width = rectangle->w;
				region.m_height = rectangle->h;
				if (rectangle->type == SUBTITLE_BITMAP) {
					region.m_content = SubtitleContent::Bitmap;
					const auto bytes = ImageBytes(rectangle->w, rectangle->h);
					if (!rectangle->data[0] || !rectangle->data[1] || rectangle->nb_colors <= 0
						|| rectangle->linesize[0] < rectangle->w || event.m_bytes + bytes > MaxBytes)
						throw Exception("invalid or oversized subtitle bitmap");
					region.m_pixels.resize(ByteSize{bytes});
					auto* pixels = reinterpret_cast<std::uint8_t*>(region.m_pixels.data());
					for (int y = 0; y < rectangle->h; ++y) {
						for (int x = 0; x < rectangle->w; ++x) {
							const unsigned index = rectangle->data[0][y * rectangle->linesize[0] + x];
							if (index >= static_cast<unsigned>(rectangle->nb_colors))
								throw Exception("invalid subtitle palette index");
							std::uint32_t color;
							std::memcpy(&color, rectangle->data[1] + index * 4, 4);
							*pixels++ = (color >> 16) & 255;
							*pixels++ = (color >> 8) & 255;
							*pixels++ = color & 255;
							*pixels++ = (color >> 24) & 255;
						}
					}
					event.m_bytes += bytes;
				}
				else if (rectangle->type == SUBTITLE_ASS && rectangle->ass) {
					region.m_content = SubtitleContent::Ass;
					region.m_text = std::string_view(rectangle->ass);
				}
				else if (rectangle->type == SUBTITLE_TEXT && rectangle->text)
					region.m_text = std::string_view(rectangle->text);
				else
					throw Exception("unsupported decoded subtitle rectangle");
				event.m_bytes += static_cast<std::size_t>(region.m_text.size());
				cue->m_regions.push_back(std::move(region));
			}
			event.m_bytes += static_cast<std::size_t>(cue->m_style_header.size());
			event.m_start = std::max(m_target, cue->m_start);
			event.m_end = cue->m_end;
			if (m_open_cue) {
				auto clear = Safe::MakeShared<PreviewSubtitle>();
				clear->m_epoch = m_epoch;
				clear->m_id = m_open_cue;
				clear->m_start = event.m_start;
				clear->m_end = event.m_start;
				Event closing;
				closing.m_start = event.m_start;
				closing.m_end = event.m_start;
				closing.m_subtitle = std::move(clear);
				if (!Enqueue(std::move(closing)))
					return false;
				m_open_cue = 0;
			}
			if (!std::isfinite(cue->m_end)) {
				m_open_cue = cue->m_id;
				m_open_cue_start = cue->m_start;
			}
			if (cue->m_regions.empty())
				cue->m_id = 0;
			event.m_subtitle = std::move(cue);
			return Enqueue(std::move(event));
		}

		/**
		 * @brief Drain available pictures or samples from a selected codec.
		 * @param slot Decoder slot.
		 * @param frame Reusable private scratch.
		 * @return Whether decoding may continue.
		 */
		bool Receive(int slot, AVFrame* frame) {
			auto& decoder = m_decoders[slot];
			while (!m_cancel && !m_decode_stop) {
				const int result = avcodec_receive_frame(decoder.m_codec, frame);
				if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
					return true;
				Check(result, "receive preview frame");
				const bool accepted = slot == 0 ? Video(frame) : Audio(frame);
				av_frame_unref(frame);
				if (!accepted)
					return false;
			}
			return false;
		}

		/**
		 * @brief Reject corruption but discard one verified AVI byte-clock seek fragment.
		 * @param packet First audio packet after seeking, or its following complete frame.
		 * @return Whether the packet is a complete decoding unit.
		 * @throws StormByte::Exception If the seek prefix is not bounded by a valid frame.
		 */
		bool SeekAudioPacket(const AVPacket* packet) {
			if (!m_seek_audio && !m_seek_prefix_size)
				return true;
			m_seek_audio = false;
			const auto* stream = m_format->streams[packet->stream_index];
			if (stream->codecpar->codec_id != AV_CODEC_ID_AC3
				|| std::strcmp(m_format->iformat->name, "avi") != 0)
				return true;
			std::uint8_t id = 0;
			std::uint16_t frame_size = 0;
			const bool header = av_ac3_parse_header(packet->data, packet->size, &id, &frame_size) >= 0;
			if (m_seek_prefix_size) {
				const bool boundary = header && id <= 10 && packet->size == frame_size
					&& m_seek_prefix_size < frame_size && packet->pts != AV_NOPTS_VALUE
					&& packet->pts > m_seek_prefix_pts
					&& packet->pts - m_seek_prefix_pts == frame_size
					&& stream->codecpar->sample_rate > 0
					&& Timestamp(1, packet->pts) <= m_target + 1536.0 / stream->codecpar->sample_rate;
				if (boundary)
					std::fprintf(stderr, "SimplePlayer: verified AVI AC-3 seek boundary position=%.6f frame-bytes=%u discarded-bytes=%d\n",
						Timestamp(1, packet->pts), frame_size, m_seek_prefix_size);
				m_seek_prefix_size = 0;
				if (!boundary)
					throw Exception("invalid AVI AC-3 seek fragment boundary");
				return true;
			}
			// AVI's byte-clock chunks can start inside AC-3 frames. Its parser
			// emits that prefix before the first synchronized frame after seek.
			const bool sync = packet->size >= 2 && ((packet->data[0] == 0x0b && packet->data[1] == 0x77)
				|| (packet->data[0] == 0x77 && packet->data[1] == 0x0b));
			if (!header && !sync && packet->size > 0 && packet->size < 3840
				&& packet->pts != AV_NOPTS_VALUE && Timestamp(1, packet->pts) <= m_target
				&& stream->time_base.num == 1 && stream->codecpar->bit_rate > 0
				&& stream->time_base.den == stream->codecpar->bit_rate / 8) {
				m_seek_prefix_size = packet->size;
				m_seek_prefix_pts = packet->pts;
				std::fprintf(stderr, "SimplePlayer: AVI AC-3 seek preroll fragment bytes=%d position=%.6f target=%.6f\n",
					packet->size, Timestamp(1, packet->pts), m_target);
				return false;
			}
			return true;
		}

		/**
		 * @brief Read, decode and drain selected streams on the decode worker.
		 */
		void Decode() noexcept {
			try {
				Scratch scratch;
				for (int slot = 0; slot < 2; ++slot)
					if (m_decoders[slot].m_codec && !Receive(slot, scratch.m_frame))
						return;
				while (!m_cancel && !m_decode_stop) {
					{
						std::unique_lock lock(m_mutex);
						m_ready.wait(lock, [&] { return m_cancel || m_decode_stop || !m_paused || m_preview; });
					}
					if (m_cancel || m_decode_stop)
						return;
					const int result = av_read_frame(m_format, scratch.m_packet);
					if (result == AVERROR(EAGAIN)) {
						std::unique_lock lock(m_mutex);
						m_ready.wait_for(lock, std::chrono::milliseconds(5), [&] { return m_cancel.load(); });
						m_format->pb->error = 0;
						m_format->pb->eof_reached = 0;
						continue;
					}
					if (result == AVERROR_EOF)
						break;
					Check(result, "read preview packet");
					int slot = -1;
					for (int i = 0; i < 3; ++i)
						if (m_selected[i] == scratch.m_packet->stream_index)
							slot = i;
					if (slot == 2) {
						if (m_preview && Timestamp(2, scratch.m_packet->pts) > m_target)
							m_preview_subtitle_ready = true;
						if (!Subtitle(scratch.m_packet))
							return;
					}
					else if (slot >= 0) {
						if (slot == 1 && !SeekAudioPacket(scratch.m_packet)) {
							av_packet_unref(scratch.m_packet);
							continue;
						}
						int sent = avcodec_send_packet(m_decoders[slot].m_codec, scratch.m_packet);
						if (sent == AVERROR(EAGAIN)) {
							if (!Receive(slot, scratch.m_frame))
								return;
							sent = avcodec_send_packet(m_decoders[slot].m_codec, scratch.m_packet);
						}
						if (sent == AVERROR_INVALIDDATA) {
							char reason[AV_ERROR_MAX_STRING_SIZE]{};
							av_strerror(sent, reason, sizeof(reason));
							const auto message = Safe::String::ReplaceToken(
								"preview packet rejected t={} pts={} bytes={}: {}; skipping packet and continuing decoding",
								scratch.m_packet->stream_index, scratch.m_packet->pts, scratch.m_packet->size, reason);
							if (m_logger)
								*m_logger << Logger::Level::Warning << std::string_view(message) << std::endl;
							else
								std::fprintf(stderr, "SimplePlayer: %s\n", message.c_str());
						}
						else
							Check(sent, "send preview packet");
						if (!Receive(slot, scratch.m_frame))
							return;
					}
					av_packet_unref(scratch.m_packet);
					if (m_preview && (m_selected[0] < 0 || m_preview_video_ready)
						&& (m_selected[2] < 0 || m_preview_subtitle_ready))
						break;
				}
				if (m_cancel || m_decode_stop)
					return;
				if (m_seek_prefix_size)
					throw Exception("incomplete AVI AC-3 seek fragment at EOF");
				{
					std::lock_guard lock(m_mutex);
					m_draining = true;
				}
				for (int slot = 0; slot < 2; ++slot) {
					if (!m_decoders[slot].m_codec)
						continue;
					Check(avcodec_send_packet(m_decoders[slot].m_codec, nullptr), "drain preview decoder");
					if (!Receive(slot, scratch.m_frame))
						return;
				}
				if (m_open_cue && !m_preview) {
					auto clear = Safe::MakeShared<PreviewSubtitle>();
					clear->m_epoch = m_epoch;
					clear->m_id = m_open_cue;
					{
						std::lock_guard lock(m_mutex);
						clear->m_start = std::max({m_end, m_duration, m_open_cue_start, m_target});
					}
					clear->m_end = clear->m_start;
					Event event;
					event.m_start = clear->m_start;
					event.m_end = clear->m_start;
					event.m_subtitle = std::move(clear);
					if (!Enqueue(std::move(event)))
						return;
					m_open_cue = 0;
				}
				{
					std::lock_guard lock(m_mutex);
					m_eof = true;
				}
				m_ready.notify_all();
			}
			catch (...) {
				try { Failure(); }
				catch (...) {
					LogFailure("unable to publish preview decode failure");
					std::lock_guard lock(m_mutex);
					m_state = PlaybackState::Error;
					m_cancel = true;
					m_ready.notify_all();
				}
			}
		}

		/**
		 * @brief Advance from audible device feedback, or a monotonic fallback.
		 * @return Current media seconds.
		 */
		double MediaClock() {
			std::lock_guard lock(m_mutex);
			const double wall = m_anchor + std::chrono::duration<double>(PlaybackClock::now() - m_start).count();
			if (m_audio_switching)
				return wall;
			if (m_selected[1] < 0 || m_audio_end <= m_target)
				return wall;
			const auto audio = m_display->AudioClock(m_epoch);
			if (!audio.m_valid)
				return wall;
			if (!std::isfinite(audio.m_position) || !std::isfinite(audio.m_latency) || audio.m_latency < 0)
				throw Exception("display supplied an invalid audio clock");
			const double audible = std::max(m_target, audio.m_position - audio.m_latency);
			if (m_audio_transition) {
				if (audible + 0.02 < wall)
					return wall;
				m_audio_transition = false;
			}
			const bool pending_audio = std::any_of(m_queue.begin(), m_queue.end(),
				[](const Event& event) { return static_cast<bool>(event.m_audio); });
			if (audio.m_drained && !pending_audio && audible + 0.001 >= m_audio_end) {
				m_anchor = std::max(m_position, audible);
				m_start = PlaybackClock::now();
				m_audio_end = m_target;
			}
			return std::max(m_position, audible);
		}

		/**
		 * @brief Schedule sink delivery and publish presentation-complete EOF.
		 */
		void Present() noexcept {
			try {
				while (!m_cancel) {
					std::unique_lock delivery(m_delivery);
					std::unique_lock lock(m_mutex);
					if (m_paused && (!m_preview || m_preview_done)) {
						delivery.unlock();
						m_ready.wait(lock, [&] { return m_cancel || !m_paused || (m_preview && !m_preview_done); });
						continue;
					}
					if (m_preview && !m_eof) {
						delivery.unlock();
						m_ready.wait(lock, [&] { return m_cancel || m_eof; });
						continue;
					}
					const bool preview = m_preview;
					lock.unlock();
					const double clock = preview ? m_target : MediaClock();
					lock.lock();
					if (!preview)
						m_position = std::max(m_position, clock);
					auto found = std::find_if(m_queue.begin(), m_queue.end(), [&](const Event& event) {
						if (preview)
							return static_cast<bool>(event.m_video) || static_cast<bool>(event.m_hdr_video)
								|| (event.m_subtitle && event.m_start <= m_target);
						return (!event.m_audio || !m_audio_switching)
							&& event.m_start <= clock + (event.m_audio ? AudioAhead : 0);
					});
					if (found != m_queue.end()) {
						Event event = std::move(*found);
						m_queue.erase(found);
						m_bytes -= event.m_bytes;
						if (event.m_audio)
							m_audio_end = std::max(m_audio_end, event.m_end);
						if (preview && (event.m_video || event.m_hdr_video)) {
							m_position = m_target;
						}
						lock.unlock();
						m_ready.notify_all();
						if (event.m_hdr_video)
							m_hdr_display->VideoHDR(std::move(event.m_hdr_video));
						else if (event.m_video)
							m_display->Video(std::move(event.m_video));
						else if (event.m_audio)
							m_display->Audio(std::move(event.m_audio));
						else
							m_display->Subtitle(std::move(event.m_subtitle));
						continue;
					}
					const bool finished = m_eof && m_queue.empty() && clock + 0.000001 >= m_end;
					const bool preview_eof = preview && m_eof && m_queue.empty();
					lock.unlock();
					m_display->Position(clock, m_epoch);
					if (preview_eof) {
						lock.lock();
						m_preview_done = true;
						return;
					}
					if (!preview && finished && m_display->Drained(m_epoch)) {
						lock.lock();
						m_position = std::max(m_target, m_end);
						m_state = PlaybackState::Ended;
						return;
					}
					delivery.unlock();
					lock.lock();
					m_ready.wait_for(lock, std::chrono::milliseconds(3), [&] { return m_cancel.load(); });
				}
			}
			catch (...) {
				try { Failure(); }
				catch (...) {
					LogFailure("unable to publish preview presentation failure");
					std::lock_guard lock(m_mutex);
					m_state = PlaybackState::Error;
					m_cancel = true;
					m_ready.notify_all();
				}
			}
		}

		/**
		 * @brief Start workers for playback or one paused seek preview.
		 * @param preview Whether to withhold audio and freeze presentation time.
		 */
		void Launch(bool preview) {
			m_cancel = false;
			m_decode_stop = false;
			m_audio_switching = false;
			m_audio_transition = false;
			m_audio_target = m_target;
			m_preview = preview;
			m_preview_done = false;
			m_preview_video_ready = false;
			m_preview_subtitle_ready = false;
			m_anchor = m_position;
			m_start = PlaybackClock::now();
			m_present = std::thread([this] { Present(); });
			try {
				if (!m_eof)
					m_decode = std::thread([this] { Decode(); });
			}
			catch (...) {
				Halt();
				throw;
			}
		}

		/**
		 * @brief Flush the old epoch and reconstruct decode at a new position.
		 * @param seconds Media target.
		 * @param playing Whether to resume playback.
		 * @return Whether seek setup succeeded.
		 */
		bool SeekTo(double seconds, bool playing) {
			Halt();
			m_cancel = false;
			if (!m_format)
				OpenFormat(false);
			Clear();
			{
				std::lock_guard lock(m_mutex);
				m_target = seconds;
				m_position = seconds;
			}
			Invalidate();
			m_seek_audio = true;
			m_seek_prefix_size = 0;
			const double absolute = ((m_selected[2] >= 0 ? 0 : seconds) + m_origin) * AV_TIME_BASE;
			if (absolute >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
				throw Exception("preview seek exceeds timestamp range");
			Check(avformat_seek_file(m_format, -1, std::numeric_limits<std::int64_t>::min(),
				static_cast<std::int64_t>(absolute), m_selected[0] >= 0
					? static_cast<std::int64_t>(absolute) : std::numeric_limits<std::int64_t>::max(), 0), "seek preview input");
			for (auto& decoder : m_decoders) {
				if (decoder.m_codec)
					avcodec_flush_buffers(decoder.m_codec);
				decoder.m_next = seconds;
				decoder.m_audio_timed = false;
			}
			{
				std::lock_guard lock(m_mutex);
				m_paused = !playing;
				m_state = playing ? PlaybackState::Playing : PlaybackState::Paused;
			}
			m_display->Pause(!playing, m_epoch);
			if (playing || m_selected[0] >= 0 || m_selected[2] >= 0)
				Launch(!playing);
			return true;
		}

		/**
		 * @brief Replace only the audio decoder at the current demux cursor.
		 * @param index Selected stream, or -1.
		 * @return True if the sink supports audio-only invalidation.
		 */
		bool SwitchAudio(int index) {
			{
				std::lock_guard delivery(m_delivery);
				const double position = MediaClock();
				std::lock_guard lock(m_mutex);
				m_anchor = std::max(m_position, position);
				m_start = PlaybackClock::now();
				m_audio_switching = true;
				m_decode_stop = true;
				// Free old PCM first so a blocked video/cue enqueue can finish.
				for (auto it = m_queue.begin(); it != m_queue.end();) {
					if (it->m_audio) {
						m_bytes -= it->m_bytes;
						it = m_queue.erase(it);
					}
					else ++it;
				}
			}
			m_ready.notify_all();
			if (m_decode.joinable())
				m_decode.join();
			if (m_cancel)
				throw Exception("audio selection interrupted by playback failure");
			{
				std::lock_guard lock(m_mutex);
				if (m_eof || m_draining)
					return false;
			}
			{
				std::lock_guard delivery(m_delivery);
				if (!m_display->InvalidateAudio(m_epoch))
					return false;
				{
					std::lock_guard lock(m_mutex);
					m_audio_end = m_position;
					m_audio_target = m_position;
					m_selected[1] = index;
				}
				OpenDecoder(1, index);
				m_decoders[1].m_next = m_audio_target;
				m_seek_audio = true;
				m_seek_prefix_size = 0;
				m_display->Pause(false, m_epoch);
				{
					std::lock_guard lock(m_mutex);
					m_audio_switching = false;
					m_audio_transition = index >= 0;
				}
			}
			m_decode_stop = false;
			m_ready.notify_all();
			m_decode = std::thread([this] { Decode(); });
			return true;
		}

		/**
		 * @brief Stop execution and rewind the retained reader.
		 * @return Whether the reader is prepared at logical offset zero.
		 */
		bool StopInput() {
			Halt();
			ResetFormat();
			Clear();
			Invalidate();
			m_display->Pause(true, m_epoch);
			if (!Rewind())
				return false;
			std::lock_guard lock(m_mutex);
			m_position = 0;
			m_target = 0;
			m_state = PlaybackState::Stopped;
			m_paused = true;
			return true;
		}

		/**
		 * @brief Stop execution, invalidate presentation and close the owned reader.
		 * @return Whether sink and reader cleanup succeeded.
		 */
		bool CloseInput() {
			Halt();
			ResetFormat();
			Clear();
			bool success = true;
			try {
				m_selected = {-1, -1, -1};
				Invalidate();
				m_display->Pause(true, m_epoch);
			}
			catch (...) { success = Failure(); }
			if (m_reader) {
				try {
					const auto result = m_reader->Close();
					if (result.status != Buffer::IO::Status::Ok)
						success = Fail("reader close failed");
				}
				catch (...) { success = Failure(); }
				m_reader.reset();
			}
			std::lock_guard lock(m_mutex);
			m_tracks.clear();
			m_position = 0;
			m_duration = 0;
			m_target = 0;
			if (success)
				m_state = PlaybackState::Closed;
			return success;
		}

		Safe::Shared<Display> m_display;							///< Application presentation owner.
		HDRDisplay* m_hdr_display = nullptr;						///< Borrowed optional HDR extension.
		Safe::Shared<Logger::Log> m_logger;							///< Scoped diagnostic owner.
		Safe::Unique<Buffer::IO::BufferedLocationReader> m_reader;	///< Exclusive dynamic input owner.
		std::optional<FileAvio> m_avio;								///< Borrowed-reader custom AVIO owner.
		AVFormatContext* m_format = nullptr;						///< Private demux context.
		std::array<Decoder, 3> m_decoders;							///< Selected video, audio and subtitle codecs.
		std::array<int, 3> m_selected{-1, -1, -1};					///< Selected stream indexes.
		Safe::Vector<PreviewTrack> m_tracks;						///< Selectable stream snapshots.
		Safe::Vector<Safe::Shared<const Attachment>> m_fonts;		///< Shared subtitle fonts.
		std::size_t m_font_bytes = 0;								///< Charged subtitle resource storage.
		mutable std::mutex m_control;								///< Serializes public controls.
		mutable std::mutex m_mutex;									///< Protects worker queues and observable state.
		std::mutex m_delivery;										///< Serializes sink callbacks with pause.
		std::condition_variable m_ready;							///< Interruptible queue and pause wakeup.
		std::atomic<bool> m_cancel{false};							///< Worker cancellation request.
		std::atomic<bool> m_decode_stop{false};						///< Stop demux at a decode boundary, retaining video state.
		std::thread m_decode;										///< Decode worker.
		std::thread m_present;										///< Presentation worker.
		std::deque<Event> m_queue;									///< Bounded scheduled presentation.
		std::size_t m_bytes = 0;									///< Charged queue storage.
		PlaybackState m_state = PlaybackState::Closed;				///< Observable lifecycle.
		Safe::String m_error;										///< Last failure.
		std::uint64_t m_epoch = 0;									///< Delivery generation.
		std::uint64_t m_cue_id = 0;									///< Next subtitle identity.
		std::uint64_t m_open_cue = 0;								///< Until-clear subtitle identity.
		double m_open_cue_start = 0;								///< Until-clear subtitle start.
		double m_position = 0;										///< Observable media position.
		double m_duration = 0;										///< Known duration, or zero.
		double m_origin = 0;										///< Container timestamp origin.
		double m_target = 0;										///< Current seek target.
		double m_audio_target = 0;									///< Audio trim boundary after a live selection.
		double m_end = 0;											///< Last scheduled presentation end.
		double m_audio_end = 0;										///< Last submitted audio end.
		double m_anchor = 0;										///< Monotonic clock media anchor.
		PlaybackClock::time_point m_start = PlaybackClock::now();	///< Monotonic clock wall anchor.
		bool m_paused = false;										///< Frozen presentation.
		bool m_audio_switching = false;								///< Use wall timing while replacing audio.
		bool m_audio_transition = false;							///< Wait for fresh device timing to catch up.
		bool m_preview = false;										///< Paused seek decode.
		bool m_preview_done = false;								///< Paused preview delivered.
		bool m_preview_video_ready = false;							///< Target image decoded.
		bool m_preview_subtitle_ready = false;						///< Subtitle target boundary reached.
		bool m_eof = false;											///< Decode or paused reconstruction complete.
		bool m_draining = false;									///< Codec EOF has started; live replacement must reconstruct.
		bool m_seek_audio = false;									///< Inspect first post-seek audio framing.
		int m_seek_prefix_size = 0;									///< Pending bounded AC-3 prefix length.
		std::int64_t m_seek_prefix_pts = 0;							///< Byte-clock prefix timestamp.
		DecodeAcceleration m_acceleration = DecodeAcceleration::Auto;	///< Video decoder acceleration policy.
		PreviewColorMode m_color_mode = PreviewColorMode::SDR;		///< Preview signal output policy.
};

Backend::SimplePlayer::SimplePlayer(Safe::Shared<Display> display, Safe::Shared<Logger::Log> logger):
	m_ctx(Safe::MakeUnique<Context>(std::move(display), std::move(logger))) {}

Backend::SimplePlayer::~SimplePlayer() noexcept {
	try { static_cast<void>(Close()); }
	catch (...) {
		m_ctx->LogFailure("preview close threw during destruction");
	}
}

bool Backend::SimplePlayer::Open(Safe::Unique<Buffer::IO::BufferedLocationReader> reader) {
	std::lock_guard control(m_ctx->m_control);
	try {
		if (!m_ctx->CloseInput()) {
			m_ctx->m_reader = std::move(reader);
			return false;
		}
		m_ctx->m_reader = std::move(reader);
		if (!m_ctx->m_reader)
			return m_ctx->Fail("preview reader is empty");
		if (!m_ctx->m_reader->IsOpen() && !m_ctx->m_reader->Open())
			return m_ctx->Fail("preview reader open failed");
		if (!m_ctx->Rewind())
			return false;
		m_ctx->OpenFormat(true);
		m_ctx->Invalidate();
		m_ctx->m_display->Pause(true, m_ctx->m_epoch);
		std::lock_guard lock(m_ctx->m_mutex);
		m_ctx->m_paused = true;
		m_ctx->m_state = PlaybackState::Stopped;
		return true;
	}
	catch (...) { return m_ctx->Failure(); }
}

void Backend::SimplePlayer::Acceleration(DecodeAcceleration acceleration) noexcept {
	std::lock_guard control(m_ctx->m_control);
	m_ctx->m_acceleration = acceleration;
}

bool Backend::SimplePlayer::OutputColor(PreviewColorMode mode) {
	std::lock_guard control(m_ctx->m_control);
	switch (mode) {
		case PreviewColorMode::SDR:
			break;
		case PreviewColorMode::HDR:
			if (!m_ctx->m_hdr_display) {
				m_ctx->LogFailure("HDR preview requested without an HDRDisplay sink");
				return false;
			}
			break;
		default:
			m_ctx->LogFailure("preview color mode is invalid");
			return false;
	}
	m_ctx->m_color_mode = mode;
	return true;
}

bool Backend::SimplePlayer::Play() {
	std::lock_guard control(m_ctx->m_control);
	try {
		const auto state = State();
		if (state == PlaybackState::Playing)
			return true;
		if (state != PlaybackState::Paused && state != PlaybackState::Stopped)
			return m_ctx->Fail("preview Play requires stopped or paused input");
		if (m_ctx->m_preview)
			return m_ctx->SeekTo(Position(), true);
		if (!m_ctx->m_present.joinable())
			m_ctx->Halt();
		if (!m_ctx->m_format)
			m_ctx->OpenFormat(false);
		{
			std::lock_guard delivery(m_ctx->m_delivery);
			m_ctx->m_display->Pause(false, m_ctx->m_epoch);
			std::lock_guard lock(m_ctx->m_mutex);
			m_ctx->m_paused = false;
			m_ctx->m_anchor = m_ctx->m_position;
			m_ctx->m_start = PlaybackClock::now();
			m_ctx->m_state = PlaybackState::Playing;
		}
		if (m_ctx->m_cancel || !m_ctx->m_present.joinable())
			m_ctx->Launch(false);
		m_ctx->m_ready.notify_all();
		return true;
	}
	catch (...) { return m_ctx->Failure(); }
}

bool Backend::SimplePlayer::Pause() {
	std::lock_guard control(m_ctx->m_control);
	try {
		if (State() == PlaybackState::Paused)
			return true;
		if (State() != PlaybackState::Playing)
			return m_ctx->Fail("preview Pause requires playing input");
		std::lock_guard delivery(m_ctx->m_delivery);
		m_ctx->m_display->Pause(true, m_ctx->m_epoch);
		std::lock_guard lock(m_ctx->m_mutex);
		m_ctx->m_paused = true;
		m_ctx->m_state = PlaybackState::Paused;
		return true;
	}
	catch (...) { return m_ctx->Failure(); }
}

bool Backend::SimplePlayer::Stop() {
	std::lock_guard control(m_ctx->m_control);
	try { return m_ctx->StopInput(); }
	catch (...) { return m_ctx->Failure(); }
}

bool Backend::SimplePlayer::Seek(double seconds) {
	std::lock_guard control(m_ctx->m_control);
	try {
		if (!m_ctx->m_reader || !std::isfinite(seconds) || seconds < 0)
			return m_ctx->Fail("invalid preview seek target or missing input");
		return m_ctx->SeekTo(seconds, State() == PlaybackState::Playing);
	}
	catch (...) { return m_ctx->Failure(); }
}

bool Backend::SimplePlayer::SelectTrack(enum StormByte::Multimedia::Type type, int index) {
	std::lock_guard control(m_ctx->m_control);
	try {
		const int slot = Slot(type);
		if (!m_ctx->m_reader || slot < 0 || index < -1)
			return m_ctx->Fail("invalid preview track selection");
		bool found = index == -1;
		for (const auto& track : m_ctx->m_tracks)
			found = found || (track.m_index == index && track.m_type == type);
		if (!found)
			return m_ctx->Fail("preview track index does not match media kind");
		const bool playing = State() == PlaybackState::Playing;
		const double position = Position();
		if (m_ctx->m_selected[slot] == index)
			return true;
		bool live_audio = false;
		{
			std::lock_guard lock(m_ctx->m_mutex);
			live_audio = slot == 1 && playing && !m_ctx->m_eof && !m_ctx->m_draining;
		}
		if (live_audio && m_ctx->SwitchAudio(index))
			return true;
		m_ctx->Halt();
		if (!m_ctx->m_format)
			m_ctx->OpenFormat(false);
		m_ctx->OpenDecoder(slot, index);
		m_ctx->m_selected[slot] = index;
		return m_ctx->SeekTo(position, playing);
	}
	catch (...) { return m_ctx->Failure(); }
}

int Backend::SimplePlayer::SelectedTrack(enum StormByte::Multimedia::Type type) const {
	std::lock_guard control(m_ctx->m_control);
	const int slot = Slot(type);
	return slot < 0 ? -1 : m_ctx->m_selected[slot];
}

Safe::Vector<PreviewTrack> Backend::SimplePlayer::Tracks() const {
	std::lock_guard lock(m_ctx->m_mutex);
	return m_ctx->m_tracks;
}

PlaybackState Backend::SimplePlayer::State() const {
	std::lock_guard lock(m_ctx->m_mutex);
	return m_ctx->m_state;
}

double Backend::SimplePlayer::Position() const {
	std::lock_guard lock(m_ctx->m_mutex);
	return m_ctx->m_position;
}

double Backend::SimplePlayer::Duration() const {
	std::lock_guard lock(m_ctx->m_mutex);
	return m_ctx->m_duration;
}

Safe::String Backend::SimplePlayer::Error() const {
	std::lock_guard lock(m_ctx->m_mutex);
	return m_ctx->m_error;
}

bool Backend::SimplePlayer::Close() {
	std::lock_guard control(m_ctx->m_control);
	try { return m_ctx->CloseInput(); }
	catch (...) { return m_ctx->Failure(); }
}

Safe::Unique<Buffer::IO::BufferedLocationReader> Backend::SimplePlayer::ReleaseReader() {
	std::lock_guard control(m_ctx->m_control);
	try {
		if (!m_ctx->StopInput())
			return {};
		m_ctx->m_selected = {-1, -1, -1};
		m_ctx->Invalidate();
		{
			std::lock_guard lock(m_ctx->m_mutex);
			m_ctx->m_tracks.clear();
			m_ctx->m_duration = 0;
			m_ctx->m_state = PlaybackState::Closed;
		}
		return std::move(m_ctx->m_reader);
	}
	catch (...) {
		m_ctx->Failure();
		return {};
	}
}

Multimedia::SimplePlayer::SimplePlayer(Safe::Shared<Display> display, Safe::Shared<Logger::Log> logger) {
	if (!display)
		throw Exception("SimplePlayer requires a nonempty Display");
	m_backend = Safe::MakeUnique<Backend::SimplePlayer>(std::move(display), std::move(logger));
}

Multimedia::SimplePlayer::~SimplePlayer() noexcept = default;

bool Multimedia::SimplePlayer::Open(std::string_view file) {
	return Open(Safe::MakeUnique<Buffer::IO::BufferedFileReader>(Safe::String(file)));
}

bool Multimedia::SimplePlayer::Open(Safe::Unique<Buffer::IO::BufferedLocationReader> reader) {
	return m_backend->Open(std::move(reader));
}

void Multimedia::SimplePlayer::Acceleration(DecodeAcceleration acceleration) noexcept {
	m_backend->Acceleration(acceleration);
}

bool Multimedia::SimplePlayer::OutputColor(PreviewColorMode mode) {
	return m_backend->OutputColor(mode);
}

bool Multimedia::SimplePlayer::Play() {
	return m_backend->Play();
}

bool Multimedia::SimplePlayer::Pause() {
	return m_backend->Pause();
}

bool Multimedia::SimplePlayer::Stop() {
	return m_backend->Stop();
}

bool Multimedia::SimplePlayer::Seek(double seconds) {
	return m_backend->Seek(seconds);
}

bool Multimedia::SimplePlayer::SelectTrack(enum StormByte::Multimedia::Type type, int index) {
	return m_backend->SelectTrack(type, index);
}

int Multimedia::SimplePlayer::SelectedTrack(enum StormByte::Multimedia::Type type) const {
	return m_backend->SelectedTrack(type);
}

Safe::Vector<PreviewTrack> Multimedia::SimplePlayer::Tracks() const {
	return m_backend->Tracks();
}

PlaybackState Multimedia::SimplePlayer::State() const {
	return m_backend->State();
}

double Multimedia::SimplePlayer::Position() const {
	return m_backend->Position();
}

double Multimedia::SimplePlayer::Duration() const {
	return m_backend->Duration();
}

Safe::String Multimedia::SimplePlayer::Error() const {
	return m_backend->Error();
}

bool Multimedia::SimplePlayer::Close() {
	return m_backend->Close();
}

Safe::Unique<Buffer::IO::BufferedLocationReader> Multimedia::SimplePlayer::ReleaseReader() {
	return m_backend->ReleaseReader();
}
