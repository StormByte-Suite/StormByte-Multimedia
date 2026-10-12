#include "../../test_helpers.hxx"
#include "../hardware_capability.hxx"

#include <StormByte/multimedia/ffmpeg/video_input.hxx>

#include <array>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>

extern "C" {
	#include <libavutil/frame.h>
}

namespace FFmpeg = StormByte::Multimedia::FFmpeg;
namespace Input = FFmpeg::VideoInput;

namespace {
	/**
	 * @brief Test-only access to frame strides without extending the production API.
	 */
	class TestFrame final: public FFmpeg::AVFrame {
		public:
			TestFrame() noexcept = default;
			TestFrame(const TestFrame&) noexcept = default;
			TestFrame(TestFrame&&) noexcept = default;
			~TestFrame() noexcept override = default;
			TestFrame& operator=(const TestFrame&) noexcept = default;
			TestFrame& operator=(TestFrame&&) noexcept = default;

			/**
			 * @brief Reverses stored row traversal while keeping buffer ownership intact.
			 */
			void NegativeStride() noexcept {
				const int planes = Format() == AV_PIX_FMT_YUV420P10LE ? 3 : 2;
				for (int plane = 0; plane < planes; ++plane) {
					const int height = plane == 0 ? Height() : Height() / 2 + Height() % 2;
					m_ptr->data[plane] += static_cast<std::ptrdiff_t>(height - 1) * m_ptr->linesize[plane];
					m_ptr->linesize[plane] = -m_ptr->linesize[plane];
				}
			}

			/**
			 * @brief Borrows the header for the standalone libav send/drain probe.
			 * @return Owned raw frame header.
			 */
			::AVFrame* Raw() noexcept { return m_ptr; }
	};

	/**
	 * @brief Computes a component's boundary-heavy sample pattern.
	 * @param component Y, U or V index.
	 * @param row Logical sample row.
	 * @param column Logical sample column.
	 * @return A 10-bit sample, including both extrema and one.
	 */
	unsigned Sample(int component, int row, int column) {
		constexpr std::array<unsigned, 6> Samples{0, 1, 1023, 2, 511, 1022};
		return Samples[static_cast<std::size_t>((component * 2 + row + column) % 6)];
	}

	/**
	 * @brief Writes or checks logical samples using independent byte-level expectations.
	 * @param frame CPU test frame.
	 * @param write Whether to populate instead of check.
	 * @return Whether all checked samples match.
	 */
	bool Samples(FFmpeg::AVFrame& frame, bool write) {
		const bool planar = frame.Format() == AV_PIX_FMT_YUV420P10LE;
		for (int component = 0; component < 3; ++component) {
			const int width = component == 0 ? frame.Width() : frame.Width() / 2 + frame.Width() % 2;
			const int height = component == 0 ? frame.Height() : frame.Height() / 2 + frame.Height() % 2;
			const int plane = planar || component == 0 ? component : 1;
			const int step = planar || component == 0 ? 2 : 4;
			const int offset = !planar && component == 2 ? 2 : 0;
			for (int row = 0; row < height; ++row) {
				auto* data = frame.Data(plane) + static_cast<std::ptrdiff_t>(row) * frame.Linesize(plane) + offset;
				for (int column = 0; column < width; ++column) {
					const unsigned expected = Sample(component, row, column) << (planar ? 0 : 6);
					auto* bytes = data + column * step;
					if (write) {
						bytes[0] = static_cast<std::uint8_t>(expected);
						bytes[1] = static_cast<std::uint8_t>(expected >> 8);
					}
					else if (bytes[0] != static_cast<std::uint8_t>(expected)
						|| bytes[1] != static_cast<std::uint8_t>(expected >> 8))
						return false;
				}
			}
		}
		return true;
	}

	constexpr std::array SideTypes{AV_FRAME_DATA_MASTERING_DISPLAY_METADATA,
		AV_FRAME_DATA_CONTENT_LIGHT_LEVEL, AV_FRAME_DATA_DYNAMIC_HDR_PLUS,
		AV_FRAME_DATA_DOVI_METADATA, AV_FRAME_DATA_DOVI_RPU_BUFFER, AV_FRAME_DATA_A53_CC};

	/**
	 * @brief Populates timing, colorimetry and opaque native metadata for exact-copy checks.
	 * @param frame Frame to populate.
	 * @return Whether side-data allocation succeeded.
	 */
	bool Metadata(FFmpeg::AVFrame& frame) {
		frame.Pts(91);
		frame.BestEffortTimestamp(90);
		frame.DurationTicks(7);
		frame.TimeBase({1, 1000});
		frame.SampleAspectRatio({4, 3});
		frame.ColorRange(AVCOL_RANGE_JPEG);
		frame.ColorSpace(AVCOL_SPC_BT2020_NCL);
		frame.ColorPrimaries(AVCOL_PRI_BT2020);
		frame.ColorTransfer(AVCOL_TRC_SMPTE2084);
		frame.ChromaLocation(AVCHROMA_LOC_LEFT);
		for (const auto type : SideTypes) {
			auto* bytes = frame.NewSideData(type, 16);
			if (!bytes)
				return false;
			std::memset(bytes, static_cast<int>(type), 16);
		}
		return true;
	}

	/**
	 * @brief Checks all populated properties and side-data bytes against their source.
	 * @param source Original frame.
	 * @param destination Referenced or repacked frame.
	 * @return Whether the entire metadata sample is preserved.
	 */
	bool SameMetadata(const FFmpeg::AVFrame& source, const FFmpeg::AVFrame& destination) {
		if (source.Width() != destination.Width() || source.Height() != destination.Height()
			|| source.Pts() != destination.Pts() || source.DurationTicks() != destination.DurationTicks()
			|| source.BestEffortTimestamp() != destination.BestEffortTimestamp()
			|| source.TimeBase() != destination.TimeBase() || source.SampleAspectRatio() != destination.SampleAspectRatio()
			|| source.ColorRange() != destination.ColorRange() || source.ColorSpace() != destination.ColorSpace()
			|| source.ColorPrimaries() != destination.ColorPrimaries() || source.ColorTransfer() != destination.ColorTransfer()
			|| source.ChromaLocation() != destination.ChromaLocation() || source.SideDataCount() != destination.SideDataCount())
			return false;
		for (const auto type : SideTypes) {
			const auto* original = source.SideData(type);
			const auto* copied = destination.SideData(type);
			if (!original || !copied || original->size != copied->size
				|| std::memcmp(original->data, copied->data, original->size) != 0)
				return false;
		}
		return true;
	}
}

// -------------------
// Input
// -------------------

int test_encoder_configs() {
	void* state = nullptr;
	int checked = 0;
	while (const auto* codec = av_codec_iterate(&state)) {
		if (!av_codec_is_encoder(codec) || codec->type != AVMEDIA_TYPE_VIDEO)
			continue;
		const void* formats = nullptr;
		int count = 0;
		const int query = avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &formats, &count);
		const char* error = nullptr;
		const int selected = Input::Negotiate(codec, AV_PIX_FMT_YUV420P10LE, error);
		if (query < 0 || count < 0 || (formats && count == 0)) {
			TEST_REQUIRE(selected == AV_PIX_FMT_NONE && error);
			continue;
		}
		int expected = Input::Select(AV_PIX_FMT_YUV420P10LE, formats
			? std::span{static_cast<const AVPixelFormat*>(formats), static_cast<std::size_t>(count)}
			: std::span<const AVPixelFormat>{});
		if (expected == AV_PIX_FMT_NONE) {
			for (int index = 0; const auto* configuration = avcodec_get_hw_config(codec, index); ++index) {
				if (!(configuration->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
					|| configuration->device_type == AV_HWDEVICE_TYPE_NONE || !Input::Hardware(configuration->pix_fmt))
					continue;
				bool supported = !formats;
				for (int position = 0; formats && position < count; ++position)
					if (static_cast<const AVPixelFormat*>(formats)[position] == configuration->pix_fmt)
						supported = true;
				if (supported) {
					expected = configuration->pix_fmt;
					break;
				}
			}
		}
		TEST_REQUIRE(selected == expected);
		TEST_REQUIRE((selected == AV_PIX_FMT_NONE) == (error != nullptr));
		++checked;
	}
	TEST_REQUIRE(checked > 0);
	return 0;
}

int test_hardware_plan_selection() {
	const AVCodecHWConfig vaapi{AV_PIX_FMT_VAAPI, AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, AV_HWDEVICE_TYPE_VAAPI};
	const AVCodecHWConfig cuda{AV_PIX_FMT_CUDA, AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, AV_HWDEVICE_TYPE_CUDA};
	const AVCodecHWConfig qsv{AV_PIX_FMT_QSV, AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, AV_HWDEVICE_TYPE_QSV};
	const AVCodecHWConfig deviceOnly{AV_PIX_FMT_VIDEOTOOLBOX, AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX, AV_HWDEVICE_TYPE_VIDEOTOOLBOX};
	const AVCodecHWConfig internal{AV_PIX_FMT_VAAPI, AV_CODEC_HW_CONFIG_METHOD_INTERNAL, AV_HWDEVICE_TYPE_VAAPI};
	const AVCodecHWConfig noDevice{AV_PIX_FMT_VAAPI, AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, AV_HWDEVICE_TYPE_NONE};
	const AVCodecHWConfig cpu{AV_PIX_FMT_P010LE, AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, AV_HWDEVICE_TYPE_CUDA};
	const std::array<const AVCodecHWConfig*, 3> hardware{&vaapi, &cuda, &qsv};
	const std::array<const AVCodecHWConfig*, 5> unsupported{nullptr, &deviceOnly, &internal, &noDevice, &cpu};
	constexpr std::array vaapiFormats{AV_PIX_FMT_VAAPI};
	constexpr std::array cudaFormats{AV_PIX_FMT_P010LE, AV_PIX_FMT_CUDA};
	constexpr std::array qsvFormats{AV_PIX_FMT_QSV};
	TEST_REQUIRE(Input::SelectHardware(vaapiFormats, hardware) == AV_PIX_FMT_VAAPI);
	TEST_REQUIRE(Input::SelectHardware(cudaFormats, hardware) == AV_PIX_FMT_CUDA);
	TEST_REQUIRE(Input::SelectHardware(qsvFormats, hardware) == AV_PIX_FMT_QSV);
	TEST_REQUIRE(Input::SelectHardware({}, unsupported) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::SelectHardware({}, {}) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::SelectHardware({}, hardware) == AV_PIX_FMT_VAAPI);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P10LE, cudaFormats) == AV_PIX_FMT_P010LE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV422P10LE, cudaFormats) == AV_PIX_FMT_NONE);
	constexpr std::array deviceFormats{AV_PIX_FMT_NV12, AV_PIX_FMT_P010LE, AV_PIX_FMT_YUV420P10LE};
	TEST_REQUIRE(Input::SelectTransfer(AV_PIX_FMT_YUV420P10LE, deviceFormats) == AV_PIX_FMT_P010LE);
	TEST_REQUIRE(Input::SelectTransfer(AV_PIX_FMT_YUV422P10LE, deviceFormats) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::SelectTransfer(AV_PIX_FMT_YUV420P10LE, {}) == AV_PIX_FMT_NONE);
	return 0;
}

int test_cpu_context_preference() {
	void* state = nullptr;
	int checked = 0;
	while (const auto* codec = av_codec_iterate(&state)) {
		if (!av_codec_is_encoder(codec) || codec->type != AVMEDIA_TYPE_VIDEO)
			continue;
		const void* formats = nullptr;
		int count = 0;
		if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &formats, &count) < 0
			|| count < 0 || (formats && count == 0))
			continue;
		const int cpu = Input::Select(AV_PIX_FMT_YUV420P10LE, formats
			? std::span{static_cast<const AVPixelFormat*>(formats), static_cast<std::size_t>(count)}
			: std::span<const AVPixelFormat>{});
		if (cpu == AV_PIX_FMT_NONE)
			continue;
		const auto freeContext = [](AVCodecContext* context) { avcodec_free_context(&context); };
		std::unique_ptr<AVCodecContext, decltype(freeContext)> context(avcodec_alloc_context3(codec), freeContext);
		TEST_REQUIRE(context);
		context->width = 64;
		context->height = 64;
		context->pix_fmt = AV_PIX_FMT_YUV420P10LE;
		std::string error;
		TEST_REQUIRE(Input::Configure(context.get(), codec, nullptr, error));
		TEST_REQUIRE(error.empty() && context->pix_fmt == cpu && context->sw_pix_fmt == cpu);
		TEST_REQUIRE(!context->hw_device_ctx && !context->hw_frames_ctx);
		++checked;
	}
	TEST_REQUIRE(checked > 0);
	return 0;
}

int test_invalid_hardware_context() {
	const auto* codec = avcodec_find_encoder(AV_CODEC_ID_RAWVIDEO);
	TEST_REQUIRE(codec);
	const auto freeContext = [](AVCodecContext* context) { avcodec_free_context(&context); };
	std::unique_ptr<AVCodecContext, decltype(freeContext)> context(avcodec_alloc_context3(codec), freeContext);
	TEST_REQUIRE(context);
	context->width = 64;
	context->height = 64;
	context->pix_fmt = AV_PIX_FMT_VAAPI;
	TestFrame source;
	TestFrame destination;
	source.Format(AV_PIX_FMT_VAAPI);
	source.Width(64);
	source.Height(64);
	source.Pts(91);
	std::string error;
	TEST_REQUIRE(!Input::Configure(context.get(), codec, source.Raw(), error));
	TEST_REQUIRE(error == "hardware source has no valid software representation or frames context");
	TEST_REQUIRE(!Input::Frames(source.Raw()) && !Input::Download(source.Raw(), destination.Raw()));
	source.Raw()->hw_frames_ctx = av_buffer_alloc(1);
	TEST_REQUIRE(source.Raw()->hw_frames_ctx);
	TEST_REQUIRE(!Input::Configure(context.get(), codec, source.Raw(), error));
	TEST_REQUIRE(error == "hardware source has no valid software representation or frames context");
	TEST_REQUIRE(!Input::Frames(source.Raw()) && !Input::Download(source.Raw(), destination.Raw()));
	TEST_REQUIRE(!Input::Upload(context.get(), source.Raw(), destination.Raw()));
	TEST_REQUIRE(source.Pts() == 91 && source.Format() == AV_PIX_FMT_VAAPI);
	TEST_REQUIRE(!context->hw_device_ctx && !context->hw_frames_ctx);
	TEST_REQUIRE(!Input::Configure(nullptr, codec, nullptr, error));
	return 0;
}

/**
 * @brief Ensures malformed input, allocation and generic initialization failures remain failures.
 * @return Zero when the hardware-unavailable classification stays narrowly scoped.
 */
int test_hardware_capability_classifier() {
	constexpr std::array devices{AV_HWDEVICE_TYPE_CUDA, AV_HWDEVICE_TYPE_QSV, AV_HWDEVICE_TYPE_VAAPI};
	constexpr std::array failures{AVERROR(EINVAL), AVERROR(ENOMEM), AVERROR(EIO), AVERROR_UNKNOWN,
		AVERROR_INVALIDDATA, AVERROR_EXTERNAL, AVERROR_BUG};
	for (const auto device : devices) {
		for (const int status : failures) {
			TEST_REQUIRE(!HardwareCapabilityUnavailable(device, status, true));
			TEST_REQUIRE(!HardwareCapabilityUnavailable(device, status, false));
		}
		TEST_REQUIRE(!HardwareCapabilityUnavailable(device, 0, true));
		TEST_REQUIRE(!HardwareCapabilityUnavailable(device, AVERROR(EINVAL), false, true));
		TEST_REQUIRE(!HardwareCapabilityUnavailable(device, AVERROR(ENOMEM), false, true));
		TEST_REQUIRE(HardwareCapabilityUnavailable(device, AVERROR(ENODEV), false));
		TEST_REQUIRE(HardwareCapabilityUnavailable(device, AVERROR(ENOENT), false));
		TEST_REQUIRE(!HardwareCapabilityUnavailable(device, AVERROR(ENODEV), true));
	}
	TEST_REQUIRE(HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_QSV, AVERROR_EXTERNAL, false, true));
	TEST_REQUIRE(HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_QSV, AVERROR(ENOSYS), true));
	TEST_REQUIRE(HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_VAAPI, AVERROR(ENOSYS), true));
	TEST_REQUIRE(!HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_CUDA, AVERROR(ENOSYS), true));
	TEST_REQUIRE(!HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_CUDA, AVERROR_UNKNOWN, false, true));
	TEST_REQUIRE(!HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_VAAPI, AVERROR_EXTERNAL, false, true));
	TEST_REQUIRE(!HardwareCapabilityUnavailable(AV_HWDEVICE_TYPE_NONE, AVERROR(ENOSYS), true));
	TEST_REQUIRE(CheckHardwareEncoderCapability(nullptr, 1920, 1080) == 1);
	TEST_REQUIRE(CheckHardwareEncoderCapability(avcodec_find_encoder_by_name("hevc_qsv"), 0, 1080) == 1);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_NV12) == AV_PROFILE_HEVC_MAIN);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_YUV420P) == AV_PROFILE_HEVC_MAIN);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_P010LE) == AV_PROFILE_HEVC_MAIN_10);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_YUV420P10LE) == AV_PROFILE_HEVC_MAIN_10);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_NONE) == AV_PROFILE_UNKNOWN);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_VAAPI) == AV_PROFILE_UNKNOWN);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_YUV420P12LE) == AV_PROFILE_UNKNOWN);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_YUV422P) == AV_PROFILE_UNKNOWN);
	TEST_REQUIRE(HardwareHevcProfile(AV_PIX_FMT_RGB24) == AV_PROFILE_UNKNOWN);
	return 0;
}

/**
 * @brief Probes the concrete HEVC NVENC implementation with exact 10-bit input.
 * @return Zero when usable, TEST_SKIP when unavailable, one on an unexpected failure.
 */
int test_hardware_hevc_nvenc_capability() {
	const auto* codec = avcodec_find_encoder_by_name("hevc_nvenc");
	return codec ? CheckHardwareEncoderCapability(codec, 1920, 1080) : TEST_SKIP;
}

/**
 * @brief Probes the concrete HEVC QSV implementation with exact 10-bit input.
 * @return Zero when usable, TEST_SKIP when unavailable, one on an unexpected failure.
 */
int test_hardware_hevc_qsv_capability() {
	const auto* codec = avcodec_find_encoder_by_name("hevc_qsv");
	return codec ? CheckHardwareEncoderCapability(codec, 1920, 1080) : TEST_SKIP;
}

/**
 * @brief Probes the concrete HEVC VAAPI implementation with exact 10-bit input.
 * @return Zero when usable, TEST_SKIP when unavailable, one on an unexpected failure.
 */
int test_hardware_hevc_vaapi_capability() {
	const auto* codec = avcodec_find_encoder_by_name("hevc_vaapi");
	return codec ? CheckHardwareEncoderCapability(codec, 1920, 1080) : TEST_SKIP;
}

int test_hardware_transfer() {
	void* state = nullptr;
	std::set<AVHWDeviceType> checked;
	int exercised = 0;
	while (const auto* codec = av_codec_iterate(&state)) {
		if (!av_codec_is_encoder(codec) || codec->type != AVMEDIA_TYPE_VIDEO)
			continue;
		const char* negotiationError = nullptr;
		const int format = Input::Negotiate(codec, AV_PIX_FMT_YUV420P10LE, negotiationError);
		if (negotiationError || !Input::Hardware(format))
			continue;
		const AVCodecHWConfig* configuration = nullptr;
		for (int index = 0; const auto* candidate = avcodec_get_hw_config(codec, index); ++index) {
			if (candidate->pix_fmt == format && (candidate->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
				&& candidate->device_type != AV_HWDEVICE_TYPE_NONE) {
				configuration = candidate;
				break;
			}
		}
		TEST_REQUIRE(configuration);
		if (!checked.insert(configuration->device_type).second)
			continue;
		if (configuration->device_type == AV_HWDEVICE_TYPE_QSV || configuration->device_type == AV_HWDEVICE_TYPE_VAAPI) {
			const int capability = CheckHardwareEncoderCapability(codec, 64, 64);
			if (capability == TEST_SKIP)
				continue;
			TEST_REQUIRE(capability == 0);
		}
		const auto freeContext = [](AVCodecContext* context) { avcodec_free_context(&context); };
		std::unique_ptr<AVCodecContext, decltype(freeContext)> context(avcodec_alloc_context3(codec), freeContext);
		TEST_REQUIRE(context);
		context->width = 64;
		context->height = 64;
		context->pix_fmt = AV_PIX_FMT_YUV420P10LE;
		AVBufferRef* device = nullptr;
		const int available = av_hwdevice_ctx_create(&device, configuration->device_type, nullptr, nullptr, 0);
		bool constraintsKnown = false;
		bool exact = false;
		if (available >= 0 && device) {
			auto* constraints = av_hwdevice_get_hwframe_constraints(device, nullptr);
			if (constraints) {
				constraintsKnown = true;
				for (int index = 0; constraints->valid_sw_formats && constraints->valid_sw_formats[index] != AV_PIX_FMT_NONE; ++index)
					if (constraints->valid_sw_formats[index] == AV_PIX_FMT_YUV420P10LE
						|| constraints->valid_sw_formats[index] == AV_PIX_FMT_P010LE)
						exact = true;
				av_hwframe_constraints_free(&constraints);
			}
		}
		av_buffer_unref(&device);
		std::string error;
		const bool configured = Input::Configure(context.get(), codec, nullptr, error);
		const char* name = av_hwdevice_get_type_name(configuration->device_type);
		if (available < 0) {
			TEST_REQUIRE(HardwareCapabilityUnavailable(configuration->device_type, available, false));
			TEST_REQUIRE(!configured && error.starts_with("hardware device unavailable ("));
			TEST_REQUIRE(!context->hw_device_ctx && !context->hw_frames_ctx);
			std::cout << "[Hardware] " << name << ": verified unavailable-device error\n";
			continue;
		}
		if (!constraintsKnown || !exact) {
			TEST_REQUIRE(!configured);
			TEST_REQUIRE(error == (!constraintsKnown ? "hardware device cannot validate exact frame constraints"
				: "hardware device has no sample-exact input format or compatible frame dimensions"));
			std::cout << "[Hardware] " << name << ": verified unsupported exact-representation error\n";
			++exercised;
			continue;
		}
		TEST_REQUIRE(configured && error.empty());
		TEST_REQUIRE(context->hw_device_ctx && context->hw_frames_ctx && context->pix_fmt == format);
		TestFrame source;
		TestFrame upload;
		TestFrame hardware;
		TestFrame downloaded;
		TestFrame roundtrip;
		TEST_REQUIRE(source.AllocVideo(64, 64, AV_PIX_FMT_YUV420P10LE) && Samples(source, true) && Metadata(source));
		TEST_REQUIRE(Input::Prepare(source, context->sw_pix_fmt, upload));
		TEST_REQUIRE(Input::Upload(context.get(), upload.Raw(), hardware.Raw()));
		TEST_REQUIRE(Input::Frames(hardware.Raw()) && SameMetadata(source, hardware));
		TEST_REQUIRE(Input::Download(hardware.Raw(), downloaded.Raw()));
		TEST_REQUIRE(Input::Prepare(downloaded, source.Format(), roundtrip));
		TEST_REQUIRE(Samples(roundtrip, false) && SameMetadata(source, roundtrip));
		TEST_REQUIRE(Samples(source, false) && source.Pts() == 91 && source.Format() == AV_PIX_FMT_YUV420P10LE);
		std::unique_ptr<AVCodecContext, decltype(freeContext)> reused(avcodec_alloc_context3(codec), freeContext);
		TEST_REQUIRE(reused);
		reused->width = 64;
		reused->height = 64;
		TEST_REQUIRE(Input::Configure(reused.get(), codec, hardware.Raw(), error));
		TEST_REQUIRE(reused->hw_frames_ctx->data == context->hw_frames_ctx->data);
		context.reset();
		TEST_REQUIRE(Input::Upload(reused.get(), upload.Raw(), hardware.Raw()));
		TEST_REQUIRE(Input::Download(hardware.Raw(), downloaded.Raw()) && SameMetadata(source, downloaded));
		std::cout << "[Hardware] " << name << ": exact upload/download and pool lifetime verified\n";
		++exercised;
	}
	if (checked.empty())
		std::cout << "[Hardware] FFmpeg advertises no hardware-only generic input for this source\n";
	return exercised > 0 ? 0 : TEST_SKIP;
}

int test_format_negotiation() {
	constexpr std::array planar{AV_PIX_FMT_P010LE, AV_PIX_FMT_YUV420P10LE};
	constexpr std::array packed{AV_PIX_FMT_P010LE};
	constexpr std::array loss{AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P10LE, AV_PIX_FMT_YUV444P10LE,
		AV_PIX_FMT_YUV420P12LE, AV_PIX_FMT_GRAY10LE, AV_PIX_FMT_RGB48LE, AV_PIX_FMT_P010BE, AV_PIX_FMT_VAAPI};
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P10LE, planar) == AV_PIX_FMT_YUV420P10LE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P10LE, packed) == AV_PIX_FMT_P010LE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_P010LE, std::span{planar}.subspan(1)) == AV_PIX_FMT_YUV420P10LE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P10LE, loss) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P12LE, packed) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV422P10LE, packed) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P10BE, packed) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_VAAPI, {}) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_NONE, {}) == AV_PIX_FMT_NONE);
	TEST_REQUIRE(Input::Select(AV_PIX_FMT_YUV420P10LE, {}) == AV_PIX_FMT_YUV420P10LE);
	const char* error = nullptr;
	TEST_REQUIRE(Input::Negotiate(nullptr, AV_PIX_FMT_YUV420P10LE, error) == AV_PIX_FMT_NONE && error);
	return 0;
}

int test_invalid_input() {
	TestFrame source;
	FFmpeg::AVFrame destination;
	TEST_REQUIRE(source.AllocVideo(5, 3, AV_PIX_FMT_YUV420P10LE, 64));
	TEST_REQUIRE(Samples(source, true));
	TEST_REQUIRE(!Input::Prepare(source, AV_PIX_FMT_YUV420P, destination));
	TEST_REQUIRE(!Input::Prepare(source, AV_PIX_FMT_YUV422P10LE, destination));
	TEST_REQUIRE(!Input::Prepare(source, AV_PIX_FMT_P010BE, destination));
	TEST_REQUIRE(!Input::Prepare(source, source.Format(), source));
	source.Data(0)[0] = 0;
	source.Data(0)[1] = 4;
	TEST_REQUIRE(!Input::Prepare(source, AV_PIX_FMT_P010LE, destination));
	TEST_REQUIRE(source.Data(0)[1] == 4);
	return 0;
}

int test_rawvideo_retry() {
	const auto* codec = avcodec_find_encoder(AV_CODEC_ID_RAWVIDEO);
	TEST_REQUIRE(codec);
	const auto freeContext = [](AVCodecContext* context) { avcodec_free_context(&context); };
	std::unique_ptr<AVCodecContext, decltype(freeContext)> context(avcodec_alloc_context3(codec), freeContext);
	TEST_REQUIRE(context);
	context->width = 5;
	context->height = 3;
	context->pix_fmt = AV_PIX_FMT_P010LE;
	context->time_base = {1, 1000};
	context->thread_count = 1;
	TEST_REQUIRE(avcodec_open2(context.get(), codec, nullptr) == 0);
	TestFrame source;
	TestFrame prepared;
	TEST_REQUIRE(source.AllocVideo(5, 3, AV_PIX_FMT_YUV420P10LE, 64));
	TEST_REQUIRE(Samples(source, true));
	source.Pts(91);
	source.DurationTicks(7);
	TEST_REQUIRE(Input::Prepare(source, context->pix_fmt, prepared));
	StormByte::Safe::Binary expected;
	prepared.CopyPrimaryBuffer(expected);
	TEST_REQUIRE(!expected.empty());
	const auto* header = prepared.Raw();
	const auto* pixels = prepared.Data(0);
	int result = 0;
	for (int attempt = 0; attempt < 8 && result == 0; ++attempt)
		result = avcodec_send_frame(context.get(), prepared.Raw());
	TEST_REQUIRE(result == AVERROR(EAGAIN));
	TEST_REQUIRE(prepared.Raw() == header && prepared.Data(0) == pixels && prepared.Pts() == 91);
	TEST_REQUIRE(Samples(prepared, false) && Samples(source, false));
	const auto freePacket = [](AVPacket* packet) { av_packet_free(&packet); };
	std::unique_ptr<AVPacket, decltype(freePacket)> packet(av_packet_alloc(), freePacket);
	TEST_REQUIRE(packet);
	for (int attempt = 0; attempt < 8 && result == AVERROR(EAGAIN); ++attempt) {
		TEST_REQUIRE(avcodec_receive_packet(context.get(), packet.get()) == 0);
		TEST_REQUIRE(packet->size == static_cast<int>(expected.size()));
		TEST_REQUIRE(std::memcmp(packet->data, expected.data(), static_cast<std::size_t>(packet->size)) == 0);
		result = avcodec_send_frame(context.get(), prepared.Raw());
		TEST_REQUIRE(result == 0 || result == AVERROR(EAGAIN));
		TEST_REQUIRE(prepared.Raw() == header && prepared.Data(0) == pixels && prepared.Pts() == 91);
	}
	TEST_REQUIRE(result == 0);
	TEST_REQUIRE(prepared.Raw() == header && prepared.Data(0) == pixels && prepared.Pts() == 91);
	TEST_REQUIRE(source.Format() == AV_PIX_FMT_YUV420P10LE && source.Pts() == 91 && source.DurationTicks() == 7);
	TEST_REQUIRE(Samples(source, false));
	return 0;
}

int test_roundtrip() {
	for (const auto format : {AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_P010LE}) {
		for (const bool negative : {false, true}) {
			TestFrame source;
			TEST_REQUIRE(source.AllocVideo(5, 3, format, 64));
			TEST_REQUIRE(source.Linesize(0) > source.Width() * 2);
			if (negative)
				source.NegativeStride();
			TEST_REQUIRE(Samples(source, true));
			TEST_REQUIRE(Metadata(source));
			const auto* sourcePixels = source.Data(0);
			const int sourceStride = source.Linesize(0);
			FFmpeg::AVFrame converted;
			FFmpeg::AVFrame roundtrip;
			const int other = format == AV_PIX_FMT_YUV420P10LE ? AV_PIX_FMT_P010LE : AV_PIX_FMT_YUV420P10LE;
			TEST_REQUIRE(Input::Prepare(source, other, converted));
			TEST_REQUIRE(converted.Format() == other && converted.Data(0) != sourcePixels);
			TEST_REQUIRE(Samples(converted, false));
			TEST_REQUIRE(SameMetadata(source, converted));
			TEST_REQUIRE(Input::Prepare(converted, format, roundtrip));
			TEST_REQUIRE(Samples(roundtrip, false));
			TEST_REQUIRE(SameMetadata(source, roundtrip));
			TEST_REQUIRE(source.Data(0) == sourcePixels && source.Linesize(0) == sourceStride);
			TEST_REQUIRE(source.Format() == format && Samples(source, false));
			converted.Pts(100);
			converted.DurationTicks(2);
			converted.ColorRange(AVCOL_RANGE_MPEG);
			converted.RemoveSideData(AV_FRAME_DATA_DOVI_METADATA);
			TEST_REQUIRE(source.Pts() == 91 && source.DurationTicks() == 7 && source.ColorRange() == AVCOL_RANGE_JPEG);
			TEST_REQUIRE(source.SideData(AV_FRAME_DATA_DOVI_METADATA));
		}
	}
	return 0;
}

int test_supported_input_shares_pixels() {
	TestFrame source;
	FFmpeg::AVFrame prepared;
	TEST_REQUIRE(source.AllocVideo(5, 3, AV_PIX_FMT_YUV420P10LE, 64));
	TEST_REQUIRE(Samples(source, true) && Metadata(source));
	TEST_REQUIRE(Input::Prepare(source, source.Format(), prepared));
	TEST_REQUIRE(prepared.Data(0) == source.Data(0) && prepared.Data(1) == source.Data(1));
	TEST_REQUIRE(SameMetadata(source, prepared));
	prepared.Pts(300);
	prepared.TimeBase({1, 24});
	prepared.RemoveSideData(AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
	TEST_REQUIRE(source.Pts() == 91 && source.TimeBase() == FFmpeg::AVRational(1, 1000));
	TEST_REQUIRE(source.SideData(AV_FRAME_DATA_CONTENT_LIGHT_LEVEL));
	TEST_REQUIRE(Samples(source, false));
	return 0;
}

int test_wrapper_attachments() {
	using StormByte::Multimedia::Pipeline::SideData;
	using StormByte::Multimedia::Pipeline::SideDataKind;
	StormByte::Safe::Vector<SideData> attachments;
	const auto add = [&](SideDataKind kind, const void* data, std::size_t size) {
		StormByte::Safe::Binary bytes;
		bytes.resize(StormByte::ByteSize{size});
		std::memcpy(bytes.data(), data, size);
		attachments.push_back(SideData{kind, StormByte::Buffer::FIFO{std::move(bytes)}});
	};
	AVMasteringDisplayMetadata mastering{};
	mastering.has_luminance = 1;
	mastering.min_luminance = {1, 10000};
	mastering.max_luminance = {1000, 1};
	AVContentLightMetadata light{1000, 400};
	constexpr std::array<std::uint8_t, 4> payload{0, 1, 2, 255};
	add(SideDataKind::MasteringDisplay, &mastering, sizeof(mastering));
	add(SideDataKind::ContentLight, &light, sizeof(light));
	add(SideDataKind::DolbyVisionRpu, payload.data(), payload.size());
	add(SideDataKind::A53CC, payload.data(), payload.size());
	TestFrame source;
	FFmpeg::AVFrame prepared;
	FFmpeg::AVFrame restored;
	TEST_REQUIRE(source.AllocVideo(5, 3, AV_PIX_FMT_YUV420P10LE, 64) && Samples(source, true));
	TEST_REQUIRE(Input::Prepare(source, AV_PIX_FMT_P010LE, prepared));
	TEST_REQUIRE(Input::Attach(attachments, prepared));
	TEST_REQUIRE(Input::Prepare(prepared, source.Format(), restored));
	constexpr std::array types{AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL,
		AV_FRAME_DATA_DOVI_RPU_BUFFER, AV_FRAME_DATA_A53_CC};
	for (std::size_t index = 0; index < types.size(); ++index) {
		StormByte::Safe::Binary bytes;
		const auto size = attachments[index].Payload().Available();
		TEST_REQUIRE(attachments[index].Payload().Peek(size, bytes));
		const auto* side = restored.SideData(types[index]);
		TEST_REQUIRE(side && side->size == static_cast<std::size_t>(size));
		TEST_REQUIRE(std::memcmp(side->data, bytes.data(), side->size) == 0);
		TEST_REQUIRE(attachments[index].Payload().Available() == size);
		TEST_REQUIRE(!source.SideData(types[index]));
	}
	TEST_REQUIRE(Samples(source, false) && Samples(restored, false));
	attachments.clear();
	add(SideDataKind::ContentLight, payload.data(), 1);
	TEST_REQUIRE(!Input::Attach(attachments, source));
	TEST_REQUIRE(!source.SideData(AV_FRAME_DATA_CONTENT_LIGHT_LEVEL));
	TEST_REQUIRE(Input::Attach(attachments, prepared));
	TEST_REQUIRE(std::memcmp(prepared.SideData(AV_FRAME_DATA_CONTENT_LIGHT_LEVEL)->data, &light, sizeof(light)) == 0);
	return 0;
}

int main(int argc, char** argv) {
	// -------------------
	// Input
	// -------------------
	constexpr TestEntry tests[] = {
		{"test_encoder_configs", test_encoder_configs},
		{"test_hardware_capability_classifier", test_hardware_capability_classifier},
		{"test_hardware_hevc_nvenc_capability", test_hardware_hevc_nvenc_capability},
		{"test_hardware_hevc_qsv_capability", test_hardware_hevc_qsv_capability},
		{"test_hardware_hevc_vaapi_capability", test_hardware_hevc_vaapi_capability},
		{"test_hardware_plan_selection", test_hardware_plan_selection},
		{"test_cpu_context_preference", test_cpu_context_preference},
		{"test_invalid_hardware_context", test_invalid_hardware_context},
		{"test_hardware_transfer", test_hardware_transfer},
		{"test_format_negotiation", test_format_negotiation},
		{"test_invalid_input", test_invalid_input},
		{"test_rawvideo_retry", test_rawvideo_retry},
		{"test_roundtrip", test_roundtrip},
		{"test_supported_input_shares_pixels", test_supported_input_shares_pixels},
		{"test_wrapper_attachments", test_wrapper_attachments}
	};
	return RunSelectedTest(argc, argv, tests);
}