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
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/simple_player.hxx>

#include "preview_audio.hxx"
#include "test_helpers.hxx"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
	#include <sys/resource.h>
#endif

namespace Safe = StormByte::Safe;
namespace IO = StormByte::Buffer::IO;
using StormByte::ByteSize;
using StormByte::Multimedia::AudioPresentation;
using StormByte::Multimedia::AudioLayout;
using StormByte::Multimedia::AudioSample;
using StormByte::Multimedia::DecodeAcceleration;
using StormByte::Multimedia::Display;
using StormByte::Multimedia::HDRDisplay;
using StormByte::Multimedia::PlaybackState;
using StormByte::Multimedia::PreviewAudio;
using StormByte::Multimedia::PreviewColorMode;
using StormByte::Multimedia::PreviewHDRVideo;
using StormByte::Multimedia::PreviewSubtitle;
using StormByte::Multimedia::PreviewVideo;
using StormByte::Multimedia::SimplePlayer;
using MediaType = StormByte::Multimedia::Type;
using TestClock = std::chrono::steady_clock;

/**
 * @class FakeDisplay
 * @brief Headless clocked sink with deliberately deferred GUI delivery.
 */
class FakeDisplay final: public Display, public HDRDisplay {
	public:
		/**
		 * @brief Construct a clocked sink.
		 */
		FakeDisplay() = default;

		FakeDisplay(const FakeDisplay&) = delete;

		FakeDisplay(FakeDisplay&&) = delete;

		/**
		 * @brief Release the test sink.
		 */
		~FakeDisplay() noexcept override = default;

		FakeDisplay& operator=(const FakeDisplay&) = delete;

		FakeDisplay& operator=(FakeDisplay&&) = delete;

		/**
		 * @brief Clear active image and audio while preserving deferred stale messages.
		 * @param epoch New generation.
		 * @param active_video Selection state.
		 */
		void Invalidate(std::uint64_t epoch, bool active_video) override {
			std::lock_guard lock(m_mutex);
			if (m_throw_invalidate)
				throw StormByte::Exception("test invalidation failure");
			m_epoch = epoch;
			m_active_video = active_video;
			m_image.reset();
			m_hdr_image.reset();
			m_audio_valid = false;
			m_pending_end = 0;
			m_audible = 0;
			m_cues.clear();
			++m_invalidations;
		}

		/**
		 * @brief Freeze/resume the simulated audio device.
		 * @param paused Pause state.
		 * @param epoch Generation.
		 */
		void Pause(bool paused, std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			if (epoch != m_epoch)
				throw StormByte::Exception("stale pause");
			m_audible = Played();
			m_started = TestClock::now();
			m_paused = paused;
		}

		/**
		 * @brief Reset only simulated PCM timing for opt-in live-switch tests.
		 * @param epoch Current presentation generation.
		 * @return Whether this adapter supports audio-only invalidation.
		 */
		bool InvalidateAudio(std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			if (!m_live_audio)
				return false;
			if (m_throw_audio_invalidation)
				throw StormByte::Exception("test audio invalidation failure");
			if (epoch != m_epoch)
				throw StormByte::Exception("stale audio invalidation");
			m_audio_valid = false;
			m_audible = 0;
			m_pending_end = 0;
			m_first_audio.reset();
			++m_audio_invalidations;
			return true;
		}

		/**
		 * @brief Capture real decoded images and check A/V clock alignment.
		 * @param image Decoded image.
		 */
		void Video(Safe::Shared<const PreviewVideo> image) override {
			std::lock_guard lock(m_mutex);
			if (m_throw_video)
				throw StormByte::Exception("test display failure");
			if (image->m_epoch != m_epoch || image->m_pixels.empty())
				throw StormByte::Exception("invalid image");
			if (m_videos == 12 && m_video_delay.count())
				std::this_thread::sleep_for(m_video_delay);
			m_image = image;
			if (m_capture_deferred)
				m_deferred.push_back(image);
			m_video_positions.push_back(image->m_position);
			m_video_times.push_back(TestClock::now());
			if (m_capture_timing)
				m_video_timing.push_back({Played(), m_pending_end, m_audios, m_audio_invalidations, m_epoch});
			if (m_audio_valid && !m_paused) {
				m_max_video_lead = std::max(m_max_video_lead, image->m_position - Played());
				m_max_video_late = std::max(m_max_video_late, Played() - image->m_position);
			}
			++m_videos;
		}

		/**
		 * @brief Capture HDR signal values and their transfer metadata.
		 * @param image Decoded HDR image.
		 */
		void VideoHDR(Safe::Shared<const PreviewHDRVideo> image) override {
			std::lock_guard lock(m_mutex);
			if (!image)
				throw StormByte::Exception("empty HDR image");
			const auto expected = static_cast<std::size_t>(image->m_width) * image->m_height * 8;
			if (image->m_epoch != m_epoch || image->m_pixels.size() != expected
				|| (image->m_transfer != StormByte::Multimedia::Property::Transfer::SMPTE2084
					&& image->m_transfer != StormByte::Multimedia::Property::Transfer::ARIB_B67))
				throw StormByte::Exception("invalid HDR image");
			m_hdr_image = image;
			m_video_positions.push_back(image->m_position);
			m_video_times.push_back(TestClock::now());
			++m_videos;
		}

		/**
		 * @brief Queue exact decoded samples on the simulated device.
		 * @param audio Samples.
		 */
		void Audio(Safe::Shared<const PreviewAudio> audio) override {
			std::lock_guard lock(m_mutex);
			const auto channels = static_cast<std::size_t>(audio->m_channels.size());
			const auto width = audio->m_format == AudioSample::Unsigned8 ? 1
				: audio->m_format == AudioSample::Signed16 ? 2
				: audio->m_format == AudioSample::Signed64 || audio->m_format == AudioSample::Float64 ? 8 : 4;
			if (audio->m_epoch != m_epoch || !audio->m_frames || !audio->m_rate || !channels
				|| audio->m_planes.size() != (audio->m_planar ? channels : 1))
				throw StormByte::Exception("invalid audio");
			for (const auto& plane : audio->m_planes)
				if (static_cast<std::size_t>(plane.size()) != audio->m_frames * width * (audio->m_planar ? 1 : channels))
					throw StormByte::Exception("invalid audio plane");
			if (!m_first_audio)
				m_first_audio = audio;
			if (m_capture_audio)
				m_audio_blocks.push_back(audio);
			if (m_audio_valid && !m_paused)
				m_max_audio_underrun = std::max(m_max_audio_underrun, m_audible - m_pending_end
					+ std::chrono::duration<double>(TestClock::now() - m_started).count() * m_speed);
			if (!m_audio_valid) {
				m_audio_valid = true;
				m_audible = audio->m_position;
				m_started = TestClock::now();
			}
			else
				m_max_audio_gap = std::max(m_max_audio_gap, std::abs(audio->m_position - m_pending_end));
			m_pending_end = audio->m_position + static_cast<double>(audio->m_frames) / audio->m_rate;
			m_max_audio_ahead = std::max(m_max_audio_ahead, audio->m_position - Played());
			++m_audios;
		}

		/**
		 * @brief Capture decoded subtitle content.
		 * @param subtitle Cue.
		 */
		void Subtitle(Safe::Shared<const PreviewSubtitle> subtitle) override {
			std::lock_guard lock(m_mutex);
			if (subtitle->m_epoch != m_epoch)
				throw StormByte::Exception("stale subtitle");
			for (const auto& region : subtitle->m_regions) {
				if (region.m_content == StormByte::Multimedia::SubtitleContent::Bitmap) {
					if (region.m_pixels.empty())
						throw StormByte::Exception("empty bitmap");
					++m_bitmaps;
				}
				if (!subtitle->m_fonts.empty())
					++m_fonts;
				else if (!region.m_text.empty())
					++m_texts;
			}
			if (subtitle->m_regions.empty())
				std::erase_if(m_cues, [&](const auto& cue) { return !subtitle->m_id || cue->m_id == subtitle->m_id; });
			else
				m_cues.push_back(subtitle);
			++m_subtitles;
		}

		/**
		 * @brief Supply a latency-bearing device clock, optionally slower than wall time.
		 * @param epoch Generation.
		 * @return Current device feedback.
		 */
		AudioPresentation AudioClock(std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			AudioPresentation result;
			result.m_valid = epoch == m_epoch && m_audio_valid;
			result.m_position = Played() + 0.020;
			result.m_latency = 0.020;
			result.m_drained = Played() + 0.001 >= m_pending_end;
			if (m_force_underrun) {
				result.m_position = m_pending_end + result.m_latency;
				result.m_drained = true;
			}
			return result;
		}

		/**
		 * @brief Expire subtitles against media time.
		 * @param position Media time.
		 * @param epoch Generation.
		 */
		void Position(double position, std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			if (epoch != m_epoch)
				throw StormByte::Exception("stale position");
			std::erase_if(m_cues, [&](const auto& cue) { return position >= cue->m_end; });
		}

		/**
		 * @brief Simulate a presentation-drain barrier.
		 * @param epoch Generation.
		 * @return Whether playback is drained.
		 */
		bool Drained(std::uint64_t epoch) override {
			std::lock_guard lock(m_mutex);
			return epoch == m_epoch && !m_hold_drain && (!m_audio_valid || Played() + 0.001 >= m_pending_end)
				&& m_cues.empty();
		}

		/**
		 * @brief Execute pending GUI messages, discarding old epochs.
		 * @return Number of stale messages rejected.
		 */
		unsigned DeliverDeferred() {
			std::lock_guard lock(m_mutex);
			unsigned rejected = 0;
			for (const auto& image : m_deferred) {
				if (image->m_epoch == m_epoch)
					m_image = image;
				else
					++rejected;
			}
			m_deferred.clear();
			return rejected;
		}

		/**
		 * @brief Read the simulated audible cursor with m_mutex held.
		 * @return Media seconds.
		 */
		double Played() const {
			if (!m_audio_valid || m_paused)
				return m_audible;
			return std::min(m_pending_end, m_audible + std::chrono::duration<double>(TestClock::now() - m_started).count() * m_speed);
		}

		std::mutex m_mutex;											///< Test and callback synchronization.
		Safe::Shared<const PreviewVideo> m_image;					///< Active image.
		Safe::Shared<const PreviewHDRVideo> m_hdr_image;				///< Active HDR image.
		std::vector<Safe::Shared<const PreviewVideo>> m_deferred;	///< m_deferred GUI messages.
		std::vector<Safe::Shared<const PreviewSubtitle>> m_cues;	///< Active cues.
		std::vector<double> m_video_positions;						///< Captured decode timestamps.
		std::vector<TestClock::time_point> m_video_times;			///< Delivery wall timestamps.
		struct VideoTiming {
			double m_audio;
			double m_pending_end;
			unsigned m_audios;
			unsigned m_audio_invalidations;
			std::uint64_t m_epoch;
		};
		std::vector<VideoTiming> m_video_timing;
		bool m_capture_timing = false;
		TestClock::time_point m_started = TestClock::now();			///< Simulated device anchor.
		std::uint64_t m_epoch = 0;									///< Active generation.
		double m_audible = 0;										///< Frozen audio cursor.
		double m_pending_end = 0;									///< Last queued sample time.
		double m_max_video_lead = 0;								///< Worst video lead over audible samples.
		double m_max_video_late = 0;								///< Worst picture delivery behind audible samples.
		double m_max_audio_underrun = 0;							///< Worst device starvation before the next block.
		Safe::Shared<const PreviewAudio> m_first_audio;				///< Retained native block for contract assertions.
		std::vector<Safe::Shared<const PreviewAudio>> m_audio_blocks;	///< Optional full fixture capture.
		bool m_capture_audio = false;								///< Capture short deterministic fixtures.
		double m_max_audio_ahead = 0;								///< Worst queued audio lead.
		double m_max_audio_gap = 0;									///< Worst sample-boundary discontinuity within an epoch.
		std::chrono::milliseconds m_video_delay{0};					///< Optional one-shot slow-device callback.
		double m_speed = 1;											///< Device clock speed.
		unsigned m_videos = 0;										///< Delivered images.
		unsigned m_audios = 0;										///< Delivered audio blocks.
		unsigned m_subtitles = 0;									///< Delivered subtitle packets.
		unsigned m_texts = 0;										///< Text/styled rectangles.
		unsigned m_bitmaps = 0;										///< Bitmap rectangles.
		unsigned m_fonts = 0;										///< m_cues carrying embedded fonts.
		unsigned m_invalidations = 0;								///< Synchronous generation changes.
		unsigned m_audio_invalidations = 0;							///< PCM-only resets.
		bool m_live_audio = false;									///< Enable the optional sink operation.
		bool m_throw_audio_invalidation = false;					///< Exercise live-reset failures.
		bool m_capture_deferred = true;								///< Retain stale pictures for epoch tests.
		bool m_active_video = false;								///< Selected video state.
		bool m_audio_valid = false;									///< Device clock availability.
		bool m_paused = true;										///< Device pause state.
		bool m_hold_drain = false;									///< Artificial GUI completion barrier.
		bool m_throw_video = false;									///< Simulated callback failure.
		bool m_throw_invalidate = false;								///< Simulated cleanup callback failure.
		bool m_force_underrun = false;
};

constexpr double DefaultDeadline =
#ifdef STORMBYTE_MM_TEST_DEADLINE_SEC
	STORMBYTE_MM_TEST_DEADLINE_SEC;
#else
	5;
#endif

template<class Predicate>
bool Wait(Predicate predicate, double seconds = DefaultDeadline);
std::string Fixture(std::string_view path);

int test_preview_diagnostics() {
	using Backend = StormByte::Multimedia::PreviewDecodeBackend;
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_capture_deferred = false;
	SimplePlayer player(display);
	TEST_REQUIRE(player.Diagnostics().m_decoder.empty());
	player.Acceleration(DecodeAcceleration::Software);
	TEST_REQUIRE(player.Open(Fixture("video/bluray_like_hdr10.mkv")));
	TEST_REQUIRE(!player.Diagnostics().m_decoder.empty());
	TEST_REQUIRE(player.Diagnostics().m_backend == Backend::Unknown);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }, 15));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	const auto result = player.Diagnostics();
	TEST_REQUIRE(result.m_backend == Backend::Software);
	TEST_REQUIRE(result.m_device.empty());
	TEST_REQUIRE(result.m_converted_video > 0);
	TEST_REQUIRE(result.m_shown_video == result.m_converted_video);
	TEST_REQUIRE(result.m_linearization_seconds > 0);
	TEST_REQUIRE(result.m_tone_mapping_seconds > 0);
	TEST_REQUIRE(result.m_color_conversion_seconds > 0);
	TEST_REQUIRE(result.m_rgba_packing_seconds > 0);
	TEST_REQUIRE(result.m_download_seconds == 0);
	TEST_REQUIRE(result.m_conversion_fps > 0);
	TEST_REQUIRE(result.m_peak_queue_bytes <= StormByte::Multimedia::PreviewBuffering{}.MaxBytes);
	TEST_REQUIRE(player.Seek(.5));
	TEST_REQUIRE(player.Diagnostics().m_converted_video >= result.m_converted_video);
	TEST_REQUIRE(player.Close());
	TEST_REQUIRE(player.Diagnostics().m_decoder.empty());
	TEST_REQUIRE(player.Diagnostics().m_converted_video >= result.m_converted_video);
	player.Acceleration(DecodeAcceleration::Auto);
	TEST_REQUIRE(player.Open(Fixture("video/live_audio_switch_long.mkv")));
	TEST_REQUIRE(player.Diagnostics().m_converted_video == 0);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Diagnostics().m_shown_video > 0; }, 15));
	const auto actual = player.Diagnostics();
	TEST_REQUIRE(actual.m_backend != Backend::Unknown);
	TEST_REQUIRE((actual.m_backend == Backend::Hardware) == !actual.m_device.empty());
	{
		std::lock_guard lock(display->m_mutex);
		display->m_force_underrun = true;
	}
	TEST_REQUIRE(Wait([&] { return player.Diagnostics().m_audio_underruns == 1; }));
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	TEST_REQUIRE(player.Diagnostics().m_audio_underruns == 1);
	{
		std::lock_guard lock(display->m_mutex);
		display->m_force_underrun = false;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	{
		std::lock_guard lock(display->m_mutex);
		display->m_force_underrun = true;
	}
	TEST_REQUIRE(Wait([&] { return player.Diagnostics().m_audio_underruns == 2; }));
	TEST_REQUIRE(player.Close());
	return 0;
}

/**
 * @struct ReaderStats
 * @brief Lifetime counters surviving transfer or destruction.
 */
struct ReaderStats {
	/**
	 * @brief Construct zeroed reader counters.
	 */
	ReaderStats() = default;

	/**
	 * @brief Copy reader counters.
	 * @param other Source.
	 */
	ReaderStats(const ReaderStats& other) = default;

	/**
	 * @brief Move reader counters.
	 * @param other Source.
	 */
	ReaderStats(ReaderStats&& other) noexcept = default;

	/**
	 * @brief Release reader counters.
	 */
	~ReaderStats() noexcept = default;

	/**
	 * @brief Copy reader counters.
	 * @param other Source.
	 * @return This snapshot.
	 */
	ReaderStats& operator=(const ReaderStats& other) = default;

	/**
	 * @brief Move reader counters.
	 * @param other Source.
	 * @return This snapshot.
	 */
	ReaderStats& operator=(ReaderStats&& other) noexcept = default;

	unsigned m_opens = 0;		///< Origin open count.
	unsigned m_closes = 0;		///< Origin close count.
	unsigned m_seeks = 0;		///< Origin seek count.
	bool m_fail_seek = false;	///< Inject seek failure.
	bool m_fail_close = false;	///< Inject close failure.
	bool m_fail_read = false;	///< Inject read failure.
	bool m_released = false;	///< Permanently released remote token.
};

/**
 * @class TokenReader
 * @brief In-memory remote-like reader whose Close irreversibly releases its token.
 */
class TokenReader final: public IO::BufferedLocationReader {
	public:
		/**
		 * @brief Load a synthetic fixture into an exclusively owned reader.
		 * @param path Fixture path.
		 * @param stats Lifetime counters.
		 */
		TokenReader(std::string_view path, Safe::Shared<ReaderStats> stats):
			BufferedLocationReader(Safe::String(path), IO::Location::Remote, ByteSize{0}, ByteSize{16384},
				std::chrono::milliseconds(50), false), m_stats(std::move(stats)) {
			std::ifstream input(std::string(path), std::ios::binary | std::ios::ate);
			if (!input)
				throw StormByte::Exception("test fixture open failed");
			const auto size = static_cast<std::size_t>(input.tellg());
			m_data.resize(ByteSize{size});
			input.seekg(0);
			input.read(reinterpret_cast<char*>(m_data.data()), size);
		}

		/**
		 * @brief Own synthetic input bytes without creating a temporary file.
		 * @param data Encoded fixture bytes.
		 * @param stats Lifetime counters.
		 */
		TokenReader(Safe::Binary data, Safe::Shared<ReaderStats> stats):
			BufferedLocationReader(Safe::String("memory://preview"), IO::Location::Remote, ByteSize{0}, ByteSize{16384},
				std::chrono::milliseconds(50), false), m_data(std::move(data)), m_stats(std::move(stats)) {}

		TokenReader(const TokenReader&) = delete;

		TokenReader(TokenReader&&) = delete;

		/**
		 * @brief Close while the leaf's virtual hooks are still alive.
		 */
		~TokenReader() noexcept override {
			try { static_cast<void>(Close()); }
			catch (...) {}
		}

		TokenReader& operator=(const TokenReader&) = delete;

		TokenReader& operator=(TokenReader&&) = delete;

	protected:
		/**
		 * @brief No device probing for the deterministic memory origin.
		 * @return Empty device owner.
		 */
		Safe::Shared<StormByte::System::Device> OriginDevice() const override { return {}; }

		/**
		 * @brief Open exactly once before permanent release.
		 * @return Outcome.
		 */
		IO::Result OriginOpen() override {
			++m_stats->m_opens;
			if (m_stats->m_released)
				return {IO::Status::Failed, ByteSize{0}};
			m_cursor = 0;
			SetState(IO::State::Idle);
			return {IO::Status::Ok, ByteSize{0}};
		}

		/**
		 * @brief Irreversibly release the simulated mount token.
		 * @return Injected close outcome.
		 */
		IO::Result OriginClose() override {
			++m_stats->m_closes;
			m_stats->m_released = true;
			SetState(IO::State::Unavailable);
			return {m_stats->m_fail_close ? IO::Status::Error : IO::Status::Ok, ByteSize{0}};
		}

		/**
		 * @brief Pull bounded memory bytes.
		 * @param n Maximum length.
		 * @param dest Byte destination.
		 * @return Read outcome.
		 */
		IO::Result OriginPull(ByteSize n, StormByte::Buffer::FIFO& dest) override {
			if (m_stats->m_fail_read) {
				SetState(IO::State::Fault);
				return {IO::Status::Error, ByteSize{0}};
			}
			const auto remaining = static_cast<std::size_t>(m_data.size()) - m_cursor;
			const auto count = std::min(static_cast<std::size_t>(n), remaining);
			if (!dest.Write(std::span<const std::byte>(m_data.data() + m_cursor, count)))
				return {IO::Status::Error, ByteSize{0}};
			m_cursor += count;
			return {count == 0 ? IO::Status::End : IO::Status::Ok, ByteSize{count}};
		}

		/**
		 * @brief Seek without releasing the mount token.
		 * @param offset Byte displacement.
		 * @param mode Seek mode.
		 * @return Outcome.
		 */
		IO::Result OriginSeek(std::ptrdiff_t offset, StormByte::Buffer::Position mode) override {
			++m_stats->m_seeks;
			if (m_stats->m_fail_seek) {
				SetState(IO::State::Fault);
				return {IO::Status::Error, ByteSize{0}};
			}
			const auto target = mode == StormByte::Buffer::Position::Absolute
				? offset : static_cast<std::ptrdiff_t>(m_cursor) + offset;
			if (target < 0 || static_cast<std::size_t>(target) > static_cast<std::size_t>(m_data.size()))
				return {IO::Status::Failed, ByteSize{0}};
			m_cursor = static_cast<std::size_t>(target);
			return {IO::Status::Ok, ByteSize{m_cursor}};
		}

		/**
		 * @brief Expose memory length.
		 * @return Known size.
		 */
		Safe::Optional<ByteSize> OriginSize() const noexcept override { return m_data.size(); }

	private:
		Safe::Binary m_data;				///< Fixture bytes.
		Safe::Shared<ReaderStats> m_stats;	///< External lifetime counters.
		std::size_t m_cursor = 0;			///< Origin cursor.
};

/**
 * @brief Wait for a bounded observable result without assuming decoder speed.
 * @tparam Predicate Observable condition.
 * @param predicate Condition.
 * @param seconds Deadline.
 * @return Whether the condition occurred.
 */
template<class Predicate>
bool Wait(Predicate predicate, double seconds) {
	const auto start = TestClock::now();
	while (!predicate()) {
		if (std::chrono::duration<double>(TestClock::now() - start).count() >= seconds)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

/**
 * @brief Resolve an existing deterministic media fixture.
 * @param path Relative fixture path.
 * @return Local path.
 */
std::string Fixture(std::string_view path) {
	return std::string(STORMBYTE_TEST_FILES_DIR) + "/" + std::string(path);
}

// -------------------
// Construct
// -------------------

/**
 * @brief Reject an empty mandatory display.
 * @return Test status.
 */
int test_empty_display() {
	bool rejected = false;
	try { SimplePlayer player({}); }
	catch (const StormByte::Exception&) { rejected = true; }
	TEST_REQUIRE(rejected);
	return 0;
}

int test_buffering_policy() {
	using StormByte::Multimedia::PreviewBuffering;
	const PreviewBuffering defaults;
	TEST_REQUIRE(defaults.Preload == std::chrono::milliseconds(750));
	TEST_REQUIRE(defaults.ReadAhead == std::chrono::milliseconds(2000));
	TEST_REQUIRE(defaults.AudioAhead == std::chrono::milliseconds(500));
	TEST_REQUIRE(defaults.MaxBytes == 128u * 1024 * 1024);
	TEST_REQUIRE(defaults.MaxEvents == 256);
	for (int invalid = 0; invalid < 7; ++invalid) {
		auto policy = defaults;
		switch (invalid) {
			case 0: policy.MaxBytes = 0; break;
			case 1: policy.MaxEvents = 0; break;
			case 2: policy.ReadAhead = std::chrono::milliseconds(0); break;
			case 3: policy.Preload = std::chrono::milliseconds(-1); break;
			case 4: policy.AudioAhead = std::chrono::milliseconds(-1); break;
			case 5: policy.Preload = policy.ReadAhead + std::chrono::milliseconds(1); break;
			case 6: policy.AudioAhead = policy.ReadAhead + std::chrono::milliseconds(1); break;
		}
		bool rejected = false;
		try { SimplePlayer player(Safe::MakeShared<FakeDisplay>(), {}, policy); }
		catch (const StormByte::Exception&) { rejected = true; }
		TEST_REQUIRE(rejected);
	}
	auto policy = defaults;
	policy.MaxEvents = 1;
	policy.MaxBytes = 1024 * 1024;
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display, {}, policy);
	// Mutating the caller's copy must not change the running policy.
	policy.MaxBytes = 0;
	TEST_REQUIRE(player.Open(Fixture("audio/noise_stereo.wav")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	TEST_REQUIRE(player.Seek(1.95));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	TEST_REQUIRE(player.Close());
	policy = defaults;
	policy.MaxBytes = 1;
	SimplePlayer undersized(Safe::MakeShared<FakeDisplay>(), {}, policy);
	TEST_REQUIRE(undersized.Open(Fixture("audio/noise_stereo.wav")));
	TEST_REQUIRE(undersized.Play());
	TEST_REQUIRE(Wait([&] { return undersized.State() == PlaybackState::Error; }));
	TEST_REQUIRE(std::string_view(undersized.Error()).find("configured queue byte limit") != std::string_view::npos);
	TEST_REQUIRE(undersized.Close());
	policy = defaults;
	policy.Preload = std::chrono::milliseconds(0);
	policy.ReadAhead = std::chrono::milliseconds(250);
	policy.AudioAhead = std::chrono::milliseconds(50);
	auto low_latency = Safe::MakeShared<FakeDisplay>();
	SimplePlayer immediate(low_latency, {}, policy);
	TEST_REQUIRE(immediate.Open(Fixture("audio/noise_stereo.wav")));
	TEST_REQUIRE(immediate.Play());
	TEST_REQUIRE(Wait([&] { return immediate.State() == PlaybackState::Ended || immediate.State() == PlaybackState::Error; }));
	TEST_REQUIRE(immediate.State() == PlaybackState::Ended);
	TEST_REQUIRE(immediate.Close());
	{
		std::lock_guard lock(low_latency->m_mutex);
		TEST_REQUIRE(low_latency->m_max_audio_ahead < .10);
	}
	return 0;
}

// -------------------
// Playback
// -------------------

/**
 * @brief Write a short extensible WAV with distinct, exactly known channel samples.
 * @param path Output fixture.
 * @param rate Source sample rate.
 * @param channels Source channel count.
 * @param mask Exact WAV speaker mask.
 * @param frame_count Explicit length, or zero for the default short fixture.
 * @return Native-endian expected decoded sample bytes.
 */
std::vector<std::byte> WriteAudioFixture(const std::filesystem::path& path,
	std::uint32_t rate, std::uint16_t channels, std::uint32_t mask, std::uint32_t frame_count = 0) {
	const auto frames = frame_count ? frame_count : rate / 10;
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	auto number = [&](std::uint32_t value, unsigned width) {
		for (unsigned i = 0; i < width; ++i)
			file.put(static_cast<char>((value >> (i * 8)) & 255));
	};
	file.write("RIFF", 4);
	number(60 + frames * channels * 2, 4);
	file.write("WAVEfmt ", 8);
	number(40, 4);
	number(0xfffe, 2);
	number(channels, 2);
	number(rate, 4);
	number(rate * channels * 2, 4);
	number(channels * 2, 2);
	number(16, 2);
	number(22, 2);
	number(16, 2);
	number(mask, 4);
	number(1, 4);
	number(0, 2);
	number(0x10, 2);
	const unsigned char guid[] = {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
	file.write(reinterpret_cast<const char*>(guid), sizeof(guid));
	file.write("data", 4);
	number(frames * channels * 2, 4);
	std::vector<std::byte> expected(frames * channels * 2);
	for (std::size_t frame = 0; frame < frames; ++frame)
		for (std::size_t channel = 0; channel < channels; ++channel) {
			const auto sample = static_cast<std::int16_t>((frame * 31 + channel * 1009) % 24000 - 12000);
			number(static_cast<std::uint16_t>(sample), 2);
			std::memcpy(expected.data() + (frame * channels + channel) * 2, &sample, sizeof(sample));
		}
	if (!file)
		throw StormByte::Exception("cannot write native audio test fixture");
	return expected;
}

/**
 * @brief Check lossless PCM ownership, exact channel identities and sample continuity.
 * @param channels Ordered source speakers.
 * @param rate Source rate.
 * @param mask WAV speaker mask.
 * @return Test status.
 */
int CheckNativePcm(std::initializer_list<std::string_view> channels, std::uint32_t rate, std::uint32_t mask) {
	const auto path = OutputPath("native.wav");
	const auto expected = WriteAudioFixture(path, rate, static_cast<std::uint16_t>(channels.size()), mask);
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_capture_audio = true;
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(TestLocation(path)));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	TEST_REQUIRE(player.Close());
	std::lock_guard lock(display->m_mutex);
	TEST_REQUIRE(!display->m_audio_blocks.empty());
	std::vector<std::byte> actual;
	for (const auto& audio : display->m_audio_blocks) {
		TEST_REQUIRE(audio->m_rate == rate);
		TEST_REQUIRE(audio->m_format == AudioSample::Signed16);
		TEST_REQUIRE(!audio->m_planar);
		TEST_REQUIRE(audio->m_layout == AudioLayout::Speakers);
		TEST_REQUIRE(audio->m_channels.size() == channels.size());
		std::size_t index = 0;
		for (const auto identity : channels)
			TEST_REQUIRE(std::string_view(audio->m_channels[index++]) == identity);
		TEST_REQUIRE(audio->m_planes.size() == 1);
		const auto& plane = audio->m_planes[0];
		actual.insert(actual.end(), plane.data(), plane.data() + static_cast<std::size_t>(plane.size()));
	}
	TEST_REQUIRE(actual == expected);
	TEST_REQUIRE(display->m_max_audio_gap < 0.000001);
	return 0;
}

/**
 * @brief Test exact SDL 5.1/7.1 mapping and explicit unknown-layout rejection.
 * @return Test status.
 */
int test_audio_adapter_mapping() {
	PreviewAudio audio;
	audio.m_rate = 48000;
	audio.m_layout = AudioLayout::Custom;
	for (const auto identity : {"SR", "FL", "FC", "FR", "SL", "LFE"})
		audio.m_channels.emplace_back(identity);
	const std::vector<std::size_t> expected{1, 3, 2, 5, 4, 0};
	TEST_REQUIRE(PreviewAdapter::ChannelMap(audio) == expected);
	audio.m_frames = 2;
	audio.m_format = AudioSample::Float32;
	audio.m_planar = true;
	for (std::size_t channel = 0; channel < 6; ++channel) {
		Safe::Binary plane;
		plane.resize(ByteSize{2 * sizeof(float)});
		for (std::size_t frame = 0; frame < 2; ++frame) {
			const float value = static_cast<float>(channel * 10 + frame);
			std::memcpy(plane.data() + frame * sizeof(float), &value, sizeof(value));
		}
		audio.m_planes.push_back(std::move(plane));
	}
	const auto packed = PreviewAdapter::Pack(audio, expected);
	TEST_REQUIRE(packed.size() == 2 * 6 * sizeof(float));
	for (std::size_t frame = 0; frame < 2; ++frame)
		for (std::size_t channel = 0; channel < 6; ++channel) {
			float value;
			std::memcpy(&value, packed.data() + (frame * 6 + channel) * sizeof(float), sizeof(value));
			TEST_REQUIRE(value == expected[channel] * 10 + frame);
		}
	audio.m_channels.clear();
	for (const auto identity : {"FL", "FR", "FC", "LFE", "BL", "BR", "SL", "SR"})
		audio.m_channels.emplace_back(identity);
	const std::vector<std::size_t> identity{0, 1, 2, 3, 4, 5, 6, 7};
	TEST_REQUIRE(PreviewAdapter::ChannelMap(audio) == identity);
	for (const auto layout : {AudioLayout::Unspecified, AudioLayout::Ambisonic}) {
		audio.m_layout = layout;
		bool rejected = false;
		try { PreviewAdapter::ChannelMap(audio); }
		catch (const StormByte::Exception&) { rejected = true; }
		TEST_REQUIRE(rejected);
	}
	audio.m_layout = AudioLayout::Custom;
	for (const auto unknown : {"UNK", "NONE", "AMBI0", "USR123", "FL"}) {
		audio.m_channels[1] = Safe::String(unknown);
		bool rejected = false;
		try { PreviewAdapter::ChannelMap(audio); }
		catch (const StormByte::Exception&) { rejected = true; }
		TEST_REQUIRE(rejected);
	}
	audio.m_channels.clear();
	for (const auto speaker : {"FL", "FR", "FC", "LFE", "SL", "BR"})
		audio.m_channels.emplace_back(speaker);
	bool rejected = false;
	try { PreviewAdapter::ChannelMap(audio); }
	catch (const StormByte::Exception&) { rejected = true; }
	TEST_REQUIRE(rejected);
	return 0;
}

/**
 * @brief Verify native byte interleaving and adapter-only wide-scalar conversion.
 * @return Test status.
 */
int test_audio_adapter_samples() {
	PreviewAudio audio;
	audio.m_rate = 44100;
	audio.m_layout = AudioLayout::Custom;
	audio.m_channels.emplace_back("FR");
	audio.m_channels.emplace_back("FL");
	audio.m_frames = 2;
	const auto mapping = PreviewAdapter::ChannelMap(audio);
	for (const auto format : {AudioSample::Unsigned8, AudioSample::Signed16, AudioSample::Signed32, AudioSample::Float32}) {
		audio.m_format = format;
		const auto width = PreviewAdapter::SampleBytes(format);
		for (const bool planar : {false, true}) {
			audio.m_planar = planar;
			audio.m_planes.clear();
			for (std::size_t plane = 0; plane < (planar ? 2 : 1); ++plane) {
				Safe::Binary bytes;
				bytes.resize(ByteSize{2 * width * (planar ? 1 : 2)});
				for (std::size_t i = 0; i < static_cast<std::size_t>(bytes.size()); ++i)
					bytes[i] = static_cast<std::byte>(plane * 30 + i);
				audio.m_planes.push_back(std::move(bytes));
			}
			const auto packed = PreviewAdapter::Pack(audio, mapping);
			TEST_REQUIRE(packed.size() == 4 * width);
			for (std::size_t frame = 0; frame < 2; ++frame)
				for (std::size_t channel = 0; channel < 2; ++channel) {
					const auto source = mapping[channel];
					const auto& plane = audio.m_planes[planar ? source : 0];
					const auto offset = (planar ? frame : frame * 2 + source) * width;
					TEST_REQUIRE(std::memcmp(packed.data() + (frame * 2 + channel) * width,
						plane.data() + offset, width) == 0);
				}
		}
	}
	audio.m_planar = false;
	for (const auto format : {AudioSample::Signed64, AudioSample::Float64}) {
		audio.m_format = format;
		audio.m_planes.clear();
		Safe::Binary bytes;
		bytes.resize(ByteSize{4 * 8});
		for (std::size_t i = 0; i < 4; ++i) {
			if (format == AudioSample::Signed64) {
				const std::int64_t value = i % 2 ? 4611686018427387904LL : -4611686018427387904LL;
				std::memcpy(bytes.data() + i * 8, &value, sizeof(value));
			}
			else {
				const double value = i % 2 ? 0.5 : -0.5;
				std::memcpy(bytes.data() + i * 8, &value, sizeof(value));
			}
		}
		audio.m_planes.push_back(std::move(bytes));
		const auto packed = PreviewAdapter::Pack(audio, mapping);
		TEST_REQUIRE(packed.size() == 4 * sizeof(float));
		for (std::size_t i = 0; i < 4; ++i) {
			float value;
			std::memcpy(&value, packed.data() + i * sizeof(float), sizeof(value));
			TEST_REQUIRE(value == (i % 2 ? -0.5 : 0.5));
		}
	}
	audio.m_planes.clear();
	bool rejected = false;
	try { PreviewAdapter::Pack(audio, mapping); }
	catch (const StormByte::Exception&) { rejected = true; }
	TEST_REQUIRE(rejected);
	return 0;
}

/**
 * @brief Preserve exact 5.1 back versus side layouts and every PCM channel.
 * @return Test status.
 */
int test_audio_native_51() {
	TEST_REQUIRE(CheckNativePcm({"FL", "FR", "FC", "LFE", "BL", "BR"}, 48000, 0x3f) == 0);
	TEST_REQUIRE(CheckNativePcm({"FL", "FR", "FC", "LFE", "SL", "SR"}, 48000, 0x60f) == 0);
	return 0;
}

/**
 * @brief Preserve mono 44100 Hz signed PCM bytes without resampling.
 * @return Test status.
 */
int test_audio_native_mono() {
	return CheckNativePcm({"FC"}, 44100, 4);
}

/**
 * @brief Retain all six decoded Opus float planes in their exact 5.1 order.
 * @return Test status.
 */
int test_audio_native_opus() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("audio/noise_51.opus")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return static_cast<bool>(display->m_first_audio);
	}));
	TEST_REQUIRE(player.Pause());
	TEST_REQUIRE(player.Close());
	const auto audio = display->m_first_audio;
	TEST_REQUIRE(audio->m_rate == 48000);
	TEST_REQUIRE(audio->m_format == AudioSample::Float32);
	TEST_REQUIRE(audio->m_planar);
	TEST_REQUIRE(audio->m_layout == AudioLayout::Speakers);
	TEST_REQUIRE(audio->m_channels.size() == 6);
	TEST_REQUIRE(audio->m_planes.size() == 6);
	const std::array order{"FL", "FR", "FC", "LFE", "BL", "BR"};
	for (std::size_t channel = 0; channel < order.size(); ++channel) {
		TEST_REQUIRE(std::string_view(audio->m_channels[channel]) == order[channel]);
		TEST_REQUIRE(audio->m_planes[channel].size() == audio->m_frames * sizeof(float));
		bool nonzero = false;
		bool finite = true;
		for (std::size_t frame = 0; frame < audio->m_frames; ++frame) {
			float sample;
			std::memcpy(&sample, audio->m_planes[channel].data() + frame * sizeof(float), sizeof(sample));
			finite = finite && std::isfinite(sample);
			nonzero = nonzero || sample != 0;
		}
		TEST_REQUIRE(finite);
		TEST_REQUIRE(nonzero);
	}
	return 0;
}

/**
 * @brief Retain stereo signed PCM and validate the existing WAV fixture.
 * @return Test status.
 */
int test_audio_native_stereo() {
	TEST_REQUIRE(CheckNativePcm({"FL", "FR"}, 48000, 3) == 0);
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("audio/noise_stereo.wav")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return static_cast<bool>(display->m_first_audio);
	}));
	TEST_REQUIRE(player.Pause());
	TEST_REQUIRE(player.Close());
	const auto audio = display->m_first_audio;
	TEST_REQUIRE(audio->m_rate == 48000);
	TEST_REQUIRE(audio->m_format == AudioSample::Signed16);
	TEST_REQUIRE(!audio->m_planar);
	TEST_REQUIRE(audio->m_channels.size() == 2);
	TEST_REQUIRE(audio->m_layout == AudioLayout::Unspecified);
	TEST_REQUIRE(std::string_view(audio->m_channels[0]) == "NONE");
	TEST_REQUIRE(std::string_view(audio->m_channels[1]) == "NONE");
	TEST_REQUIRE(audio->m_planes[0].size() == audio->m_frames * 4);
	return 0;
}

/**
 * @brief Decode actual audio with bounded pacing, pause, resume and EOF drain.
 * @return Test status.
 */
int test_audio_pacing_pause_drain() {
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_hold_drain = true;
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("audio/noise_stereo.wav")));
	TEST_REQUIRE(!display->m_active_video);
	const auto start = TestClock::now();
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > 0.20; }));
	TEST_REQUIRE(player.Pause());
	const double paused = player.Position();
	unsigned audios;
	{
		std::lock_guard lock(display->m_mutex);
		audios = display->m_audios;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	TEST_REQUIRE(player.Position() == paused);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_audios == audios);
	}
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() >= 1.99 || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Playing);
	TEST_REQUIRE(std::chrono::duration<double>(TestClock::now() - start).count() > 1.8);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_audios > 20);
		TEST_REQUIRE(display->m_max_audio_ahead < 0.55);
		display->m_hold_drain = false;
	}
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended; }));
	TEST_REQUIRE(player.Stop());
	TEST_REQUIRE(player.Position() == 0);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > 0.05; }));
	TEST_REQUIRE(player.Close());
	return 0;
}

/**
 * @brief Close while audio playback workers enter their queue waits.
 * @return Test status.
 */
int test_close_wakes_idle_workers() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	for (unsigned iteration = 0; iteration < 32; ++iteration) {
		TEST_REQUIRE(player.Open(Fixture("audio/noise_stereo.wav")));
		TEST_REQUIRE(player.Play());
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		TEST_REQUIRE(player.Close());
		TEST_REQUIRE(player.State() == PlaybackState::Closed);
	}
	return 0;
}

/**
 * @brief Seek audio-only input while paused and resume at the requested sample.
 * @return Test status.
 */
int test_audio_paused_seek() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("audio/noise_stereo.wav")));
	TEST_REQUIRE(player.Seek(1.0));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	TEST_REQUIRE(player.Position() == 1.0);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_audios == 0);
		TEST_REQUIRE(!display->m_active_video);
	}
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended; }));
	TEST_REQUIRE(player.Position() >= 1.99);
	TEST_REQUIRE(player.Stop());
	TEST_REQUIRE(player.Close());
	return 0;
}

/**
 * @brief Verify device-master A/V sync, paused seek, track disable and epoch rejection.
 * @return Test status.
 */
int test_av_seek_switch_epochs() {
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_speed = 0.8;
	SimplePlayer player(display);
	player.Acceleration(StormByte::Multimedia::DecodeAcceleration::Software);
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	const int video = player.SelectedTrack(MediaType::Video);
	TEST_REQUIRE(video >= 0);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		if (player.State() == PlaybackState::Error)
			return true;
		if (player.Position() <= 0.30)
			return false;
		std::lock_guard lock(display->m_mutex);
		return display->m_videos > 2;
	}));
	TEST_REQUIRE(player.State() != PlaybackState::Error);
	TEST_REQUIRE(player.Pause());
	unsigned before;
	std::uint64_t epoch;
	{
		std::lock_guard lock(display->m_mutex);
		before = display->m_videos;
		epoch = display->m_epoch;
		TEST_REQUIRE(before > 2);
		TEST_REQUIRE(display->m_max_video_lead < 0.08);
		TEST_REQUIRE(display->m_max_audio_gap < 0.00001);
	}
	TEST_REQUIRE(player.Seek(1.0));
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return display->m_videos > before;
	}));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_epoch > epoch);
		TEST_REQUIRE(display->m_image->m_position >= 1.0);
		TEST_REQUIRE(display->m_image->m_position < 1.10);
	}
	TEST_REQUIRE(display->DeliverDeferred() > 0);
	TEST_REQUIRE(player.SelectTrack(MediaType::Video, -1));
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(!display->m_active_video);
		TEST_REQUIRE(!display->m_image);
	}
	TEST_REQUIRE(display->DeliverDeferred() == 0);
	TEST_REQUIRE(player.SelectTrack(MediaType::Video, video));
	TEST_REQUIRE(player.SelectTrack(MediaType::Audio, -1));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended; }));
	TEST_REQUIRE(player.Stop());
	TEST_REQUIRE(display->DeliverDeferred() > 0);
	TEST_REQUIRE(!display->m_image);
	return 0;
}

/**
 * @brief Cancel and reconstruct queued HDR conversion without stale delivery or blocked joins.
 * @return Test status.
 */
int test_video_conversion_lifecycle() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	player.Acceleration(DecodeAcceleration::Software);
	TEST_REQUIRE(player.Open(Fixture("video/bluray_like_hdr10.mkv")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(player.Pause());
	TEST_REQUIRE(player.Seek(.25));
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return display->m_image && display->m_image->m_epoch == display->m_epoch;
	}));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	TEST_REQUIRE(player.Position() == .25);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(player.Stop());
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(player.Close());
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(!display->m_image);
		TEST_REQUIRE(display->m_paused);
	}
	return 0;
}

/**
 * @brief Verify SDR preview tone-maps a short PQ/BT.2020 clip.
 * @return Test status.
 */
int test_hdr_preview_tone_mapping() {
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_speed = 20;
	SimplePlayer player(display);
	player.Acceleration(StormByte::Multimedia::DecodeAcceleration::Software);
	TEST_REQUIRE(player.Open(Fixture("video/bluray_like_hdr10.mkv")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error;
	}));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	std::lock_guard lock(display->m_mutex);
	TEST_REQUIRE(display->m_videos > 0);
	TEST_REQUIRE(display->m_image);
	TEST_REQUIRE(display->m_image->m_pixels.size()
		== static_cast<std::size_t>(display->m_image->m_width) * display->m_image->m_height * 4);
	const auto [minimum, maximum] = std::minmax_element(
		display->m_image->m_pixels.begin(), display->m_image->m_pixels.end());
	TEST_REQUIRE(minimum != display->m_image->m_pixels.end());
	TEST_REQUIRE(*minimum < *maximum);
	TEST_REQUIRE(*maximum > std::byte{0});
	return 0;
}

/**
 * @brief Preserve PQ signal data and metadata for an HDR-capable sink.
 * @return Test status.
 */
int test_hdr_preview_preserves_signal() {
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_speed = 20;
	SimplePlayer player(display);
	player.Acceleration(StormByte::Multimedia::DecodeAcceleration::Software);
	TEST_REQUIRE(player.OutputColor(PreviewColorMode::HDR));
	TEST_REQUIRE(player.Open(Fixture("video/bluray_like_hdr10.mkv")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error;
	}));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	std::lock_guard lock(display->m_mutex);
	TEST_REQUIRE(display->m_videos > 0);
	TEST_REQUIRE(display->m_hdr_image);
	TEST_REQUIRE(!display->m_image);
	TEST_REQUIRE(display->m_hdr_image->m_pixels.size()
		== static_cast<std::size_t>(display->m_hdr_image->m_width)
			* display->m_hdr_image->m_height * 8);
	TEST_REQUIRE(display->m_hdr_image->m_transfer == StormByte::Multimedia::Property::Transfer::SMPTE2084);
	TEST_REQUIRE(display->m_hdr_image->m_primaries == StormByte::Multimedia::Property::Primaries::BT2020);
	TEST_REQUIRE(display->m_hdr_image->m_has_mastering_display);
	TEST_REQUIRE(display->m_hdr_image->m_max_luminance_nits > 0);
	return 0;
}

/**
 * @brief Keep video and the reader cursor intact across live audio changes.
 * @return Test status.
 */
int test_live_audio_selection() {
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_live_audio = true;
	display->m_capture_deferred = false;
	auto stats = Safe::MakeShared<ReaderStats>();
	// Live switches must remain away from EOF even with instrumented decoding.
	StormByte::Multimedia::PreviewBuffering buffering;
	buffering.Preload = std::chrono::milliseconds(100);
	buffering.ReadAhead = std::chrono::milliseconds(250);
	buffering.AudioAhead = std::chrono::milliseconds(100);
	SimplePlayer player(display, {}, buffering);
	TEST_REQUIRE(player.Open(Safe::MakeUnique<TokenReader>(Fixture("video/live_audio_switch_long.mkv"), stats)));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, -1));
	const int audio = player.SelectedTrack(MediaType::Audio);
	TEST_REQUIRE(audio >= 0);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > .15; }));
	const unsigned seeks = stats->m_seeks;
	std::uint64_t epoch = 0;
	unsigned invalidations = 0;
	{
		std::lock_guard lock(display->m_mutex);
		epoch = display->m_epoch;
		invalidations = display->m_invalidations;
	}
	for (const int selected : {-1, audio, -1, audio}) {
		const double position = player.Position();
		unsigned videos = 0, resets = 0;
		{
			std::lock_guard lock(display->m_mutex);
			videos = display->m_videos;
			resets = display->m_audio_invalidations;
		}
		TEST_REQUIRE(player.SelectTrack(MediaType::Audio, selected));
		TEST_REQUIRE(player.SelectedTrack(MediaType::Audio) == selected);
		TEST_REQUIRE(stats->m_seeks == seeks);
		TEST_REQUIRE(player.State() == PlaybackState::Playing);
		TEST_REQUIRE(Wait([&] {
			std::lock_guard lock(display->m_mutex);
			return display->m_videos >= videos + 2;
		}));
		TEST_REQUIRE(player.Position() >= position);
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_epoch == epoch);
		TEST_REQUIRE(display->m_invalidations == invalidations);
		TEST_REQUIRE(display->m_audio_invalidations == resets + 1);
		TEST_REQUIRE(display->m_image);
	}
	for (int i = 0; i < 10; ++i) {
		TEST_REQUIRE(player.SelectTrack(MediaType::Audio, -1));
		TEST_REQUIRE(player.SelectTrack(MediaType::Audio, audio));
	}
	TEST_REQUIRE(stats->m_seeks == seeks);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_epoch == epoch);
		TEST_REQUIRE(display->m_invalidations == invalidations);
		for (std::size_t i = 1; i < display->m_video_times.size(); ++i)
			TEST_REQUIRE(display->m_video_times[i] - display->m_video_times[i - 1] < std::chrono::milliseconds(500));
	}
	TEST_REQUIRE(player.Pause());
	const double paused = player.Position();
	TEST_REQUIRE(player.SelectTrack(MediaType::Audio, -1));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	TEST_REQUIRE(player.Position() == paused);
	TEST_REQUIRE(player.Seek(player.Duration() - 1.5));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	TEST_REQUIRE(player.SelectTrack(MediaType::Audio, audio));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	TEST_REQUIRE(player.Seek(player.Duration() - .1));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	TEST_REQUIRE(player.Close());
	return 0;
}

/**
 * @brief Cycle all audio tracks while checking uninterrupted video timestamps.
 * @return Test status.
 */
int test_live_audio_tracks() {
	#ifdef _WIN32
	char* environment = nullptr;
	std::size_t environment_size = 0;
	const auto result = _dupenv_s(&environment, &environment_size, "STORMBYTE_MM_LIVE_INPUT");
	const std::unique_ptr<char, decltype(&std::free)> owned_input(environment, &std::free);
	TEST_REQUIRE(result == 0);
	const char* input = owned_input.get();
	#else
	const char* input = std::getenv("STORMBYTE_MM_LIVE_INPUT");
	#endif
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_live_audio = true;
	display->m_capture_deferred = false;
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(input ? std::string(input) : Fixture("video/live_audio_switch_long.mkv")));
	std::vector<int> tracks{-1};
	for (const auto& track : player.Tracks())
		if (track.m_type == MediaType::Audio)
			tracks.push_back(track.m_index);
	TEST_REQUIRE(tracks.size() >= 2);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > .15; }));
	for (int cycle = 0; cycle < 3; ++cycle) {
		for (int track : tracks) {
			const double previous = player.Position();
			unsigned videos, resets;
			std::uint64_t epoch;
			{
				std::lock_guard lock(display->m_mutex);
				videos = display->m_videos;
				resets = display->m_audio_invalidations;
				epoch = display->m_epoch;
			}
			const auto started = TestClock::now();
			TEST_REQUIRE(player.SelectTrack(MediaType::Audio, track));
			TEST_REQUIRE(player.SelectTrack(MediaType::Audio, track));
			TEST_REQUIRE(Wait([&] {
				{
					std::lock_guard lock(display->m_mutex);
					if (display->m_videos > videos && (track < 0 || display->m_first_audio))
						return true;
				}
				return player.State() == PlaybackState::Error;
			}));
			TEST_REQUIRE(player.State() == PlaybackState::Playing);
			TEST_REQUIRE(player.Position() >= previous);
			if (input)
				TEST_REQUIRE(Wait([&] { return player.Position() > previous + .25 || player.State() == PlaybackState::Error; }));
			TEST_REQUIRE(player.State() != PlaybackState::Error);
			{
				std::lock_guard lock(display->m_mutex);
				TEST_REQUIRE(display->m_epoch == epoch);
				TEST_REQUIRE(display->m_audio_invalidations == resets + 1);
				TEST_REQUIRE(display->m_image);
			}
			std::cout << "live audio track=" << track << " position=" << player.Position()
				<< " switch+delivery_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(
					TestClock::now() - started).count() << '\n';
		}
	}
	TEST_REQUIRE(player.Close());
	for (std::size_t i = 1; i < display->m_video_positions.size(); ++i) {
		TEST_REQUIRE(display->m_video_positions[i] > display->m_video_positions[i - 1]);
		TEST_REQUIRE(display->m_video_times[i] - display->m_video_times[i - 1] < std::chrono::milliseconds(500));
	}
	return 0;
}

/**
 * @brief Play an input whose audio or video packets are stored three seconds late.
 * @param name Fixture path.
 * @param acceleration Decoder policy; software isolates scheduler timing from device latency.
 * @return Test status.
 */
int SkewedInterleavePlayback(const char* name, DecodeAcceleration acceleration) {
	const bool check_timing = acceleration == DecodeAcceleration::Software;
	// Media time must pass in real time, so these waits scale with the skew.
	const double deadline = std::max(DefaultDeadline, 15.0);
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_live_audio = true;
	display->m_capture_deferred = false;
	display->m_capture_timing = true;
	// Auto exercises a slow device without turning its latency into a
	// scheduler guarantee. Software retains the strict timing assertions.
	if (!check_timing)
		display->m_video_delay = std::chrono::milliseconds(600);
	SimplePlayer player(display);
	player.Acceleration(acceleration);
	const auto test_started = TestClock::now();
	const char* phase = "open";
	const auto enter_phase = [&](const char* next) {
		phase = next;
		std::cout << "[skew phase] fixture=" << name
			<< " policy=" << (check_timing ? "Software" : "Auto") << " phase=" << next
			<< " elapsed=" << std::chrono::duration<double>(TestClock::now() - test_started).count()
			<< " position=" << player.Position()
			<< " state_code=" << static_cast<int>(player.State())
			<< " video_track=" << player.SelectedTrack(MediaType::Video)
			<< " audio_track=" << player.SelectedTrack(MediaType::Audio) << std::endl;
	};
	const int result = [&]() -> int {
		enter_phase("open");
		TEST_REQUIRE(player.Open(Fixture(name)));
		const int audio = player.SelectedTrack(MediaType::Audio);
		TEST_REQUIRE(audio >= 0 && player.SelectedTrack(MediaType::Video) >= 0);
		enter_phase("play");
		TEST_REQUIRE(player.Play());
		enter_phase("initial playback");
		TEST_REQUIRE(Wait([&] { return player.Position() > 6 || player.State() != PlaybackState::Playing; }, deadline));
		TEST_REQUIRE(player.State() == PlaybackState::Playing);
		std::uint64_t epoch;
		unsigned videos, resets;
		{
			std::lock_guard lock(display->m_mutex);
			std::cout << name << " playback underrun=" << display->m_max_audio_underrun
				<< " video_lead=" << display->m_max_video_lead
				<< " video_late=" << display->m_max_video_late << '\n';
			if (check_timing) {
				TEST_REQUIRE(display->m_max_audio_underrun < 0.05);
				TEST_REQUIRE(display->m_max_video_lead < 0.10);
#ifdef STORMBYTE_MM_TEST_VIDEO_LATENESS_SEC
				// Instrumentation on shared runners can deschedule presentation.
				TEST_REQUIRE(display->m_max_video_late < STORMBYTE_MM_TEST_VIDEO_LATENESS_SEC);
#else
				TEST_REQUIRE(display->m_max_video_late < 0.10);
#endif
			}
			epoch = display->m_epoch;
			resets = display->m_audio_invalidations;
		}
		enter_phase("disable audio");
		TEST_REQUIRE(player.SelectTrack(MediaType::Audio, -1));
		{
			std::lock_guard lock(display->m_mutex);
			videos = display->m_videos;
		}
		TEST_REQUIRE(Wait([&] {
			std::lock_guard lock(display->m_mutex);
			return display->m_videos > videos + 12;
		}));
		const double switched = player.Position();
		const auto started = TestClock::now();
		enter_phase("re-enable audio");
		TEST_REQUIRE(player.SelectTrack(MediaType::Audio, audio));
		TEST_REQUIRE(Wait([&] {
			{
				std::lock_guard lock(display->m_mutex);
				if (display->m_first_audio)
					return true;
			}
			return player.State() != PlaybackState::Playing;
		}, deadline));
		TEST_REQUIRE(player.State() == PlaybackState::Playing);
		{
			std::lock_guard lock(display->m_mutex);
			const double silence = display->m_first_audio->m_position - switched;
			std::cout << name << " re-enable silence=" << silence << " wall_ms="
				<< std::chrono::duration_cast<std::chrono::milliseconds>(TestClock::now() - started).count() << '\n';
			// The new decoder starts at the retained demux cursor: at most the
			// read-ahead horizon plus the fixture's three-second storage skew.
			TEST_REQUIRE(silence < 2 + 3 + 0.5);
			TEST_REQUIRE(display->m_epoch == epoch);
			TEST_REQUIRE(display->m_audio_invalidations == resets + 2);
		}
		enter_phase("close");
		TEST_REQUIRE(player.Close());
		enter_phase("video continuity");
		TEST_REQUIRE(display->m_video_positions.size() > 12);
		double max_gap = 0;
		for (std::size_t i = 1; i < display->m_video_positions.size(); ++i) {
			TEST_REQUIRE(display->m_video_positions[i] > display->m_video_positions[i - 1]);
			TEST_REQUIRE(display->m_video_positions[i] - display->m_video_positions[i - 1] < 0.05);
			max_gap = std::max(max_gap, std::chrono::duration<double>(
				display->m_video_times[i] - display->m_video_times[i - 1]).count());
			if (check_timing)
				TEST_REQUIRE(display->m_video_times[i] - display->m_video_times[i - 1] < std::chrono::milliseconds(500));
		}
		std::cout << name << " max_delivery_gap=" << max_gap << '\n';
		if (!check_timing)
			TEST_REQUIRE(max_gap >= 0.6);
		return 0;
	}();
	const auto state = player.State();
	const double position = player.Position();
	std::lock_guard lock(display->m_mutex);
	std::cout << "[skew diagnostics] fixture=" << name
		<< " policy=" << (check_timing ? "Software" : "Auto")
		<< " phase=" << phase << " result=" << result
		<< " state_code=" << static_cast<int>(state) << " position=" << position
		<< " elapsed=" << std::chrono::duration<double>(TestClock::now() - test_started).count()
		<< " videos=" << display->m_videos << " audios=" << display->m_audios
		<< " epoch=" << display->m_epoch << " audio_resets=" << display->m_audio_invalidations
		<< " underrun=" << display->m_max_audio_underrun
		<< " video_lead=" << display->m_max_video_lead
		<< " video_late=" << display->m_max_video_late << '\n';
	std::cout << "[skew video] index pts wall_s gap_s audio_pts queued_audio_end audio_blocks audio_resets epoch\n";
	for (std::size_t i = 0; i < display->m_video_timing.size(); ++i) {
		const auto& sample = display->m_video_timing[i];
		std::cout << "[skew video] " << i << ' ' << display->m_video_positions[i] << ' '
			<< std::chrono::duration<double>(display->m_video_times[i] - test_started).count() << ' '
			<< (i ? std::chrono::duration<double>(display->m_video_times[i] - display->m_video_times[i - 1]).count() : 0)
			<< ' ' << sample.m_audio << ' ' << sample.m_pending_end << ' ' << sample.m_audios
			<< ' ' << sample.m_audio_invalidations << ' ' << sample.m_epoch << '\n';
	}
	return result;
}

/**
 * @brief Keep audio fed when its packets are stored three seconds after video.
 * @return Test status.
 */
int test_skewed_audio_late() {
	return SkewedInterleavePlayback("video/skew_audio_late.mkv", DecodeAcceleration::Software);
}

/**
 * @brief Keep video fed when its packets are stored three seconds after audio.
 * @return Test status.
 */
int test_skewed_video_late() {
	return SkewedInterleavePlayback("video/skew_video_late.mkv", DecodeAcceleration::Software);
}

int test_skewed_audio_late_auto() {
	return SkewedInterleavePlayback("video/skew_audio_late.mkv", DecodeAcceleration::Auto);
}

int test_skewed_video_late_auto() {
	return SkewedInterleavePlayback("video/skew_video_late.mkv", DecodeAcceleration::Auto);
}

/**
 * @brief Flush a full audio-only queue and expose sink reset failures.
 * @return Test status.
 */
int test_live_audio_backpressure_errors() {
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_live_audio = true;
	display->m_speed = 0;
	const auto path = OutputPath("live-audio-pressure.wav");
	WriteAudioFixture(path, 48000, 1, 4, 48000 * 30);
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(TestLocation(path)));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return display->m_audios > 0;
	}));
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	TEST_REQUIRE(player.SelectTrack(MediaType::Audio, -1));
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_audio_invalidations == 1);
	}
	TEST_REQUIRE(player.Close());
	display->m_speed = 1;
	display->m_throw_audio_invalidation = true;
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > .1; }));
	TEST_REQUIRE(!player.SelectTrack(MediaType::Audio, -1));
	TEST_REQUIRE(player.State() == PlaybackState::Error);
	TEST_REQUIRE(std::string_view(player.Error()).find("test audio invalidation failure") != std::string_view::npos);
	TEST_REQUIRE(player.Close());
	display->m_live_audio = false;
	display->m_throw_audio_invalidation = false;
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > .1; }));
	std::uint64_t epoch;
	unsigned resets, videos;
	{
		std::lock_guard lock(display->m_mutex);
		epoch = display->m_epoch;
		resets = display->m_audio_invalidations;
		videos = display->m_videos;
	}
	TEST_REQUIRE(player.SelectTrack(MediaType::Audio, -1));
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return display->m_videos > videos;
	}));
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_epoch > epoch);
		TEST_REQUIRE(display->m_audio_invalidations == resets);
	}
	TEST_REQUIRE(player.State() == PlaybackState::Playing);
	TEST_REQUIRE(player.Close());
	return 0;
}

/**
 * @brief Present the image covering a non-frame-aligned seek, including near EOF.
 * @return Test status.
 */
int test_av_seek_within_frame() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, -1));
	for (const double target : {0.013, 1.987}) {
		TEST_REQUIRE(player.Seek(target));
		TEST_REQUIRE(Wait([&] {
			std::lock_guard lock(display->m_mutex);
			return static_cast<bool>(display->m_image);
		}));
		TEST_REQUIRE(player.State() == PlaybackState::Paused);
		TEST_REQUIRE(player.Position() == target);
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_image->m_position <= target);
		TEST_REQUIRE(display->m_image->m_position + display->m_image->m_duration > target);
		TEST_REQUIRE(display->m_audios == 0);
	}
	TEST_REQUIRE(player.Stop());
	return 0;
}

/**
 * @brief Expose decoder and display failures instead of successful placeholders.
 * @return Test status.
 */
int test_explicit_errors() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(!player.Open(Fixture("invalid/random_garbage.mkv")));
	TEST_REQUIRE(player.State() == PlaybackState::Error);
	TEST_REQUIRE(!player.Error().empty());
	TEST_REQUIRE(player.Close());
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	display->m_throw_video = true;
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(std::string_view(player.Error()).find("test display failure") != std::string_view::npos);
	TEST_REQUIRE(player.Close());
	auto stats = Safe::MakeShared<ReaderStats>();
	{
		SimplePlayer cleanup(display);
		display->m_throw_video = false;
		TEST_REQUIRE(cleanup.Open(Safe::MakeUnique<TokenReader>(Fixture("video/anime_like.mkv"), stats)));
		display->m_throw_invalidate = true;
	}
	TEST_REQUIRE(stats->m_closes == 1);
	display->m_throw_invalidate = false;
	return 0;
}

/**
 * @brief Preserve a finite subtitle longer than ten minutes during paused reconstruction.
 * @return Test status.
 */
int test_long_subtitle_duration() {
	constexpr std::string_view text = "1\n00:00:00,000 --> 00:15:00,000\nLong finite cue\n";
	auto display = Safe::MakeShared<FakeDisplay>();
	auto stats = Safe::MakeShared<ReaderStats>();
	Safe::Binary data(reinterpret_cast<const std::byte*>(text.data()), ByteSize{text.size()});
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Safe::MakeUnique<TokenReader>(std::move(data), stats)));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, 0));
	TEST_REQUIRE(player.Seek(1.0));
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return !display->m_cues.empty();
	}));
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_cues.front()->m_end == 900.0);
	}
	TEST_REQUIRE(player.Stop());
	return 0;
}

/**
 * @brief Preview an Opus file without manufacturing video, including seek and rewind.
 * @return Test status.
 */
int test_opus_without_video() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("audio/noise_tagged.opus")));
	TEST_REQUIRE(player.SelectedTrack(MediaType::Video) == -1);
	TEST_REQUIRE(player.SelectedTrack(MediaType::Audio) >= 0);
	TEST_REQUIRE(player.Duration() > 0.1);
	const double target = player.Duration() / 2;
	TEST_REQUIRE(player.Seek(target));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(!display->m_active_video);
		TEST_REQUIRE(!display->m_image);
		TEST_REQUIRE(display->m_videos == 0);
		TEST_REQUIRE(display->m_audios > 0);
	}
	TEST_REQUIRE(player.Stop());
	auto reader = player.ReleaseReader();
	TEST_REQUIRE(reader && reader->IsOpen());
	TEST_REQUIRE(reader->Tell() == ByteSize{0});
	return 0;
}

/**
 * @brief Reconstruct packed AVI images and continuous native surround audio after byte-chunk seeks.
 * @return Test status.
 */
int test_packed_avi_seek() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("video/packed_ac3_seek.avi")));
	TEST_REQUIRE(std::abs(player.Duration() - 8) < 0.1);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > 0.20 || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() != PlaybackState::Error);
	TEST_REQUIRE(player.Pause());
	Safe::Shared<const PreviewVideo> previous;
	{
		std::lock_guard lock(display->m_mutex);
		previous = display->m_image;
		TEST_REQUIRE(previous);
	}
	for (const double target : {4.0, 7.2, 0.8}) {
		TEST_REQUIRE(player.Seek(target));
		TEST_REQUIRE(Wait([&] {
			std::lock_guard lock(display->m_mutex);
			return static_cast<bool>(display->m_image);
		}));
		TEST_REQUIRE(player.State() == PlaybackState::Paused);
		{
			std::lock_guard lock(display->m_mutex);
			const auto image = display->m_image;
			TEST_REQUIRE(image);
			TEST_REQUIRE(std::abs(image->m_position - target) < 0.081);
			if (previous)
				TEST_REQUIRE(std::memcmp(image->m_pixels.data(), previous->m_pixels.data(), image->m_pixels.size()) != 0);
			previous = image;
		}
		TEST_REQUIRE(player.Play());
		TEST_REQUIRE(Wait([&] { return player.Position() > target + 0.20 || player.State() == PlaybackState::Error; }));
		TEST_REQUIRE(player.State() != PlaybackState::Error);
		TEST_REQUIRE(player.Pause());
		{
			std::lock_guard lock(display->m_mutex);
			TEST_REQUIRE(display->m_first_audio);
			TEST_REQUIRE(display->m_first_audio->m_channels.size() == 6);
			TEST_REQUIRE(display->m_first_audio->m_planar);
			TEST_REQUIRE(display->m_first_audio->m_format == AudioSample::Float32);
			TEST_REQUIRE(display->m_first_audio->m_rate == 48000);
			constexpr std::array<std::string_view, 6> channels{"FL", "FR", "FC", "LFE", "SL", "SR"};
			for (std::size_t i = 0; i < channels.size(); ++i)
				TEST_REQUIRE(std::string_view(display->m_first_audio->m_channels[i]) == channels[i]);
			TEST_REQUIRE(display->m_max_audio_gap < 0.00001);
			TEST_REQUIRE(display->m_max_audio_ahead < 0.55);
		}
	}
	TEST_REQUIRE(player.Close());
	return 0;
}

int CheckRejectedPreviewPacket(std::string_view chunk, unsigned rejectedIndex) {
	const auto input = OutputPath("rejected-packet.avi");
	TEST_REQUIRE(WriteRejectedAviPacket(input, chunk, rejectedIndex) == 0);
	std::ostringstream diagnostics;
	auto logger = Safe::MakeShared<StormByte::Logger::Log>(
		diagnostics, StormByte::Logger::Level::Warning, "[%L]");
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display, logger);
	TEST_REQUIRE(player.Open(TestLocation(input)));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	if (!player.Error().empty())
		std::cerr << "[DETAIL] " << std::string_view(player.Error()) << std::endl;
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_videos >= 24);
		TEST_REQUIRE(display->m_audios >= 70);
	}
	TEST_REQUIRE(player.Stop());
	TEST_REQUIRE(player.Seek(0));
	TEST_REQUIRE(Wait([&] {
		if (player.State() == PlaybackState::Error)
			return true;
		std::lock_guard lock(display->m_mutex);
		return static_cast<bool>(display->m_image);
	}));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	TEST_REQUIRE(player.Error().empty());
	TEST_REQUIRE(player.Close());
	const auto output = diagnostics.str();
	TEST_REQUIRE(output.find("preview packet rejected t=" + std::string(chunk == "01wb" ? "1" : "0"))
		!= std::string::npos);
	TEST_REQUIRE(output.find("bytes=") != std::string::npos);
	TEST_REQUIRE(output.find("Invalid data found when processing input") != std::string::npos);
	TEST_REQUIRE(output.find("skipping packet and continuing decoding") != std::string::npos);
	return 0;
}

int test_recovers_rejected_initial_avi_mp3_packet() {
	return CheckRejectedPreviewPacket("01wb", 0);
}

int test_recovers_rejected_avi_mp3_packet() {
	return CheckRejectedPreviewPacket("01wb", 10);
}

int test_recovers_rejected_avi_video_packet() {
	return CheckRejectedPreviewPacket("00dc", 0);
}

/**
 * @brief Decode actual PGS bitmaps and text/styled subtitles.
 * @return Test status.
 */
int test_subtitle_decode() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, 2));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return display->m_bitmaps > 0;
	}));
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(player.State() == PlaybackState::Ended);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_fonts > 0);
		TEST_REQUIRE(display->m_cues.empty());
	}
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, 3));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, -1));
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_cues.empty());
	}
	TEST_REQUIRE(player.Close());
	TEST_REQUIRE(player.Open(Fixture("subtitles/anime_eng.srt")));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, 0));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return display->m_texts > 0;
	}));
	TEST_REQUIRE(player.Stop());
	return 0;
}

/**
 * @brief Restore a subtitle in paused audio-only preview, then resume without losing audio.
 * @return Test status.
 */
int test_subtitle_paused_seek() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	TEST_REQUIRE(player.SelectTrack(MediaType::Video, -1));
	TEST_REQUIRE(player.SelectTrack(MediaType::Subtitle, 2));
	TEST_REQUIRE(player.Seek(1.0));
	TEST_REQUIRE(Wait([&] {
		std::lock_guard lock(display->m_mutex);
		return !display->m_cues.empty();
	}));
	TEST_REQUIRE(player.State() == PlaybackState::Paused);
	TEST_REQUIRE(player.Position() == 1.0);
	{
		std::lock_guard lock(display->m_mutex);
		TEST_REQUIRE(display->m_audios == 0);
		TEST_REQUIRE(!display->m_image);
		TEST_REQUIRE(!display->m_active_video);
	}
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Ended; }));
	TEST_REQUIRE(player.Error().empty());
	TEST_REQUIRE(player.Stop());
	return 0;
}

// -------------------
// Reader
// -------------------

/**
 * @brief Retain dynamic reader ownership on fault and never reopen a released token.
 * @return Test status.
 */
int test_faulted_reader_retained() {
	auto display = Safe::MakeShared<FakeDisplay>();
	auto stats = Safe::MakeShared<ReaderStats>();
	auto reader = Safe::MakeUnique<TokenReader>(Fixture("video/anime_like.mkv"), stats);
	auto* identity = reader.get();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(std::move(reader)));
	stats->m_fail_read = true;
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.State() == PlaybackState::Error; }));
	TEST_REQUIRE(!player.ReleaseReader());
	TEST_REQUIRE(stats->m_opens == 1);
	TEST_REQUIRE(stats->m_closes == 0);
	TEST_REQUIRE(identity->IsOpen());
	TEST_REQUIRE(player.Close());
	TEST_REQUIRE(stats->m_closes == 1);
	return 0;
}

/**
 * @brief Transcode from the same still-open remote-like reader after preview ownership transfer.
 * @return Test status.
 */
int test_preview_to_transcoder() {
	auto display = Safe::MakeShared<FakeDisplay>();
	auto stats = Safe::MakeShared<ReaderStats>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Safe::MakeUnique<TokenReader>(Fixture("audio/noise_stereo.wav"), stats)));
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > 0.05; }));
	auto reader = player.ReleaseReader();
	TEST_REQUIRE(reader && reader->IsOpen());
	TEST_REQUIRE(stats->m_opens == 1 && stats->m_closes == 0);
	TEST_REQUIRE(player.Close());
	const auto output = OutputPath("pipeline/preview-transfer.mka");
	{
		auto logger = Safe::MakeShared<StormByte::Logger::ThreadedLog>(
			std::cout, StormByte::Logger::Level::Error, "[%L]");
		StormByte::Multimedia::Pipeline::Transcoder job(std::move(reader), Safe::String(output.string()),
			logger, 2000000000LL);
		job.Audio(0).Remux();
		job.Run();
		TEST_REQUIRE(Wait([&] {
			return job.Status() == StormByte::Multimedia::Pipeline::Status::Done || job.Failed();
		}));
		if (const auto error = job.Error())
			std::cerr << "[DETAIL] " << std::string_view(error.value()) << std::endl;
		TEST_REQUIRE(!job.Failed());
		TEST_REQUIRE(job.Status() == StormByte::Multimedia::Pipeline::Status::Done);
		TEST_REQUIRE(std::filesystem::file_size(output) > 0);
		TEST_REQUIRE(stats->m_opens == 1);
	}
	const auto opened = StormByte::Multimedia::File::Open(Safe::String(output.string()));
	TEST_REQUIRE(opened);
	TEST_REQUIRE(opened.value().Streams().size() == 1);
	TEST_REQUIRE(opened.value().Streams()[0].Type() == MediaType::Audio);
	const auto duration = opened.value().Duration();
	TEST_REQUIRE(duration);
	TEST_REQUIRE(duration->Nanoseconds().count() >= 1990000000LL);
	TEST_REQUIRE(duration->Nanoseconds().count() <= 2010000000LL);
	return 0;
}

/**
 * @brief Propagate origin rewind/close errors while retaining transfer ownership.
 * @return Test status.
 */
int test_rewind_close_failures() {
	auto display = Safe::MakeShared<FakeDisplay>();
	auto stats = Safe::MakeShared<ReaderStats>();
	auto reader = Safe::MakeUnique<TokenReader>(Fixture("audio/noise_stereo.wav"), stats);
	auto* identity = reader.get();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(std::move(reader)));
	identity->MaxMemory(ByteSize{0});
	stats->m_fail_seek = true;
	TEST_REQUIRE(player.Stop());
	TEST_REQUIRE(!player.Play());
	TEST_REQUIRE(player.State() == PlaybackState::Error);
	TEST_REQUIRE(!player.Stop());
	TEST_REQUIRE(!player.ReleaseReader());
	TEST_REQUIRE(stats->m_opens == 1 && stats->m_closes == 0);
	TEST_REQUIRE(identity->IsOpen());
	stats->m_fail_close = true;
	TEST_REQUIRE(!player.Close());
	TEST_REQUIRE(player.State() == PlaybackState::Error);
	TEST_REQUIRE(stats->m_closes == 1);
	return 0;
}

/**
 * @brief Stop rewinds the actual reader and transfer keeps the remote-like token alive.
 * @return Test status.
 */
int test_stop_transfer_destructor() {
	auto display = Safe::MakeShared<FakeDisplay>();
	auto stats = Safe::MakeShared<ReaderStats>();
	auto reader = Safe::MakeUnique<TokenReader>(Fixture("audio/noise_stereo.wav"), stats);
	auto* identity = reader.get();
	{
		SimplePlayer player(display);
		TEST_REQUIRE(player.Open(std::move(reader)));
		TEST_REQUIRE(player.Play());
		TEST_REQUIRE(Wait([&] { return player.Position() > 0.1; }));
		TEST_REQUIRE(player.Stop());
		TEST_REQUIRE(identity->Tell() == ByteSize{0});
		TEST_REQUIRE(identity->IsOpen());
		TEST_REQUIRE(stats->m_closes == 0 && stats->m_opens == 1);
		reader = {};
		auto transferred = player.ReleaseReader();
		TEST_REQUIRE(transferred.get() == identity);
		TEST_REQUIRE(dynamic_cast<TokenReader*>(transferred.get()));
		TEST_REQUIRE(transferred->IsOpen());
		TEST_REQUIRE(transferred->Tell() == ByteSize{0});
		TEST_REQUIRE(stats->m_closes == 0);
		TEST_REQUIRE(player.State() == PlaybackState::Closed);
		TEST_REQUIRE(transferred->Close().status == IO::Status::Ok);
	}
	TEST_REQUIRE(stats->m_closes == 1);
	auto second = Safe::MakeShared<ReaderStats>();
	{
		SimplePlayer player(display);
		TEST_REQUIRE(player.Open(Safe::MakeUnique<TokenReader>(Fixture("audio/noise_stereo.wav"), second)));
		TEST_REQUIRE(player.Play());
		TEST_REQUIRE(Wait([&] { return player.Position() > 0.05; }));
	}
	TEST_REQUIRE(second->m_closes == 1 && second->m_opens == 1);
	TEST_REQUIRE(display->m_paused);
	return 0;
}

/**
 * @brief Seek by absolute seconds when a live-style video has no duration metadata or estimate.
 * @return Test status.
 */
int test_unknown_duration_seek() {
	auto display = Safe::MakeShared<FakeDisplay>();
	SimplePlayer player(display);
	TEST_REQUIRE(player.Open(Fixture("video/unknown_duration.webm")));
	TEST_REQUIRE(player.Duration() == 0);
	for (const double target : {30.0, 120.0, 10.0}) {
		TEST_REQUIRE(player.Seek(target));
		TEST_REQUIRE(Wait([&] {
			std::lock_guard lock(display->m_mutex);
			return static_cast<bool>(display->m_image);
		}));
		TEST_REQUIRE(player.State() == PlaybackState::Paused);
		TEST_REQUIRE(player.Position() == target);
		{
			std::lock_guard lock(display->m_mutex);
			TEST_REQUIRE(std::abs(display->m_image->m_position - target) < 0.21);
		}
		TEST_REQUIRE(player.Play());
		TEST_REQUIRE(Wait([&] { return player.Position() > target + 0.20 || player.State() == PlaybackState::Error; }));
		TEST_REQUIRE(player.State() != PlaybackState::Error);
		TEST_REQUIRE(player.Pause());
		TEST_REQUIRE(player.Duration() == 0);
	}
	TEST_REQUIRE(player.Close());
	return 0;
}

/**
 * @brief Run focused deterministic playback cases.
 * @param argc Argument count.
 * @param argv Selected test name.
 * @return Test status.
 */
int MeasurePreview(int argc, char** argv) {
	if (argc != 6) {
		std::fprintf(stderr, "usage: --measure FILE WALL_SECONDS SEEK_SECONDS auto|software\n");
		return 2;
	}
	const double seconds = std::stod(argv[3]);
	const double seek = std::stod(argv[4]);
	const std::string_view policy(argv[5]);
	if (!std::isfinite(seconds) || seconds <= 0 || !std::isfinite(seek) || seek < 0
		|| (policy != "auto" && policy != "software"))
		throw StormByte::Exception("invalid measurement arguments");
	auto display = Safe::MakeShared<FakeDisplay>();
	display->m_capture_deferred = false;
	SimplePlayer player(display);
	player.Acceleration(policy == "software" ? DecodeAcceleration::Software : DecodeAcceleration::Auto);
	const auto start = TestClock::now();
	if (!player.Open(argv[2]) || !player.Play())
		throw StormByte::Exception(player.Error());
	const bool video = player.SelectedTrack(MediaType::Video) >= 0;
	const auto delivered = [&] {
		std::lock_guard lock(display->m_mutex);
		return video ? display->m_videos : display->m_audios;
	};
	if (!Wait([&] { return delivered() > 0 || player.State() == PlaybackState::Error; }, 60)
		|| player.State() == PlaybackState::Error)
		throw StormByte::Exception("measurement startup failed: " + player.Error());
	const double startup = std::chrono::duration<double, std::milli>(TestClock::now() - start).count();
	const auto first = TestClock::now();
	if (!Wait([&] {
		return std::chrono::duration<double>(TestClock::now() - first).count() >= seconds
			|| player.State() == PlaybackState::Ended || player.State() == PlaybackState::Error;
	}, seconds + 60))
		throw StormByte::Exception("measurement playback deadline exceeded");
	if (player.State() == PlaybackState::Error)
		throw StormByte::Exception(player.Error());
	const auto before_seek = TestClock::now();
	const unsigned before = delivered();
	if (!player.Seek(seek))
		throw StormByte::Exception(player.Error());
	if (player.State() != PlaybackState::Playing && !player.Play())
		throw StormByte::Exception(player.Error());
	if (!Wait([&] { return delivered() > before || player.State() == PlaybackState::Error; }, 60)
		|| player.State() == PlaybackState::Error)
		throw StormByte::Exception("measurement seek failed");
	const double seek_ms = std::chrono::duration<double, std::milli>(TestClock::now() - before_seek).count();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	if (player.State() == PlaybackState::Playing && !player.Pause())
		throw StormByte::Exception(player.Error());
	const auto result = player.Diagnostics();
	if (!player.Close())
		throw StormByte::Exception(player.Error());
	double rss_mib = -1;
#if defined(__linux__) || defined(__APPLE__)
	rusage usage{};
	if (getrusage(RUSAGE_SELF, &usage) != 0)
		throw StormByte::Exception("getrusage failed");
#if defined(__APPLE__)
	rss_mib = usage.ru_maxrss / (1024.0 * 1024.0);
#else
	rss_mib = usage.ru_maxrss / 1024.0;
#endif
#endif
	std::printf("startup_ms,seek_ms,rss_mib,queue_mib,decoder,backend,device,shown,skipped,underruns,clock_samples,converted,conversion_fps,download_ms,linear_ms,tonemap_ms,color_ms,packing_ms\n");
	const double per_frame = result.m_converted_video ? 1000.0 / result.m_converted_video : 0;
	std::printf("%.3f,%.3f,%.3f,%.3f,%s,%d,%s,%llu,%llu,%llu,%llu,%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
		startup, seek_ms, rss_mib, result.m_peak_queue_bytes / (1024.0 * 1024.0),
		result.m_decoder.c_str(), static_cast<int>(result.m_backend), result.m_device.c_str(),
		static_cast<unsigned long long>(result.m_shown_video), static_cast<unsigned long long>(result.m_skipped_video),
		static_cast<unsigned long long>(result.m_audio_underruns), static_cast<unsigned long long>(result.m_audio_clock_samples),
		static_cast<unsigned long long>(result.m_converted_video), result.m_conversion_fps,
		result.m_download_seconds * per_frame, result.m_linearization_seconds * per_frame,
		result.m_tone_mapping_seconds * per_frame, result.m_color_conversion_seconds * per_frame,
		result.m_rgba_packing_seconds * per_frame);
	return 0;
}

int LoadPreviewCPU(int argc, char** argv) {
	if (argc != 4)
		throw StormByte::Exception("usage: --cpu-load SECONDS WORKERS");
	const double seconds = std::stod(argv[2]);
	const int workers = std::stoi(argv[3]);
	if (!std::isfinite(seconds) || seconds <= 0 || workers <= 0)
		throw StormByte::Exception("invalid CPU load arguments");
	const auto end = TestClock::now() + std::chrono::duration<double>(seconds);
	std::vector<std::jthread> load;
	for (int i = 0; i < workers; ++i)
		load.emplace_back([end] {
			while (TestClock::now() < end)
				std::atomic_signal_fence(std::memory_order_seq_cst);
		});
	return 0;
}

int main(int argc, char** argv) {
	if (argc > 1 && (std::string_view(argv[1]) == "--measure" || std::string_view(argv[1]) == "--cpu-load")) {
		try {
			return std::string_view(argv[1]) == "--measure" ? MeasurePreview(argc, argv) : LoadPreviewCPU(argc, argv);
		}
		catch (const std::exception& error) {
			std::fprintf(stderr, "%s\n", error.what());
			return 1;
		}
	}
	// -------------------
	// Construct
	// -------------------
	static constexpr std::array tests{
		TestEntry{"test_empty_display", test_empty_display},
		TestEntry{"test_buffering_policy", test_buffering_policy},
		TestEntry{"test_preview_diagnostics", test_preview_diagnostics},
		// -------------------
		// Playback
		// -------------------
		TestEntry{"test_audio_adapter_mapping", test_audio_adapter_mapping},
		TestEntry{"test_audio_adapter_samples", test_audio_adapter_samples},
		TestEntry{"test_audio_native_51", test_audio_native_51},
		TestEntry{"test_audio_native_mono", test_audio_native_mono},
		TestEntry{"test_audio_native_opus", test_audio_native_opus},
		TestEntry{"test_audio_native_stereo", test_audio_native_stereo},
		TestEntry{"test_audio_pacing_pause_drain", test_audio_pacing_pause_drain},
		TestEntry{"test_close_wakes_idle_workers", test_close_wakes_idle_workers},
		TestEntry{"test_audio_paused_seek", test_audio_paused_seek},
		TestEntry{"test_av_seek_switch_epochs", test_av_seek_switch_epochs},
		TestEntry{"test_hdr_preview_tone_mapping", test_hdr_preview_tone_mapping},
		TestEntry{"test_video_conversion_lifecycle", test_video_conversion_lifecycle},
		TestEntry{"test_hdr_preview_preserves_signal", test_hdr_preview_preserves_signal},
		TestEntry{"test_live_audio_selection", test_live_audio_selection},
		TestEntry{"test_live_audio_tracks", test_live_audio_tracks},
		TestEntry{"test_skewed_audio_late", test_skewed_audio_late},
		TestEntry{"test_skewed_video_late", test_skewed_video_late},
		TestEntry{"test_skewed_audio_late_auto", test_skewed_audio_late_auto},
		TestEntry{"test_skewed_video_late_auto", test_skewed_video_late_auto},
		TestEntry{"test_live_audio_backpressure_errors", test_live_audio_backpressure_errors},
		TestEntry{"test_av_seek_within_frame", test_av_seek_within_frame},
		TestEntry{"test_explicit_errors", test_explicit_errors},
		TestEntry{"test_long_subtitle_duration", test_long_subtitle_duration},
		TestEntry{"test_opus_without_video", test_opus_without_video},
		TestEntry{"test_packed_avi_seek", test_packed_avi_seek},
		TestEntry{"test_recovers_rejected_initial_avi_mp3_packet", test_recovers_rejected_initial_avi_mp3_packet},
		TestEntry{"test_recovers_rejected_avi_mp3_packet", test_recovers_rejected_avi_mp3_packet},
		TestEntry{"test_recovers_rejected_avi_video_packet", test_recovers_rejected_avi_video_packet},
		TestEntry{"test_subtitle_decode", test_subtitle_decode},
		TestEntry{"test_subtitle_paused_seek", test_subtitle_paused_seek},
		TestEntry{"test_unknown_duration_seek", test_unknown_duration_seek},
		// -------------------
		// Reader
		// -------------------
		TestEntry{"test_faulted_reader_retained", test_faulted_reader_retained},
		TestEntry{"test_preview_to_transcoder", test_preview_to_transcoder},
		TestEntry{"test_rewind_close_failures", test_rewind_close_failures},
		TestEntry{"test_stop_transfer_destructor", test_stop_transfer_destructor},
	};
	return RunSelectedTest(argc, argv, tests);
}
