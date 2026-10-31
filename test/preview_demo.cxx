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

#include <StormByte/exception.hxx>
#include <StormByte/multimedia/simple_player.hxx>

#include "preview_audio.hxx"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL.h>

using namespace StormByte;
using namespace StormByte::Multimedia;

/**
 * @brief Surface SDL failures without exposing SDL through Multimedia.
 * @param success Whether the operation succeeded.
 */
void CheckSDL(bool success) {
	if (!success)
		throw Exception(Safe::String(SDL_GetError()));
}

/**
 * @class DemoDisplay
 * @brief Main-thread SDL video and bounded callback-driven audio for manual preview.
 *
 * Only the latest video image is retained. Text subtitles appear in the title;
 * styled rendering and bitmap subtitle composition are outside this small demo.
 */
class DemoDisplay final: public Display {
	public:
		/**
		 * @brief Open a window and renderer; audio opens when its source is known.
		 * @param diagnostics Optional bounded PCM capture directory.
		 */
		explicit DemoDisplay(std::string diagnostics = {}): m_diagnostics(std::move(diagnostics)) {
			try {
				m_window = SDL_CreateWindow("Multimedia preview", SDL_WINDOWPOS_CENTERED,
					SDL_WINDOWPOS_CENTERED, 960, 540, SDL_WINDOW_RESIZABLE);
				CheckSDL(m_window != nullptr);
				m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
				if (!m_renderer)
					m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
				CheckSDL(m_renderer != nullptr);
			}
			catch (...) {
				Destroy();
				throw;
			}
		}

		DemoDisplay(const DemoDisplay&) = delete;

		DemoDisplay(DemoDisplay&&) = delete;

		/**
		 * @brief Release SDL resources on the window's thread.
		 */
		~DemoDisplay() noexcept override {
			Destroy();
		}

		DemoDisplay& operator=(const DemoDisplay&) = delete;

		DemoDisplay& operator=(DemoDisplay&&) = delete;

		/**
		 * @brief Flush queued audio, images and subtitles synchronously.
		 * @param epoch New generation.
		 * @param active_video Whether a video track is active.
		 */
		void Invalidate(std::uint64_t epoch, bool active_video) override {
			std::lock_guard lock(m_mutex);
			m_epoch = epoch;
			m_image.reset();
			m_cues.clear();
			m_audio_valid = false;
			m_priming = true;
			m_audio_end = 0;
			m_active_video = active_video;
			m_dump.close();
			m_dump_bytes = 0;
			m_dump_part = 0;
			if (m_device)
				SDL_PauseAudioDevice(m_device, 1);
			if (m_stream)
				SDL_AudioStreamClear(m_stream);
			std::lock_guard audio(m_audio_mutex);
			m_audio_read = 0;
			m_audio_size = 0;
			m_audio_position = 0;
			m_callback_previous = {};
		}

		/**
		 * @brief Freeze or resume audio.
		 * @param paused Device pause state.
		 * @param epoch Generation.
		 */
		void Pause(bool paused, std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			if (epoch == m_epoch) {
				m_paused = paused;
				if (m_device)
					SDL_PauseAudioDevice(m_device, paused || m_priming ? 1 : 0);
				if (paused) {
					std::lock_guard audio(m_audio_mutex);
					m_callback_previous = {};
				}
			}
		}

		/**
		 * @brief Retain only the latest current-generation picture.
		 * @param image Owned image.
		 */
		void Video(Safe::Shared<const PreviewVideo> image) override {
			std::lock_guard lock(m_mutex);
			if (image->m_epoch == m_epoch) {
				if (m_video_seen_epoch != m_epoch) {
					std::uint64_t hash = 14695981039346656037ULL;
					for (const auto pixel : image->m_pixels) {
						hash ^= std::to_integer<std::uint8_t>(pixel);
						hash *= 1099511628211ULL;
					}
					std::cout << "Video epoch=" << m_epoch << " position=" << image->m_position
						<< " duration=" << image->m_duration << " image-hash=" << hash << std::endl;
					m_video_seen_epoch = m_epoch;
				}
				m_image = std::move(image);
				++m_videos;
			}
		}

		/**
		 * @brief Queue device samples, inserting timeline silence for gaps.
		 * @param audio Exact decoded samples, converted only by this adapter.
		 */
		void Audio(Safe::Shared<const PreviewAudio> audio) override {
			std::lock_guard lock(m_mutex);
			if (audio->m_epoch != m_epoch)
				return;
			ConfigureAudio(audio);
			if (m_audio_seen_epoch != m_epoch) {
				std::cout << "Audio epoch=" << m_epoch << " position=" << audio->m_position
					<< " frames=" << audio->m_frames << " rate=" << audio->m_rate
					<< " channels=" << audio->m_channels.size() << std::endl;
				m_audio_seen_epoch = m_epoch;
			}
			const auto channels = static_cast<std::size_t>(audio->m_channels.size());
			const auto packed = PreviewAdapter::Pack(*audio, m_channel_map);
			CheckSDL(SDL_AudioStreamPut(m_stream, packed.data(), static_cast<int>(packed.size())) == 0);
			const int available = SDL_AudioStreamAvailable(m_stream);
			CheckSDL(available >= 0);
			if (available > static_cast<int>(m_audio_buffer.size() * sizeof(float)))
				throw Exception("demo converted audio exceeds its one-second bound");
			std::vector<float> converted(static_cast<std::size_t>(available) / sizeof(float));
			if (available)
				CheckSDL(SDL_AudioStreamGet(m_stream, converted.data(), available) == available);
			const double gap = m_audio_valid ? audio->m_position - m_audio_end : 0;
			const auto gap_samples = static_cast<std::size_t>(std::llround(std::min(1.0, std::max(0.0, gap)) * m_rate)) * channels;
			if (m_audio_valid)
				m_timestamp_gap = std::max(m_timestamp_gap, std::abs(gap));
			bool start_device = false;
			{
				std::lock_guard device(m_audio_mutex);
				if (gap > 0.5 || m_audio_size + gap_samples + converted.size() > m_audio_buffer.size())
					throw Exception("demo audio queue exceeded its one-second bound");
				if (!m_audio_valid)
					m_audio_position = audio->m_position;
				for (std::size_t i = 0; i < gap_samples; ++i)
					m_audio_buffer[(m_audio_read + m_audio_size++) % m_audio_buffer.size()] = 0;
				m_silence_frames += gap_samples / channels;
				for (const float sample : converted) {
					m_audio_buffer[(m_audio_read + m_audio_size++) % m_audio_buffer.size()] = sample;
					m_peak = std::max(m_peak, std::abs(sample));
					if (std::abs(sample) > 1)
						++m_clipped;
				}
				start_device = m_priming && !m_paused && m_audio_size >= m_rate * channels * 0.08;
			}
			m_audio_end = audio->m_position + static_cast<double>(audio->m_frames) / audio->m_rate;
			m_audio_valid = true;
			if (start_device) {
				m_priming = false;
				SDL_PauseAudioDevice(m_device, 0);
			}
			m_audio_delivery = std::chrono::steady_clock::now();
			if (m_dump.is_open() && m_dump_bytes < 32 * 1024 * 1024) {
				const auto frame_bytes = channels * sizeof(float);
				const auto bytes = std::min(converted.size() * sizeof(float),
					(32 * 1024 * 1024 - m_dump_bytes) / frame_bytes * frame_bytes);
				m_dump.write(reinterpret_cast<const char*>(converted.data()),
					static_cast<std::streamsize>(bytes));
				if (!m_dump)
					throw Exception("cannot write diagnostic PCM output");
				m_dump_bytes += bytes;
			}
			++m_audios;
		}

		/**
		 * @brief Retain bounded subtitle text for the window title.
		 * @param cue Owned cue.
		 */
		void Subtitle(Safe::Shared<const PreviewSubtitle> cue) override {
			std::lock_guard lock(m_mutex);
			if (cue->m_epoch != m_epoch)
				return;
			if (cue->m_regions.empty())
				std::erase_if(m_cues, [&](const auto& value) { return !cue->m_id || value->m_id == cue->m_id; });
			else {
				if (m_cues.size() >= 64)
					throw Exception("demo subtitle queue exceeded its bound");
				m_cues.push_back(std::move(cue));
			}
		}

		/**
		 * @brief Report consumed sample time and estimated hardware period.
		 * @param epoch Requested generation.
		 * @return Device clock feedback.
		 */
		AudioPresentation AudioClock(std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			if (epoch == m_epoch && m_priming && m_audio_valid && !m_paused
				&& std::chrono::duration<double>(std::chrono::steady_clock::now() - m_audio_delivery).count() >= 0.08) {
				m_priming = false;
				SDL_PauseAudioDevice(m_device, 0);
			}
			AudioPresentation clock;
			clock.m_valid = epoch == m_epoch && m_audio_valid && !m_priming;
			std::lock_guard device(m_audio_mutex);
			const double queued = m_rate ? static_cast<double>(m_audio_size) / (m_rate * m_channel_count) : 0;
			clock.m_position = m_audio_position;
			clock.m_latency = queued > 0 ? m_latency : 0;
			clock.m_drained = queued == 0
				&& std::chrono::duration<double>(std::chrono::steady_clock::now() - m_audio_consumed).count() >= m_latency;
			return clock;
		}

		/**
		 * @brief Expire cues against the engine's presentation clock.
		 * @param position Media time.
		 * @param epoch Requested generation.
		 */
		void Position(double position, std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			if (epoch == m_epoch)
				std::erase_if(m_cues, [&](const auto& cue) { return cue->m_end <= position; });
		}

		/**
		 * @brief Report audio queue drain after GUI consumption of the latest image.
		 * @param epoch Requested generation.
		 * @return Whether queued presentation completed.
		 */
		bool Drained(std::uint64_t epoch) override {
			const auto clock = AudioClock(epoch);
			std::lock_guard lock(m_mutex);
			return epoch == m_epoch && (!m_audio_valid || clock.m_drained) && m_cues.empty();
		}

		/**
		 * @brief Paint the latest image on the SDL thread and update playback telemetry.
		 * @param position Current media time.
		 * @param duration Known media duration.
		 */
		void Paint(double position, double duration) {
			Safe::Shared<const PreviewVideo> image;
			std::string subtitle;
			std::uint64_t epoch;
			{
				std::lock_guard lock(m_mutex);
				image = m_active_video ? m_image : Safe::Shared<const PreviewVideo>{};
				epoch = m_epoch;
				for (const auto& cue : m_cues)
					for (const auto& region : cue->m_regions)
						subtitle += " | " + std::string(std::string_view(region.m_text));
			}
			CheckSDL(SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255) == 0);
			CheckSDL(SDL_RenderClear(m_renderer) == 0);
			if (image) {
				if (!m_texture || m_width != image->m_width || m_height != image->m_height) {
					SDL_DestroyTexture(m_texture);
					m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
						SDL_TEXTUREACCESS_STREAMING, image->m_width, image->m_height);
					CheckSDL(m_texture != nullptr);
					m_width = image->m_width;
					m_height = image->m_height;
				}
				CheckSDL(SDL_UpdateTexture(m_texture, nullptr, image->m_pixels.data(), m_width * 4) == 0);
				int width, height;
				CheckSDL(SDL_GetRendererOutputSize(m_renderer, &width, &height) == 0);
				const double scale = std::min(static_cast<double>(width) / m_width,
					static_cast<double>(height) / m_height);
				SDL_Rect rectangle{0, 0, static_cast<int>(m_width * scale), static_cast<int>(m_height * scale)};
				rectangle.x = (width - rectangle.w) / 2;
				rectangle.y = (height - rectangle.h) / 2;
				std::lock_guard lock(m_mutex);
				if (epoch == m_epoch)
					CheckSDL(SDL_RenderCopy(m_renderer, m_texture, nullptr, &rectangle) == 0);
			}
			SDL_RenderPresent(m_renderer);
			std::string title = "Multimedia " + std::to_string(position) + " / " + std::to_string(duration) + subtitle;
			SDL_SetWindowTitle(m_window, title.c_str());
		}

		/**
		 * @brief Print actual image and audio device deliveries.
		 */
		void Report() {
			std::lock_guard lock(m_mutex);
			std::lock_guard audio(m_audio_mutex);
			std::cout << "Delivered video=" << m_videos << " audio=" << m_audios
				<< " callback-underflows=" << m_underflows << " missing-frames=" << m_missing_frames
				<< " callbacks=" << m_callbacks << " played-frames=" << m_played_frames
				<< " peak=" << m_peak << " samples-over-full-scale=" << m_clipped
				<< " inserted-silence-frames=" << m_silence_frames
				<< " max-callback-gap-ms=" << m_callback_gap * 1000
				<< " max-callback-lock-ms=" << m_callback_lock * 1000
				<< " max-submit-timestamp-gap=" << m_timestamp_gap << std::endl;
			if (!m_audios && !m_videos)
				throw Exception("demo produced no audio or video");
		}

	private:
		/**
		 * @brief Negotiate all source channels and explicitly map SDL speaker order.
		 * @param audio Source contract.
		 *
		 * Called with the control mutex only. Closing/pausing may wait for the
		 * callback, which takes only the independent audio mutex.
		 */
		void ConfigureAudio(const Safe::Shared<const PreviewAudio>& audio) {
			const bool changed = !m_source || m_source->m_rate != audio->m_rate
				|| m_source->m_format != audio->m_format || m_source->m_planar != audio->m_planar
				|| m_source->m_layout != audio->m_layout || m_source->m_channels != audio->m_channels;
			if (changed) {
				m_channel_map = PreviewAdapter::ChannelMap(*audio);
				if (m_device) {
					SDL_CloseAudioDevice(m_device);
					m_device = 0;
				}
				SDL_FreeAudioStream(m_stream);
				m_stream = nullptr;
				SDL_AudioSpec desired{};
				desired.freq = audio->m_rate;
				desired.format = AUDIO_F32SYS;
				desired.channels = static_cast<Uint8>(m_channel_map.size());
				desired.samples = 1024;
				desired.callback = AudioPull;
				desired.userdata = this;
				SDL_AudioSpec obtained{};
				m_device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
				CheckSDL(m_device != 0);
				if (obtained.channels != desired.channels || obtained.freq != desired.freq || obtained.format != AUDIO_F32SYS)
					throw Exception("SDL device did not retain the requested rate/channels/float format");
				SDL_AudioFormat input;
				switch (audio->m_format) {
					case AudioSample::Unsigned8: input = AUDIO_U8; break;
					case AudioSample::Signed16: input = AUDIO_S16SYS; break;
					case AudioSample::Signed32: input = AUDIO_S32SYS; break;
					default: input = AUDIO_F32SYS; break;
				}
				m_stream = SDL_NewAudioStream(input, desired.channels, desired.freq,
					obtained.format, obtained.channels, obtained.freq);
				CheckSDL(m_stream != nullptr);
				{
					std::lock_guard lock(m_audio_mutex);
					m_rate = obtained.freq;
					m_channel_count = obtained.channels;
					m_audio_buffer.assign(static_cast<std::size_t>(m_rate) * m_channel_count, 0);
					m_audio_read = 0;
					m_audio_size = 0;
					m_callback_previous = {};
				}
				m_latency = static_cast<double>(obtained.samples) / obtained.freq;
				m_audio_valid = false;
				m_priming = true;
				m_dump.close();
				m_source = audio;
				std::cout << "Source rate=" << audio->m_rate << " channels=" << audio->m_channels.size()
					<< " format=" << PreviewAdapter::SampleName(audio->m_format) << " planar=" << audio->m_planar
					<< " layout-kind=" << static_cast<int>(audio->m_layout) << " order=";
				for (const auto& identity : audio->m_channels)
					std::cout << std::string_view(identity) << ',';
				std::cout << " SDL driver=" << SDL_GetCurrentAudioDriver()
					<< " device=" << SDL_GetAudioDeviceName(0, 0) << " obtained-rate=" << obtained.freq
					<< " obtained-channels=" << static_cast<int>(obtained.channels)
					<< " obtained-format=f32(" << obtained.format << ") callback-frames=" << obtained.samples << " order=";
				for (const auto index : m_channel_map)
					std::cout << std::string_view(audio->m_channels[index]) << ',';
				std::cout << std::endl;
			}
			if (!m_diagnostics.empty() && !m_dump.is_open()
				&& 32 * 1024 * 1024 - m_dump_bytes >= m_channel_count * sizeof(float)) {
				std::filesystem::create_directories(m_diagnostics);
				const auto path = std::filesystem::path(m_diagnostics)
					/ ("epoch-" + std::to_string(m_epoch) + "-part-" + std::to_string(++m_dump_part));
				m_dump.open(path.string() + ".f32", std::ios::binary | std::ios::trunc);
				std::ofstream metadata(path.string() + ".txt");
				if (!m_dump || !metadata)
					throw Exception("cannot open diagnostic PCM output/metadata");
				metadata << std::setprecision(17) << "Captured adapter PCM: native-endian float32 interleaved, before gap silence\n"
					<< "rate=" << m_rate << "\nchannels=" << m_channel_count << "\nfirst-position=" << audio->m_position
					<< "\nendianness=" << (SDL_BYTEORDER == SDL_LIL_ENDIAN ? "little" : "big")
					<< "\nsource-format=" << PreviewAdapter::SampleName(audio->m_format) << "\nsource-planar=" << audio->m_planar
					<< "\norder=";
				for (const auto index : m_channel_map)
					metadata << std::string_view(audio->m_channels[index]) << ',';
				metadata << '\n';
				if (!metadata)
					throw Exception("cannot write diagnostic PCM metadata");
			}
		}

		/**
		 * @brief Feed actual hardware requests and count every short read.
		 * @param opaque Display.
		 * @param output Device buffer.
		 * @param bytes Requested bytes.
		 */
		static void AudioPull(void* opaque, Uint8* output, int bytes) noexcept {
			auto& self = *static_cast<DemoDisplay*>(opaque);
			const auto requested_at = std::chrono::steady_clock::now();
			std::memset(output, 0, bytes);
			std::lock_guard lock(self.m_audio_mutex);
			const auto acquired = std::chrono::steady_clock::now();
			self.m_callback_lock = std::max(self.m_callback_lock,
				std::chrono::duration<double>(acquired - requested_at).count());
			if (self.m_callback_previous != std::chrono::steady_clock::time_point{})
				self.m_callback_gap = std::max(self.m_callback_gap,
					std::chrono::duration<double>(requested_at - self.m_callback_previous).count());
			self.m_callback_previous = requested_at;
			const auto requested = static_cast<std::size_t>(bytes) / sizeof(float);
			const auto copied = std::min(requested, self.m_audio_size);
			for (std::size_t i = 0; i < copied; ++i)
				std::memcpy(output + i * sizeof(float),
					&self.m_audio_buffer[(self.m_audio_read + i) % self.m_audio_buffer.size()], sizeof(float));
			self.m_audio_read = (self.m_audio_read + copied) % self.m_audio_buffer.size();
			self.m_audio_size -= copied;
			self.m_audio_position += static_cast<double>(copied) / (self.m_rate * self.m_channel_count);
			++self.m_callbacks;
			self.m_played_frames += copied / self.m_channel_count;
			if (copied)
				self.m_audio_consumed = std::chrono::steady_clock::now();
			if (copied < requested) {
				++self.m_underflows;
				self.m_missing_frames += (requested - copied) / self.m_channel_count;
			}
		}

		/**
		 * @brief Release partially or fully initialized SDL resources.
		 */
		void Destroy() noexcept {
			if (m_device)
				SDL_CloseAudioDevice(m_device);
			SDL_FreeAudioStream(m_stream);
			SDL_DestroyTexture(m_texture);
			SDL_DestroyRenderer(m_renderer);
			SDL_DestroyWindow(m_window);
		}

		std::mutex m_mutex;							///< Serializes devices and pending presentation.
		std::string m_diagnostics;					///< Optional private PCM diagnostic directory.
		std::ofstream m_dump;						///< Bounded outgoing PCM capture.
		std::size_t m_dump_bytes = 0;				///< Captured bytes across all parts in this epoch.
		double m_timestamp_gap = 0;					///< Largest timestamp discontinuity.
		std::uint64_t m_silence_frames = 0;			///< Silence inserted for source timestamp gaps.
		double m_callback_gap = 0;					///< Largest device callback interval.
		double m_callback_lock = 0;					///< Largest device callback lock wait.
		std::chrono::steady_clock::time_point m_callback_previous;	///< Last device request.
		std::mutex m_audio_mutex;					///< Independent device callback storage.
		std::vector<float> m_audio_buffer;			///< One second at negotiated rate/channel count.
		std::vector<std::size_t> m_channel_map;		///< SDL channel to decoded channel index.
		Safe::Shared<const PreviewAudio> m_source;	///< Current source format metadata.
		SDL_AudioStream* m_stream = nullptr;			///< Adapter-owned scalar conversion.
		std::uint32_t m_rate = 0;					///< Callback sample rate.
		std::size_t m_channel_count = 0;				///< Callback channel count.
		unsigned m_dump_part = 0;					///< Capture segment within the epoch.
		std::size_t m_audio_read = 0;				///< Ring read offset.
		std::size_t m_audio_size = 0;				///< Buffered sample count.
		double m_audio_position = 0;				///< Actual sample-consumption media cursor.
		std::uint64_t m_callbacks = 0;				///< Device callback count.
		std::uint64_t m_played_frames = 0;			///< Frames delivered to device.
		std::uint64_t m_missing_frames = 0;			///< Frames substituted by silence.
		std::chrono::steady_clock::time_point m_audio_consumed;	///< Last nonempty device read.
		SDL_Window* m_window = nullptr;				///< Main-thread window.
		SDL_Renderer* m_renderer = nullptr;			///< Main-thread renderer.
		SDL_Texture* m_texture = nullptr;			///< Main-thread video texture.
		SDL_AudioDeviceID m_device = 0;				///< All-channel callback output.
		Safe::Shared<const PreviewVideo> m_image;		///< Single latest picture.
		std::vector<Safe::Shared<const PreviewSubtitle>> m_cues;	///< Bounded active subtitles.
		std::uint64_t m_epoch = 0;					///< Current delivery generation.
		std::uint64_t m_video_seen_epoch = 0;			///< First image telemetry generation.
		std::uint64_t m_audio_seen_epoch = 0;			///< First audio telemetry generation.
		std::uint32_t m_width = 0;					///< Texture width.
		std::uint32_t m_height = 0;					///< Texture height.
		std::uint64_t m_videos = 0;					///< Actual video deliveries.
		std::uint64_t m_audios = 0;					///< Actual audio deliveries.
		double m_audio_end = 0;						///< Last queued media sample.
		double m_latency = 0;						///< Hardware buffer period.
		std::chrono::steady_clock::time_point m_audio_delivery;	///< Last submission time.
		bool m_audio_valid = false;					///< Device received audio.

		bool m_paused = true;						///< Device pause state.
		bool m_priming = true;						///< Accumulating initial device headroom.
		float m_peak = 0;							///< Maximum decoded sample amplitude.
		std::uint64_t m_clipped = 0;					///< Samples exceeding device full scale.
		std::uint64_t m_underflows = 0;				///< Actual short device callback reads.
		bool m_active_video = false;					///< Ordinary video selection.
};

/**
 * @brief Run interactive preview, or a bounded automated seek exercise with real output.
 * @param argc Argument count.
 * @param argv File and optional --smoke or --listen.
 * @return Zero after successful device playback.
 */
int main(int argc, char** argv) {
	if (argc < 2 || argc > 4) {
		std::cerr << "Usage: MultimediaPreviewDemo FILE [--smoke|--listen [DIAGNOSTIC_DIRECTORY]]\n"
			<< "Space: play/pause; Left/Right: seek 30s; Home: stop/rewind; A/V/S: cycle tracks; Esc: close\n";
		return 1;
	}
	const bool listen = argc >= 3 && std::string_view(argv[2]) == "--listen";
	const bool smoke = listen || (argc >= 3 && std::string_view(argv[2]) == "--smoke");
	if (argc >= 3 && !smoke) {
		std::cerr << "Unknown playback mode: " << argv[2] << std::endl;
		return 1;
	}
	const double segment_seconds = listen ? 30 : 3;
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		std::cerr << SDL_GetError() << std::endl;
		return 1;
	}
	int result = 0;
	try {
		auto display = Safe::MakeShared<DemoDisplay>(argc == 4 ? argv[3] : "");
		SimplePlayer player(display);
		auto check = [&](bool success) {
			if (!success)
				throw Exception(player.Error());
		};
		check(player.Open(argv[1]));
		std::cout << "Opened duration=" << player.Duration() << " seconds" << std::endl;
		for (const auto& track : player.Tracks())
			std::cout << "Track " << track.m_index << " codec=" << std::string_view(track.m_codec) << std::endl;
		check(player.Play());
		const auto started = std::chrono::steady_clock::now();
		int phase = 0;
		bool quit = false;
		while (!quit) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT)
					quit = true;
				if (event.type != SDL_KEYDOWN)
					continue;
				const auto key = event.key.keysym.sym;
				if (key == SDLK_ESCAPE)
					quit = true;
				else if (key == SDLK_SPACE)
					check(player.State() == PlaybackState::Playing ? player.Pause() : player.Play());
				else if (key == SDLK_HOME)
					check(player.Stop());
				else if (key == SDLK_LEFT || key == SDLK_RIGHT)
					check(player.Seek(std::clamp(player.Position() + (key == SDLK_LEFT ? -30 : 30),
						0.0, player.Duration() > 0 ? std::max(0.0, player.Duration() - 0.1)
							: std::numeric_limits<double>::max())));
				else if (key == SDLK_a || key == SDLK_v || key == SDLK_s) {
					const auto type = key == SDLK_a ? Multimedia::Type::Audio
						: key == SDLK_v ? Multimedia::Type::Video : Multimedia::Type::Subtitle;
					std::vector<int> choices{-1};
					for (const auto& track : player.Tracks())
						if (track.m_type == type)
							choices.push_back(track.m_index);
					const auto at = std::find(choices.begin(), choices.end(), player.SelectedTrack(type));
					const auto next = at == choices.end() || std::next(at) == choices.end() ? choices.begin() : std::next(at);
					check(player.SelectTrack(type, *next));
				}
			}
			if (player.State() == PlaybackState::Error)
				throw Exception(player.Error());
			display->Paint(player.Position(), player.Duration());
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
			if (smoke && elapsed >= segment_seconds * (phase + 1)) {
				if (phase < 3) {
					display->Report();
					const double target = player.Duration() > 0
						? player.Duration() * (phase == 0 ? 0.5 : phase == 1 ? 0.9 : 0.1)
						: phase == 0 ? 30 : phase == 1 ? 120 : 10;
					std::cout << "Seek " << target << std::endl;
					check(player.Seek(target));
					++phase;
				}
				else {
					check(player.Stop());
					check(player.Position() == 0);
					display->Report();
					quit = true;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
		check(player.Close());
	}
	catch (const Exception& error) {
		std::cerr << "Preview demo: " << error.what() << std::endl;
		result = 1;
	}
	catch (const std::exception& error) {
		std::cerr << "Preview demo: " << error.what() << std::endl;
		result = 1;
	}
	catch (...) {
		std::cerr << "Preview demo: unknown failure" << std::endl;
		result = 1;
	}
	SDL_Quit();
	return result;
}
