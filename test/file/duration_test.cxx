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

#include "../pipeline/helpers.hxx"

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/buffer/io/buffered_location_reader.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/progress.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

using namespace StormByte::Buffer;
using namespace StormByte::Buffer::IO;
using namespace StormByte::Multimedia;
using namespace std::chrono_literals;

namespace {
	class ScanReader final: public BufferedLocationReader {
		public:
			explicit ScanReader(bool knownSize)
			: BufferedLocationReader("test://duration.wav", Location::Remote,
				Parameters{StormByte::Buffer::IO::ReadAhead{0}, StormByte::Buffer::IO::MaxMemory{0}}), m_known_size(knownSize) {
				std::ifstream input(FixturePath("audio/noise_stereo.wav"), std::ios::binary);
				m_bytes.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
				if (!knownSize && m_bytes.size() >= 12) {
					for (std::size_t offset = 12; offset + 8 <= m_bytes.size();) {
						const auto chunk = static_cast<std::uint32_t>(static_cast<unsigned char>(m_bytes[offset + 4]))
							| (static_cast<std::uint32_t>(static_cast<unsigned char>(m_bytes[offset + 5])) << 8)
							| (static_cast<std::uint32_t>(static_cast<unsigned char>(m_bytes[offset + 6])) << 16)
							| (static_cast<std::uint32_t>(static_cast<unsigned char>(m_bytes[offset + 7])) << 24);
						if (std::string_view{m_bytes.data() + offset, 4} == "data") {
							// Exceed FFmpeg's probe window so packet scanning must read the source.
							m_bytes.resize(16 * 1024 * 1024);
							for (unsigned byte = 0; byte < 4; ++byte) {
								m_bytes[4 + byte] = static_cast<char>((m_bytes.size() - 8) >> (8 * byte));
								m_bytes[offset + 4 + byte] = static_cast<char>((m_bytes.size() - offset - 8) >> (8 * byte));
							}
							break;
						}
						offset += 8 + chunk + (chunk & 1);
					}
				}
			}

			~ScanReader() override {
				static_cast<void>(Close());
			}

			std::atomic<unsigned> Pulls{0};
			std::atomic<unsigned> Rewinds{0};
			std::atomic<bool> Slow{false};

		protected:
			Result OriginOpen() override {
				SetState(State::Idle);
				return {Status::Ok, 0};
			}

			Result OriginClose() override {
				SetState(State::Unavailable);
				return {Status::Ok, 0};
			}

			Result OriginPull(StormByte::ByteSize count, FIFO& dest) override {
				if (Slow.load())
					std::this_thread::sleep_for(5ms);
				++Pulls;
				const auto take = std::min(static_cast<std::size_t>(count), m_bytes.size() - m_position);
				dest = FIFO{std::string_view{m_bytes.data() + m_position, take}};
				m_position += take;
				return {m_position == m_bytes.size() ? Status::End : Status::Ok, take};
			}

			Result OriginSeek(std::ptrdiff_t offset, Position mode) override {
				if (mode != Position::Absolute || offset < 0 || static_cast<std::size_t>(offset) > m_bytes.size())
					return {Status::Failed, 0};
				m_position = static_cast<std::size_t>(offset);
				if (m_position == 0)
					++Rewinds;
				return {Status::Ok, 0};
			}

			StormByte::Safe::Optional<StormByte::ByteSize> OriginSize() const noexcept override {
				if (m_known_size)
					return m_bytes.size();
				return {};
			}

			StormByte::Safe::Shared<StormByte::System::Device> OriginDevice() const override {
				return {};
			}

		private:
			std::vector<char> m_bytes;
			std::size_t m_position = 0;
			bool m_known_size;
	};

	struct ScanObserver {
		unsigned Calls = 0;
		bool Complete = false;
		bool Cancel = true;
	};

	File::DurationProgress Observe(ScanObserver& state) {
		return File::DurationProgress(new ScanObserver*(&state),
			[](void* context, double percent) {
				auto& observer = **static_cast<ScanObserver**>(context);
				++observer.Calls;
				observer.Complete = percent == 100.0;
				return observer.Cancel && observer.Calls > 1
					? StormByte::Safe::Status::Failure : StormByte::Safe::Status::Success;
			},
			[](const void* context) noexcept -> void* {
				return new (std::nothrow) ScanObserver*(*static_cast<ScanObserver* const*>(context));
			},
			[](void* context) noexcept { delete static_cast<ScanObserver**>(context); });
	}

	int CheckInterruptedScan(bool knownSize) {
		ScanReader reader{knownSize};
		auto opened = File::Open(reader);
		TEST_REQUIRE(opened);
		reader.Slow = true;
		ScanObserver state;
		auto observer = Observe(state);
		const auto rewinds = reader.Rewinds.load();
		static_cast<void>(opened->Duration(observer));
		TEST_REQUIRE(state.Calls > 1);
		TEST_REQUIRE(!state.Complete);
		TEST_REQUIRE(reader.Tell() == StormByte::ByteSize{0});
		TEST_REQUIRE(reader.Rewinds.load() > rewinds);
		const auto calls = state.Calls;
		state.Cancel = false;
		TEST_REQUIRE(opened->Duration(observer));
		TEST_REQUIRE(state.Calls > calls);
		TEST_REQUIRE(state.Complete);
		TEST_REQUIRE(reader.Tell() == StormByte::ByteSize{0});
		const auto completedCalls = state.Calls;
		TEST_REQUIRE(opened->Duration(observer));
		TEST_REQUIRE(state.Calls == completedCalls);
		return 0;
	}

	class PausingTranscoder final: public Pipeline::Transcoder {
		public:
			using Pipeline::Transcoder::Transcoder;

			std::atomic<bool> ScanPaused{false};
			std::atomic<unsigned> Aborted{0};
			std::atomic<unsigned> Errors{0};
			bool CancelScan = false;

		protected:
			void OnProgress() noexcept override {
				const auto clock = Progress();
				if (clock && clock->CalculatingDuration() && clock->DurationCalculation()
					&& clock->DurationCalculation().value() > 0.0
					&& clock->DurationCalculation().value() < 100.0 && !ScanPaused.exchange(true)) {
					if (CancelScan)
						Cancel();
					else
						Pause();
				}
			}

			void OnAborted() noexcept override { ++Aborted; }
			void OnError(const StormByte::Safe::String&) noexcept override { ++Errors; }
	};

	template<typename Predicate>
	bool WaitUntil(Predicate ready) {
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (!ready()) {
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(1ms);
		}
		return true;
	}

	int CheckPausedScan(bool cancel) {
		auto reader = StormByte::Safe::Unique<BufferedLocationReader>::MakePointer<ScanReader>(true);
		auto* source = static_cast<ScanReader*>(reader.get());
		auto writer = StormByte::Safe::Unique<BufferedLocationWriter>::MakePointer<BufferedFileWriter>(
			TestLocation(OutputPath(cancel ? "cancelled-duration.mkv" : "resumed-duration.mkv")));
		PausingTranscoder job{std::move(reader), std::move(writer), MakeLogger()};
		job.Audio(0).Remux();
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		TEST_REQUIRE(WaitUntil([&] { return job.Status() == Pipeline::Status::Paused; }));
		std::this_thread::sleep_for(50ms);
		const auto pulls = source->Pulls.load();
		const auto percent = job.Progress()->DurationCalculation();
		std::this_thread::sleep_for(100ms);
		TEST_REQUIRE(job.Status() == Pipeline::Status::Paused);
		TEST_REQUIRE(source->Pulls.load() == pulls);
		TEST_REQUIRE(job.Progress()->DurationCalculation() == percent);
		if (cancel) {
			TEST_REQUIRE(source->Tell() > StormByte::ByteSize{0});
			job.Cancel();
			TEST_REQUIRE(WaitUntil([&] { return job.Aborted.load() == 1; }));
			TEST_REQUIRE(job.Status() == Pipeline::Status::Aborted);
			TEST_REQUIRE(job.Errors.load() == 0);
			TEST_REQUIRE(source->Tell() == StormByte::ByteSize{0});
			TEST_REQUIRE(!std::filesystem::exists(OutputPath("cancelled-duration.mkv")));
		}
		else {
			job.Resume();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
			TEST_REQUIRE(job.Aborted.load() == 0);
			TEST_REQUIRE(job.Errors.load() == 0);
		}
		return 0;
	}
}

int test_duration_cancel_rewinds_and_retries() {
	return CheckInterruptedScan(true);
}

int test_unknown_size_duration_cancel_rewinds_and_retries() {
	return CheckInterruptedScan(false);
}

int test_duration_pause_and_resume() {
	return CheckPausedScan(false);
}

int test_duration_cancel_while_paused() {
	return CheckPausedScan(true);
}

int test_duration_cancel_while_running() {
	PausingTranscoder job{TestLocation(FixturePath("audio/noise_stereo.wav")),
		TestLocation(OutputPath("cancelled-running-duration.mkv")), MakeLogger()};
	job.CancelScan = true;
	job.Audio(0).Remux();
	TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
	job.Run();
	TEST_REQUIRE(WaitUntil([&] { return job.Aborted.load() == 1; }));
	TEST_REQUIRE(job.Status() == Pipeline::Status::Aborted);
	TEST_REQUIRE(job.Errors.load() == 0);
	TEST_REQUIRE(!std::filesystem::exists(OutputPath("cancelled-running-duration.mkv")));
	return 0;
}

int test_supplied_duration_skips_scan_control() {
	ScanReader reader{true};
	auto opened = File::Open(reader, 2000000000LL);
	TEST_REQUIRE(opened);
	ScanObserver state;
	auto observer = Observe(state);
	const auto pulls = reader.Pulls.load();
	TEST_REQUIRE(opened->Duration(observer));
	TEST_REQUIRE(state.Calls == 0);
	TEST_REQUIRE(reader.Pulls.load() == pulls);
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_duration_cancel_rewinds_and_retries", test_duration_cancel_rewinds_and_retries},
		TestEntry{"test_unknown_size_duration_cancel_rewinds_and_retries", test_unknown_size_duration_cancel_rewinds_and_retries},
		TestEntry{"test_duration_pause_and_resume", test_duration_pause_and_resume},
		TestEntry{"test_duration_cancel_while_paused", test_duration_cancel_while_paused},
		TestEntry{"test_duration_cancel_while_running", test_duration_cancel_while_running},
		TestEntry{"test_supplied_duration_skips_scan_control", test_supplied_duration_skips_scan_control},
	};
	return RunSelectedTest(argc, argv, tests);
}
