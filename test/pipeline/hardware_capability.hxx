#pragma once

#include "../test_helpers.hxx"

#include <StormByte/multimedia/ffmpeg/video_input.hxx>

#include <fstream>
#include <memory>

/**
 * @brief Classifies only independently proven device absence or backend capability errors.
 * @param device Hardware backend selected from the concrete encoder configuration.
 * @param status Native device creation or encoder opening result.
 * @param prepared Whether sample-exact encoder input preparation succeeded.
 * @param absent Whether an independent device inventory proved the device absent.
 * @return Whether this preflight result denotes unavailable hardware rather than a test failure.
 */
inline bool HardwareCapabilityUnavailable(AVHWDeviceType device, int status, bool prepared, bool absent = false) noexcept {
	if (device == AV_HWDEVICE_TYPE_NONE || status >= 0 || status == AVERROR(EINVAL)
		|| status == AVERROR(ENOMEM))
		return false;
	if (!prepared)
		return (device == AV_HWDEVICE_TYPE_QSV && absent) || status == AVERROR(ENODEV)
			|| status == AVERROR(ENOENT) || status == AVERROR(ENOSYS);
	return (device == AV_HWDEVICE_TYPE_VAAPI || device == AV_HWDEVICE_TYPE_QSV)
		&& status == AVERROR(ENOSYS);
}

/**
 * @brief Checks Linux DRM render-node vendor IDs without interpreting generic FFmpeg errors.
 * @return True only when a complete readable inventory contains no Intel render device.
 */
inline bool IntelRenderDeviceAbsent() {
#ifdef __linux__
	std::error_code error;
	const std::filesystem::path root{"/sys/class/drm"};
	if (!std::filesystem::exists(root, error))
		return !error;
	std::filesystem::directory_iterator entries(root, error);
	if (error)
		return false;
	const std::filesystem::directory_iterator end;
	for (; entries != end; entries.increment(error)) {
		if (error)
			return false;
		if (!entries->path().filename().string().starts_with("renderD"))
			continue;
		std::ifstream vendor(entries->path() / "device/vendor");
		unsigned identifier = 0;
		if (!(vendor >> std::hex >> identifier) || identifier == 0x8086)
			return false;
	}
	return !error;
#else
	return false;
#endif
}

inline int HardwareHevcProfile(AVPixelFormat sourceFormat) noexcept {
	const auto* descriptor = av_pix_fmt_desc_get(sourceFormat);
	if (!descriptor || (descriptor->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_RGB))
		|| descriptor->nb_components != 3 || descriptor->log2_chroma_w != 1 || descriptor->log2_chroma_h != 1)
		return AV_PROFILE_UNKNOWN;
	const int depth = descriptor->comp[0].depth;
	for (int component = 1; component < descriptor->nb_components; ++component)
		if (descriptor->comp[component].depth != depth)
			return AV_PROFILE_UNKNOWN;
	return depth == 8 ? AV_PROFILE_HEVC_MAIN : depth == 10 ? AV_PROFILE_HEVC_MAIN_10 : AV_PROFILE_UNKNOWN;
}

/**
 * @brief Opens a bounded sample-exact encoder preflight before asynchronous test work starts.
 * @param codec Concrete advertised hardware encoder.
 * @param width Valid source width.
 * @param height Valid source height.
 * @param sourceFormat Exact software input representation; defaults to 10-bit HDR.
 * @return Zero when usable, TEST_SKIP for proven absence or unsupported capability, one otherwise.
 */
inline int CheckHardwareEncoderCapability(const AVCodec* codec, int width, int height,
	AVPixelFormat sourceFormat = AV_PIX_FMT_YUV420P10LE) {
	if (!codec || width <= 0 || height <= 0)
		return 1;
	const auto* descriptor = av_pix_fmt_desc_get(sourceFormat);
	if (!descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
		return 1;
	const int profile = HardwareHevcProfile(sourceFormat);
	if (codec->id == AV_CODEC_ID_HEVC && profile == AV_PROFILE_UNKNOWN)
		return 1;
	const int depth = descriptor->comp[0].depth;
	const AVCodecHWConfig* configuration = avcodec_get_hw_config(codec, 0);
	if (!configuration || configuration->device_type == AV_HWDEVICE_TYPE_NONE)
		return 0;
	std::cout << "[Hardware] " << codec->name << ": " << depth << "-bit " << descriptor->name
		<< " encoder preflight, profile " << profile << '\n';
	const auto deviceType = configuration->device_type;
	AVBufferRef* device = nullptr;
	const int created = av_hwdevice_ctx_create(&device, deviceType, nullptr, nullptr, 0);
	av_buffer_unref(&device);
	if (created < 0) {
		const bool absent = deviceType == AV_HWDEVICE_TYPE_QSV && IntelRenderDeviceAbsent();
		const bool unavailable = HardwareCapabilityUnavailable(deviceType, created, false, absent);
		char message[AV_ERROR_MAX_STRING_SIZE]{};
		av_strerror(created, message, sizeof(message));
		std::cout << "[Hardware] " << codec->name << ": device preflight " << created << " (" << message
			<< ")" << (absent ? "; no Intel DRM render device" : "")
			<< (unavailable ? "; SKIP\n" : "; FAIL\n");
		return unavailable ? TEST_SKIP : 1;
	}
	const auto freeContext = [](AVCodecContext* context) { avcodec_free_context(&context); };
	std::unique_ptr<AVCodecContext, decltype(freeContext)> context(avcodec_alloc_context3(codec), freeContext);
	if (!context)
		return 1;
	context->width = width;
	context->height = height;
	context->pix_fmt = sourceFormat;
	context->time_base = {1, 24};
	context->framerate = {24, 1};
	context->bit_rate = 500000;
	context->color_primaries = depth == 8 ? AVCOL_PRI_BT709 : AVCOL_PRI_BT2020;
	context->color_trc = depth == 8 ? AVCOL_TRC_BT709 : AVCOL_TRC_SMPTE2084;
	context->colorspace = depth == 8 ? AVCOL_SPC_BT709 : AVCOL_SPC_BT2020_NCL;
	context->color_range = AVCOL_RANGE_MPEG;
	if (codec->id == AV_CODEC_ID_HEVC)
		context->profile = profile;
	std::string error;
	if (!StormByte::Multimedia::FFmpeg::VideoInput::Configure(context.get(), codec, nullptr, error)) {
		std::cerr << "[Hardware] " << codec->name << ": input preflight failed: " << error << '\n';
		return 1;
	}
	const int opened = avcodec_open2(context.get(), codec, nullptr);
	if (opened >= 0) {
		std::cout << "[Hardware] " << codec->name << ": " << depth << "-bit encoder preflight available\n";
		return 0;
	}
	const bool unavailable = HardwareCapabilityUnavailable(deviceType, opened, true);
	char message[AV_ERROR_MAX_STRING_SIZE]{};
	av_strerror(opened, message, sizeof(message));
	std::cout << "[Hardware] " << codec->name << ": prepared " << depth << "-bit encoder open " << opened
		<< " (" << message << ")" << (unavailable ? "; unsupported backend encoding capability; SKIP\n" : "; FAIL\n");
	return unavailable ? TEST_SKIP : 1;
}