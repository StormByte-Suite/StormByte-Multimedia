#include "../test_helpers.hxx"
#include "runners.hxx"

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/parameters.hxx>
#include <StormByte/multimedia/backend/file_avio.hxx>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

extern "C" {
	#include <libavformat/avio.h>
	#include <libavutil/error.h>
}

using namespace StormByte::Buffer;
using namespace StormByte::Buffer::IO;
using StormByte::Multimedia::Backend::FileAvio;

namespace {
	constexpr std::string_view payload = "abcdefghijklmnopqrstuvwx";

	class DelayedReader final: public BufferedLocationReader {
		public:
			explicit DelayedReader(std::size_t gate, bool fail = false)
			: BufferedLocationReader("test://delayed", Location::Remote, Parameters{
				StormByte::Buffer::IO::ReadAhead{16}, StormByte::Buffer::IO::MaxMemory{32},
				StormByte::Buffer::IO::MaxWait{std::chrono::milliseconds{1}}}),
			m_gate(gate), m_fail(fail) {}

			~DelayedReader() override {
				Release();
				static_cast<void>(Close());
			}

			void Release() {
				std::lock_guard lock(m_mutex);
				m_ready = true;
				m_cv.notify_all();
			}

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
				if (m_fail)
					return {Status::Error, 0};
				std::unique_lock lock(m_mutex);
				if (m_position >= m_gate && !m_cv.wait_for(lock, std::chrono::seconds{2}, [this] { return m_ready; }))
					return {Status::Error, 0};
				const auto take = std::min(static_cast<std::size_t>(count), payload.size() - m_position);
				dest = FIFO{payload.substr(m_position, take)};
				m_position += take;
				return {m_position == payload.size() ? Status::End : Status::Ok, take};
			}

			Result OriginSeek(std::ptrdiff_t offset, Position mode) override {
				if (mode != Position::Absolute || offset < 0 || static_cast<std::size_t>(offset) > payload.size())
					return {Status::Failed, 0};
				m_position = static_cast<std::size_t>(offset);
				return {Status::Ok, 0};
			}

			StormByte::Safe::Optional<StormByte::ByteSize> OriginSize() const noexcept override {
				return payload.size();
			}

			StormByte::Safe::Shared<StormByte::System::Device> OriginDevice() const override {
				return {};
			}

		private:
			std::mutex m_mutex;
			std::condition_variable m_cv;
			std::size_t m_gate;
			bool m_fail;
			bool m_ready = false;
			std::size_t m_position = 0;
	};

	class SmallWriter final: public BufferedLocationWriter {
		public:
			SmallWriter(bool flushFailure = false, bool pushFailure = false)
			: BufferedLocationWriter("test://small", Location::Remote, Parameters{
				StormByte::Buffer::IO::WriteChunk{4}, StormByte::Buffer::IO::BackPressure{2},
				StormByte::Buffer::IO::MaxMemory{0}}),
			m_flushFailure(flushFailure), m_pushFailure(pushFailure) {}

			~SmallWriter() override {
				static_cast<void>(Close());
			}

			bool Exact(std::string_view expected) const {
				const auto bytes = std::as_bytes(std::span{expected.data(), expected.size()});
				return m_bytes.size() == bytes.size() && std::equal(bytes.begin(), bytes.end(), m_bytes.begin());
			}

		protected:
			Result OriginOpen() override {
				SetState(State::Idle);
				return {Status::Ok, 0};
			}

			Result OriginClose() override {
				SetState(State::Unavailable);
				return {Status::Ok, 0};
			}

			Result OriginPush(std::span<const std::byte> data) override {
				if (m_pushFailure)
					return {Status::Error, 0};
				if (m_position + data.size() > m_bytes.size())
					m_bytes.resize(m_position + data.size());
				std::copy(data.begin(), data.end(), m_bytes.begin() + m_position);
				m_position += data.size();
				return {Status::Ok, data.size()};
			}

			Result OriginFlush() override {
				return {m_flushFailure ? Status::Error : Status::Ok, 0};
			}

			Result OriginTruncate() override {
				m_bytes.clear();
				m_position = 0;
				return {Status::Ok, 0};
			}

			Result OriginSeek(StormByte::ByteSize position) override {
				m_position = static_cast<std::size_t>(position);
				return {Status::Ok, 0};
			}

			StormByte::ByteSize OriginSize() const noexcept override {
				return m_bytes.size();
			}

			StormByte::Safe::Shared<StormByte::System::Device> OriginDevice() const override {
				return {};
			}

		private:
			std::vector<std::byte> m_bytes;
			std::size_t m_position = 0;
			bool m_flushFailure;
			bool m_pushFailure;
	};
}

int test_file_avio_read_retry_before_data() {
	DelayedReader reader{0};
	TEST_REQUIRE(reader.Open());
	std::array<std::byte, 8> untouched{};
	const auto delayed = reader.Read(untouched);
	TEST_REQUIRE(delayed.status == Status::TryAgain && delayed.count == 0);
	FileAvio bridge{reader};
	TEST_REQUIRE(bridge.Arm());
	auto* context = bridge.Context();
	std::array<unsigned char, 32> bytes{};
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), 8) == AVERROR(EAGAIN));
	TEST_REQUIRE(reader.Tell() == StormByte::ByteSize{0});
	reader.Release();
	reader.ReadAhead(0);
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), bytes.size()) == static_cast<int>(payload.size()));
	TEST_REQUIRE((std::string_view{reinterpret_cast<const char*>(bytes.data()), payload.size()} == payload));
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), bytes.size()) == AVERROR_EOF);
	return 0;
}

int test_file_avio_read_retry_between_blocks() {
	DelayedReader reader{16};
	TEST_REQUIRE(reader.Open());
	FileAvio bridge{reader};
	TEST_REQUIRE(bridge.Arm());
	auto* context = bridge.Context();
	std::array<unsigned char, 24> bytes{};
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), 16) == 16);
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data() + 16, 8) == AVERROR(EAGAIN));
	TEST_REQUIRE(reader.Tell() == StormByte::ByteSize{16});
	reader.Release();
	reader.ReadAhead(0);
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data() + 16, 8) == 8);
	TEST_REQUIRE((std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()} == payload));
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), 8) == AVERROR_EOF);
	return 0;
}

int test_file_avio_read_error_is_not_retry() {
	DelayedReader reader{0, true};
	TEST_REQUIRE(reader.Open());
	FileAvio bridge{reader};
	TEST_REQUIRE(bridge.Arm());
	auto* context = bridge.Context();
	std::array<unsigned char, 8> bytes{};
	reader.ReadAhead(0);
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), bytes.size()) == AVERROR(EIO));
	return 0;
}

int test_file_avio_read_seek_and_invalid_buffer() {
	DelayedReader reader{payload.size()};
	TEST_REQUIRE(reader.Open());
	reader.ReadAhead(0);
	FileAvio bridge{reader};
	TEST_REQUIRE(bridge.Arm());
	auto* context = bridge.Context();
	std::array<unsigned char, 8> bytes{};
	TEST_REQUIRE(context->read_packet(context->opaque, nullptr, bytes.size()) == AVERROR(EINVAL));
	TEST_REQUIRE(reader.Tell() == StormByte::ByteSize{0});
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), bytes.size()) == 8);
	TEST_REQUIRE((std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()} == payload.substr(0, 8)));
	TEST_REQUIRE(context->seek(context->opaque, 4, SEEK_SET) == 4);
	TEST_REQUIRE(context->read_packet(context->opaque, bytes.data(), bytes.size()) == 8);
	TEST_REQUIRE((std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()} == payload.substr(4, 8)));
	return 0;
}

int test_file_avio_write_backpressure_and_seek() {
	SmallWriter writer;
	TEST_REQUIRE(writer.Open());
	const auto bytes = std::as_bytes(std::span{payload.data(), payload.size()});
	const auto rejected = writer.Write(bytes);
	TEST_REQUIRE(rejected.status == Status::TryAgain && rejected.count == 0);
	TEST_REQUIRE(writer.Tell() == StormByte::ByteSize{0});
	FileAvio bridge{writer};
	TEST_REQUIRE(bridge.Arm());
	auto* context = bridge.Context();
	avio_write(context, reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
	avio_flush(context);
	TEST_REQUIRE(context->error == 0);
	TEST_REQUIRE(writer.Flush().status == Status::Ok);
	TEST_REQUIRE(writer.Exact(payload));
	TEST_REQUIRE(avio_seek(context, 4, SEEK_SET) == 4);
	constexpr std::string_view replacement = "WXYZ";
	avio_write(context, reinterpret_cast<const unsigned char*>(replacement.data()), replacement.size());
	avio_flush(context);
	TEST_REQUIRE(context->error == 0);
	TEST_REQUIRE(writer.Flush().status == Status::Ok);
	TEST_REQUIRE(writer.Exact("abcdWXYZijklmnopqrstuvwx"));
	return 0;
}

int test_file_avio_write_flush_error() {
	SmallWriter writer{true};
	TEST_REQUIRE(writer.Open());
	FileAvio bridge{writer};
	TEST_REQUIRE(bridge.Arm());
	avio_write(bridge.Context(), reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
	avio_flush(bridge.Context());
	TEST_REQUIRE(bridge.Context()->error == AVERROR(EIO));
	TEST_REQUIRE(writer.Exact({}));
	return 0;
}

int test_file_avio_write_origin_error() {
	SmallWriter writer{false, true};
	TEST_REQUIRE(writer.Open());
	FileAvio bridge{writer};
	TEST_REQUIRE(bridge.Arm());
	avio_write(bridge.Context(), reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
	avio_flush(bridge.Context());
	TEST_REQUIRE(bridge.Context()->error == AVERROR(EIO));
	TEST_REQUIRE(writer.Exact({}));
	return 0;
}

int RunFileAvioTests(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_file_avio_read_retry_before_data", test_file_avio_read_retry_before_data},
		TestEntry{"test_file_avio_read_retry_between_blocks", test_file_avio_read_retry_between_blocks},
		TestEntry{"test_file_avio_read_error_is_not_retry", test_file_avio_read_error_is_not_retry},
		TestEntry{"test_file_avio_read_seek_and_invalid_buffer", test_file_avio_read_seek_and_invalid_buffer},
		TestEntry{"test_file_avio_write_backpressure_and_seek", test_file_avio_write_backpressure_and_seek},
		TestEntry{"test_file_avio_write_flush_error", test_file_avio_write_flush_error},
		TestEntry{"test_file_avio_write_origin_error", test_file_avio_write_origin_error},
	};
	return RunSelectedTest(argc, argv, tests);
}
