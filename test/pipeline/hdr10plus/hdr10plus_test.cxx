#include "../helpers.hxx"

#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/filters/ffmpeg.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <tables/decoder/table.hxx>
#include <tables/encoder/table.hxx>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/hdr_dynamic_metadata.h>
	#include <libavutil/mem.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	constexpr std::string_view Fixture = "video/hdr10plus_metadata_source.mkv";
	using Payload = std::vector<std::uint8_t>;

	Payload Serialize(const AVDynamicHDRPlus& metadata) {
		std::uint8_t* bytes = nullptr;
		std::size_t size = 0;
		if (av_dynamic_hdr_plus_to_t35(&metadata, &bytes, &size) < 0)
			return {};
		Payload result(bytes, bytes + size);
		av_free(bytes);
		return result;
	}

	Payload ExpectedPayload(std::size_t index) {
		AVDynamicHDRPlus metadata{};
		metadata.application_version = 1;
		metadata.num_windows = 1;
		const bool second = index % 2 != 0;
		metadata.targeted_system_display_maximum_luminance = {second ? 600 : 400, 1};
		auto& window = metadata.params[0];
		const std::array<int, 3> maxima = second
			? std::array{90000, 70000, 50000} : std::array{100000, 80000, 60000};
		for (std::size_t channel = 0; channel < maxima.size(); ++channel)
			window.maxscl[channel] = {maxima[channel], 100000};
		window.average_maxrgb = {second ? 20000 : 10000, 100000};
		constexpr std::array Percentages{1, 5, 10, 25, 50, 75, 90, 95, 99};
		const std::array<int, 9> values = second
			? std::array{200, 1000, 2000, 5000, 20000, 30000, 50000, 70000, 85000}
			: std::array{100, 500, 1000, 2500, 10000, 20000, 40000, 60000, 90000};
		window.num_distribution_maxrgb_percentiles = Percentages.size();
		for (std::size_t percentile = 0; percentile < Percentages.size(); ++percentile) {
			window.distribution_maxrgb[percentile].percentage = Percentages[percentile];
			window.distribution_maxrgb[percentile].percentile = {values[percentile], 100000};
		}
		window.fraction_bright_pixels = {0, 1000};
		window.tone_mapping_flag = 1;
		window.knee_point_x = {second ? 1280 : 1024, 4095};
		window.knee_point_y = {second ? 1792 : 1536, 4095};
		window.num_bezier_curve_anchors = 4;
		for (int anchor = 0; anchor < window.num_bezier_curve_anchors; ++anchor)
			window.bezier_curve_anchors[anchor] = {128 + anchor * 256 + (second ? 64 : 0), 1023};
		return Serialize(metadata);
	}

	struct Sample {
		std::optional<std::int64_t> m_pts;
		Payload m_payload;
		Property::Video m_video;
	};

	struct Trace {
		std::vector<Sample> m_source;
		std::vector<Sample> m_destination;
	};

	class Audit final: public Filter::Analytics {
		public:
			Audit(StormByte::Safe::Shared<StormByte::Logger::Log> logger,
				StormByte::Safe::Shared<Trace> trace, bool raw = false)
			: Filter::Analytics(std::move(logger), "audit-hdr10plus"), m_trace(std::move(trace)), m_raw(raw) {}

			Audit(const Audit& other) = delete;

			Audit(Audit&& other) noexcept = delete;

			~Audit() noexcept override = default;

			Audit& operator=(const Audit& other) = delete;

			Audit& operator=(Audit&& other) noexcept = delete;

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Video;
			}

		protected:
			void Clean() noexcept override {}

			void Setup() noexcept override {}

			void Process(const Frame& frame) noexcept override {
				if (!frame.Video() || (!m_raw && !frame.Pts())) {
					Fail("HDR10+ frame lacks video properties or timestamp");
					return;
				}
				const auto& handle = AVFrame();
				int count = 0;
				for (int side = 0; side < handle.SideDataCount(); ++side) {
					const auto* metadata = handle.SideDataAt(side);
					if (metadata && metadata->type == AV_FRAME_DATA_DYNAMIC_HDR_PLUS)
						++count;
				}
				const auto* metadata = handle.SideData(AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
				if (count != 1 || !metadata) {
					Fail(count == 0 ? "HDR10+ frame has missing metadata" : "HDR10+ frame has duplicate metadata");
					return;
				}
				if (!metadata->data || metadata->size < sizeof(AVDynamicHDRPlus)) {
					Fail("HDR10+ frame has truncated metadata");
					return;
				}
				auto payload = Serialize(*reinterpret_cast<const AVDynamicHDRPlus*>(metadata->data));
				if (payload.empty()) {
					Fail("HDR10+ metadata cannot be serialized");
					return;
				}
				const auto pts = frame.Pts() ? std::optional{frame.Pts()->Nanoseconds().count()} : std::nullopt;
				Sample sample{pts, std::move(payload), frame.Video().value()};
				if (frame.Producer() == Producer::Decoder)
					m_trace->m_source.push_back(std::move(sample));
				else if (frame.Producer() == Producer::Encoder || frame.Producer() == Producer::Remuxer)
					m_trace->m_destination.push_back(std::move(sample));
			}

		private:
			StormByte::Safe::Shared<Trace> m_trace;
			bool m_raw;
	};

	template<class Point>
	bool SamePoint(const Point& before, const Point& after, int tolerance = 0, int verticalTolerance = 0) {
		return std::abs(static_cast<std::int64_t>(before.X()) - after.X()) <= tolerance
			&& std::abs(static_cast<std::int64_t>(before.Y()) - after.Y()) <= verticalTolerance;
	}

	int CheckVideo(const Property::Video& before, const Property::Video& after, bool av1) {
		TEST_REQUIRE(after.HDR10() && before.HDR10());
		const auto& original = before.HDR10().value();
		const auto& actual = after.HDR10().value();
		TEST_REQUIRE(actual.Origin() == Property::HDR10::Source::Metadata);
		TEST_REQUIRE(actual.IsHDR10Plus());
		const int chromaTolerance = av1 ? 1 : 0;
		TEST_REQUIRE(SamePoint(original.Red(), actual.Red(), chromaTolerance, chromaTolerance));
		TEST_REQUIRE(SamePoint(original.Green(), actual.Green(), chromaTolerance, chromaTolerance));
		TEST_REQUIRE(SamePoint(original.Blue(), actual.Blue(), chromaTolerance, chromaTolerance));
		TEST_REQUIRE(SamePoint(original.White(), actual.White(), chromaTolerance, chromaTolerance));
		TEST_REQUIRE(SamePoint(original.Luminance(), actual.Luminance(), av1 ? 1 : 0, av1 ? 20 : 0));
		TEST_REQUIRE(original.LightLevel() && actual.LightLevel());
		TEST_REQUIRE(SamePoint(original.LightLevel().value(), actual.LightLevel().value()));
		TEST_REQUIRE(after.Resolution().Width() == before.Resolution().Width());
		TEST_REQUIRE(after.Resolution().Height() == before.Resolution().Height());
		TEST_REQUIRE(after.Color().PixelFormat() == before.Color().PixelFormat());
		TEST_REQUIRE(after.Color().Primaries() == before.Color().Primaries());
		TEST_REQUIRE(after.Color().Transfer() == before.Color().Transfer());
		TEST_REQUIRE(after.Color().Space() == before.Color().Space());
		TEST_REQUIRE(after.Color().Range() == before.Color().Range());
		return 0;
	}

	int CheckSamples(std::vector<Sample>& source, std::vector<Sample>& destination, bool av1, bool raw = false) {
		if (source.size() != 48 || destination.size() != 48)
			std::cerr << "[DETAIL] HDR10+ frame counts: source=" << source.size()
				<< " destination=" << destination.size() << " raw=" << raw << '\n';
		TEST_REQUIRE(source.size() == 48);
		TEST_REQUIRE(destination.size() == 48);
		if (!raw) {
			for (const auto& sample : source)
				TEST_REQUIRE(sample.m_pts);
			for (const auto& sample : destination)
				TEST_REQUIRE(sample.m_pts);
			std::ranges::sort(source, {}, &Sample::m_pts);
			std::ranges::sort(destination, {}, &Sample::m_pts);
		}
		TEST_REQUIRE(ExpectedPayload(0) != ExpectedPayload(1));
		for (std::size_t index = 0; index < source.size(); ++index) {
			TEST_REQUIRE(source[index].m_payload == ExpectedPayload(index));
			TEST_REQUIRE(destination[index].m_payload == source[index].m_payload);
			if (!raw) {
				TEST_REQUIRE(std::abs(*destination[index].m_pts - *source[index].m_pts) <= 1000000);
				TEST_REQUIRE(std::abs(*source[index].m_pts - static_cast<std::int64_t>(index) * 1000000000 / 24) <= 1000000);
				if (index != 0) {
					TEST_REQUIRE(source[index].m_pts > source[index - 1].m_pts);
					TEST_REQUIRE(destination[index].m_pts > destination[index - 1].m_pts);
				}
			}
			TEST_REQUIRE(CheckVideo(source[index].m_video, destination[index].m_video, av1) == 0);
		}
		return 0;
	}

	int CheckFile(const std::filesystem::path& path, bool plus) {
		auto file = File::Open(TestLocation(path));
		TEST_REQUIRE(file && file.value().Streams().size() == 1);
		const auto video = file.value().Streams()[0].Video();
		TEST_REQUIRE(video && video->HDR10());
		TEST_REQUIRE(video->HDR10()->Origin() == Property::HDR10::Source::Metadata);
		TEST_REQUIRE(video->HDR10()->IsHDR10Plus() == plus);
		TEST_REQUIRE(video->Resolution().Width() == 1920);
		TEST_REQUIRE(video->Resolution().Height() == 1080);
		return 0;
	}

	int Run(const std::filesystem::path& input, const std::filesystem::path& output,
		StormByte::Safe::Shared<Trace> trace, std::string_view codecName = {},
		std::string_view implementation = {}, std::string_view decoder = {}, bool raw = false) {
		auto logger = MakeLogger();
		Transcoder job{TestLocation(input), TestLocation(output), logger, 2000000000LL};
		auto track = job.Video(0);
		if (!decoder.empty()) {
			const std::string name{decoder};
			if (!avcodec_find_decoder_by_name(name.c_str()))
				return TEST_SKIP;
			track.Implementation(ImplementationSide::Decoder, StormByte::Safe::String{decoder});
		}
		if (implementation.empty())
			track.Remux();
		else {
			const std::string name{implementation};
			auto codec = Registry::Instance().FindCodec(codecName);
			TEST_REQUIRE(codec);
			if (!avcodec_find_encoder_by_name(name.c_str())
				|| !codec.value().get().HasAccess(Access{Operation::Write}))
				return TEST_SKIP;
			track.Codec(codec.value().get())
				.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{implementation});
			if (implementation == "libx265")
				track.Preset(StormByte::Safe::String{"ultrafast"}).CRF(40);
			else {
				StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options;
				options.emplace(StormByte::Safe::String{"cpu-used"}, StormByte::Safe::String{"8"});
				if (implementation == "libvpx-vp9")
					options.emplace(StormByte::Safe::String{"deadline"}, StormByte::Safe::String{"realtime"});
				track.FineTune(std::move(options)).BitRate(500000);
			}
		}
		job.Filter<Audit>(logger, trace, raw);
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		job.Run();
		return implementation.empty() && decoder.empty() ? WaitForTranscoder(job) : WaitForOptionalCodec(job);
	}

	int CheckPipeline(std::string_view codec = {}, std::string_view encoder = {}, std::string_view decoder = {}) {
		const auto output = OutputPath("hdr10plus.mkv");
		auto trace = StormByte::Safe::MakeShared<Trace>();
		if (const int result = Run(FixturePath(Fixture), output, trace, codec, encoder); result != 0)
			return result;
		const bool av1 = codec == "AV1";
		TEST_REQUIRE(CheckSamples(trace->m_source, trace->m_destination, av1) == 0);
		TEST_REQUIRE(CheckFile(output, true) == 0);
		auto reopened = StormByte::Safe::MakeShared<Trace>();
		if (const int result = Run(output, OutputPath("reopened.mkv"), reopened, {}, {}, decoder); result != 0)
			return result;
		TEST_REQUIRE(CheckSamples(trace->m_source, reopened->m_source, av1) == 0);
		TEST_REQUIRE(CheckSamples(trace->m_source, reopened->m_destination, av1) == 0);
		return 0;
	}
}

// -------------------
// Detection
// -------------------

int test_file_hdr10plus_detection() {
	TEST_REQUIRE(CheckFile(FixturePath(Fixture), true) == 0);
	TEST_REQUIRE(CheckFile(FixturePath("video/hdr10_metadata_source.mkv"), false) == 0);
	return 0;
}

int test_hdr10plus_capabilities() {
	for (const auto& row : Tables::Encoder::Video()) {
		const std::string_view name{row.name};
		TEST_REQUIRE(row.features.Has(Feature::HDR10Plus)
			== (name == "libx265" || name == "libaom-av1" || name == "libvpx-vp9"));
	}
	for (const auto& row : Tables::Decoder::Video()) {
		const std::string_view name{row.name};
		TEST_REQUIRE(row.features.Has(Feature::HDR10Plus)
			== (name == "hevc" || name == "av1" || name == "libdav1d" || name == "vp9" || name == "libvpx-vp9"));
	}
	return 0;
}

// -------------------
// Roundtrip
// -------------------

int test_aom_hdr10plus() {
	return CheckPipeline("AV1", "libaom-av1");
}

int test_hevc_av1_hevc_hdr10plus() {
	const auto av1 = OutputPath("intermediate-av1.mkv");
	const auto hevc = OutputPath("roundtrip-hevc.mkv");
	auto first = StormByte::Safe::MakeShared<Trace>();
	if (const int result = Run(FixturePath(Fixture), av1, first, "AV1", "libaom-av1"); result != 0)
		return result;
	TEST_REQUIRE(CheckSamples(first->m_source, first->m_destination, true) == 0);
	auto second = StormByte::Safe::MakeShared<Trace>();
	if (const int result = Run(av1, hevc, second, "H.265", "libx265"); result != 0)
		return result;
	TEST_REQUIRE(CheckSamples(first->m_source, second->m_source, true) == 0);
	TEST_REQUIRE(CheckSamples(first->m_source, second->m_destination, true) == 0);
	TEST_REQUIRE(CheckFile(av1, true) == 0);
	TEST_REQUIRE(CheckFile(hevc, true) == 0);
	auto reopened = StormByte::Safe::MakeShared<Trace>();
	TEST_REQUIRE(Run(hevc, OutputPath("roundtrip-reopened.mkv"), reopened) == 0);
	TEST_REQUIRE(CheckSamples(first->m_source, reopened->m_source, true) == 0);
	TEST_REQUIRE(CheckSamples(first->m_source, reopened->m_destination, true) == 0);
	const auto elementary = OutputPath("roundtrip.hevc");
	auto extracted = StormByte::Safe::MakeShared<Trace>();
	TEST_REQUIRE(Run(hevc, elementary, extracted) == 0);
	TEST_REQUIRE(CheckSamples(first->m_source, extracted->m_source, true) == 0);
	auto native = StormByte::Safe::MakeShared<Trace>();
	TEST_REQUIRE(Run(elementary, OutputPath("native-hevc.mkv"), native, {}, {}, "hevc", true) == 0);
	TEST_REQUIRE(CheckSamples(first->m_source, native->m_source, true, true) == 0);
	return 0;
}

int test_native_vp9_hdr10plus() {
	return CheckPipeline("VP9", "libvpx-vp9", "vp9");
}

int test_remux_hdr10plus() {
	return CheckPipeline();
}

int test_vp9_hdr10plus() {
	return CheckPipeline("VP9", "libvpx-vp9", "libvpx-vp9");
}

int test_x265_hdr10plus() {
	return CheckPipeline("H.265", "libx265");
}

int main(int argc, char** argv) {
	const std::array tests{
		// -------------------
		// Detection
		// -------------------
		TestEntry{"test_file_hdr10plus_detection", test_file_hdr10plus_detection},
		TestEntry{"test_hdr10plus_capabilities", test_hdr10plus_capabilities},
		// -------------------
		// Roundtrip
		// -------------------
		TestEntry{"test_aom_hdr10plus", test_aom_hdr10plus},
		TestEntry{"test_hevc_av1_hevc_hdr10plus", test_hevc_av1_hevc_hdr10plus},
		TestEntry{"test_native_vp9_hdr10plus", test_native_vp9_hdr10plus},
		TestEntry{"test_remux_hdr10plus", test_remux_hdr10plus},
		TestEntry{"test_vp9_hdr10plus", test_vp9_hdr10plus},
		TestEntry{"test_x265_hdr10plus", test_x265_hdr10plus},
	};
	return RunSelectedTest(argc, argv, tests);
}