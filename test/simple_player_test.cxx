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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <thread>
#include <vector>

namespace Safe = StormByte::Safe;
namespace IO = StormByte::Buffer::IO;
using StormByte::ByteSize;
using StormByte::Multimedia::AudioPresentation;
using StormByte::Multimedia::AudioLayout;
using StormByte::Multimedia::AudioSample;
using StormByte::Multimedia::Display;
using StormByte::Multimedia::PlaybackState;
using StormByte::Multimedia::PreviewAudio;
using StormByte::Multimedia::PreviewSubtitle;
using StormByte::Multimedia::PreviewVideo;
using StormByte::Multimedia::SimplePlayer;
using MediaType = StormByte::Multimedia::Type;
using TestClock = std::chrono::steady_clock;

/**
 * @class FakeDisplay
 * @brief Headless clocked sink with deliberately deferred GUI delivery.
 */
class FakeDisplay final: public Display {
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
		 * @brief Capture real decoded images and check A/V clock alignment.
		 * @param image Decoded image.
		 */
		void Video(Safe::Shared<const PreviewVideo> image) override {
			std::lock_guard lock(m_mutex);
			if (m_throw_video)
				throw StormByte::Exception("test display failure");
			if (image->m_epoch != m_epoch || image->m_pixels.empty())
				throw StormByte::Exception("invalid image");
			m_image = image;
			m_deferred.push_back(image);
			m_video_positions.push_back(image->m_position);
			if (m_audio_valid && !m_paused)
				m_max_video_lead = std::max(m_max_video_lead, image->m_position - Played());
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
		std::vector<Safe::Shared<const PreviewVideo>> m_deferred;	///< m_deferred GUI messages.
		std::vector<Safe::Shared<const PreviewSubtitle>> m_cues;	///< Active cues.
		std::vector<double> m_video_positions;						///< Captured decode timestamps.
		TestClock::time_point m_started = TestClock::now();			///< Simulated device anchor.
		std::uint64_t m_epoch = 0;									///< Active generation.
		double m_audible = 0;										///< Frozen audio cursor.
		double m_pending_end = 0;									///< Last queued sample time.
		double m_max_video_lead = 0;								///< Worst video lead over audible samples.
		Safe::Shared<const PreviewAudio> m_first_audio;				///< Retained native block for contract assertions.
		std::vector<Safe::Shared<const PreviewAudio>> m_audio_blocks;	///< Optional full fixture capture.
		bool m_capture_audio = false;								///< Capture short deterministic fixtures.
		double m_max_audio_ahead = 0;								///< Worst queued audio lead.
		double m_max_audio_gap = 0;									///< Worst sample-boundary discontinuity within an epoch.
		double m_speed = 1;											///< Device clock speed.
		unsigned m_videos = 0;										///< Delivered images.
		unsigned m_audios = 0;										///< Delivered audio blocks.
		unsigned m_subtitles = 0;									///< Delivered subtitle packets.
		unsigned m_texts = 0;										///< Text/styled rectangles.
		unsigned m_bitmaps = 0;										///< Bitmap rectangles.
		unsigned m_fonts = 0;										///< m_cues carrying embedded fonts.
		unsigned m_invalidations = 0;								///< Synchronous generation changes.
		bool m_active_video = false;								///< Selected video state.
		bool m_audio_valid = false;									///< Device clock availability.
		bool m_paused = true;										///< Device pause state.
		bool m_hold_drain = false;									///< Artificial GUI completion barrier.
		bool m_throw_video = false;									///< Simulated callback failure.
		bool m_throw_invalidate = false;								///< Simulated cleanup callback failure.
};

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
bool Wait(Predicate predicate, double seconds = 5) {
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

// -------------------
// Playback
// -------------------

/**
 * @brief Write a short extensible WAV with distinct, exactly known channel samples.
 * @param path Output fixture.
 * @param rate Source sample rate.
 * @param channels Source channel count.
 * @param mask Exact WAV speaker mask.
 * @return Native-endian expected decoded sample bytes.
 */
std::vector<std::byte> WriteAudioFixture(const std::filesystem::path& path,
	std::uint32_t rate, std::uint16_t channels, std::uint32_t mask) {
	const auto frames = rate / 10;
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
		TEST_REQUIRE(display->m_max_audio_ahead < 0.20);
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
	TEST_REQUIRE(player.Open(Fixture("video/anime_like.mkv")));
	const int video = player.SelectedTrack(MediaType::Video);
	TEST_REQUIRE(video >= 0);
	TEST_REQUIRE(player.Play());
	TEST_REQUIRE(Wait([&] { return player.Position() > 0.30 || player.State() == PlaybackState::Error; }));
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
			TEST_REQUIRE(display->m_max_audio_ahead < 0.20);
		}
	}
	TEST_REQUIRE(player.Close());
	return 0;
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
int main(int argc, char** argv) {
	// -------------------
	// Construct
	// -------------------
	static constexpr std::array tests{
		TestEntry{"test_empty_display", test_empty_display},
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
		TestEntry{"test_audio_paused_seek", test_audio_paused_seek},
		TestEntry{"test_av_seek_switch_epochs", test_av_seek_switch_epochs},
		TestEntry{"test_av_seek_within_frame", test_av_seek_within_frame},
		TestEntry{"test_explicit_errors", test_explicit_errors},
		TestEntry{"test_long_subtitle_duration", test_long_subtitle_duration},
		TestEntry{"test_opus_without_video", test_opus_without_video},
		TestEntry{"test_packed_avi_seek", test_packed_avi_seek},
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
