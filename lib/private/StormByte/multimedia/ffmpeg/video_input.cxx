#include <StormByte/multimedia/ffmpeg/video_input.hxx>

#include <memory>
#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

extern "C" {
	#include <libavutil/error.h>
	#include <libavutil/mem.h>
	#include <libavutil/imgutils.h>
}

namespace Input = StormByte::Multimedia::FFmpeg::VideoInput;

namespace {
	struct BufferDeleter {
		void operator()(AVBufferRef* buffer) const noexcept { av_buffer_unref(&buffer); }
	};

	using BufferOwner = std::unique_ptr<AVBufferRef, BufferDeleter>;

	bool ContainsFormat(const AVPixelFormat* formats, int format) noexcept {
		if (!formats)
			return false;
		for (int index = 0; formats[index] != AV_PIX_FMT_NONE; ++index)
			if (formats[index] == format)
				return true;
		return false;
	}

	int TransferFormat(AVBufferRef* frames, AVHWFrameTransferDirection direction, int source) noexcept {
		AVPixelFormat* formats = nullptr;
		const int result = av_hwframe_transfer_get_formats(frames, direction, &formats, 0);
		int selected = AV_PIX_FMT_NONE;
		if (result >= 0 && formats) {
			std::size_t count = 0;
			while (formats[count] != AV_PIX_FMT_NONE)
				++count;
			if (count > 0)
				selected = Input::Select(source, std::span{formats, count});
		}
		av_free(formats);
		return selected;
	}

	std::string AvError(int error) noexcept {
		char message[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(error, message, sizeof(message));
		return message;
	}

	int Offset(int extra, int shift) noexcept {
		const int unit = 1 << shift;
		return (extra / 2 / unit) * unit;
	}
}

bool Input::Align(::AVCodecContext* context, const AVCodec* codec, std::string& error) noexcept {
	if (!context || !codec || context->width <= 0 || context->height <= 0) {
		error = "invalid encoder dimensions";
		return false;
	}
	const auto format = Hardware(context->pix_fmt) ? context->sw_pix_fmt : context->pix_fmt;
	const auto* descriptor = av_pix_fmt_desc_get(format);
	if (!descriptor) {
		error = "invalid encoder pixel format";
		return false;
	}
	const std::string_view name = codec->name ? codec->name : "";
	int horizontal = 1, vertical = 1;
	if (name == "libkvazaar")
		horizontal = vertical = 8;
	else if (name == "libx264" || name == "libx265") {
		horizontal = 1 << descriptor->log2_chroma_w;
		vertical = 1 << descriptor->log2_chroma_h;
		if (name == "libx264" && (context->flags & AV_CODEC_FLAG_INTERLACED_DCT))
			vertical *= 2;
	}
	else if (name == "h263" || name == "h263p" || name == "rv20")
		horizontal = vertical = 4;
	else if (name == "wmv1" || name == "wmv2")
		horizontal = 2;
	if (context->width > std::numeric_limits<int>::max() - (horizontal - 1)
		|| context->height > std::numeric_limits<int>::max() - (vertical - 1)) {
		error = "aligned encoder dimensions overflow";
		return false;
	}
	context->width = (context->width + horizontal - 1) / horizontal * horizontal;
	context->height = (context->height + vertical - 1) / vertical * vertical;
	return true;
}

bool Input::Pad(const AVFrame& source, int width, int height, AVFrame& destination) noexcept {
	if (&source == &destination || !source || width < source.Width() || height < source.Height())
		return false;
	if (width == source.Width() && height == source.Height())
		return destination.Ref(source);
	const auto format = static_cast<AVPixelFormat>(source.Format());
	const auto* descriptor = av_pix_fmt_desc_get(format);
	if (!descriptor || Hardware(format) || !destination.AllocVideo(width, height, format, 64)
		|| !destination.CopyProps(source))
		return false;
	std::array<std::uint8_t*, 4> output{};
	std::array<const std::uint8_t*, 4> input{};
	std::array<std::ptrdiff_t, 4> strides{};
	std::array<int, 4> outputStrides{}, inputStrides{}, steps{};
	av_image_fill_max_pixsteps(steps.data(), nullptr, descriptor);
	for (int plane = 0; plane < 4; ++plane) {
		output[plane] = destination.Data(plane);
		input[plane] = source.Data(plane);
		strides[plane] = outputStrides[plane] = destination.Linesize(plane);
		inputStrides[plane] = source.Linesize(plane);
	}
	if (av_image_fill_black(output.data(), strides.data(), format,
		source.ColorRange() == AVCOL_RANGE_JPEG ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG, width, height) < 0)
		return false;
	const int left = Offset(width - source.Width(), descriptor->log2_chroma_w);
	const int top = Offset(height - source.Height(), descriptor->log2_chroma_h);
	for (int plane = 0; plane < 4; ++plane) {
		if (!output[plane])
			continue;
		const bool chroma = descriptor->nb_components > 1 && plane != descriptor->comp[0].plane
			&& (plane == descriptor->comp[1].plane || plane == descriptor->comp[2].plane);
		output[plane] += static_cast<std::ptrdiff_t>(top >> (chroma ? descriptor->log2_chroma_h : 0)) * strides[plane]
			+ (left >> (chroma ? descriptor->log2_chroma_w : 0)) * steps[plane];
	}
	av_image_copy(output.data(), outputStrides.data(), input.data(), inputStrides.data(),
		format, source.Width(), source.Height());
	return true;
}

bool Input::Crop(::AVFrame* frame, int width, int height) noexcept {
	if (!frame || width <= 0 || height <= 0 || frame->width < width || frame->height < height)
		return false;
	if (frame->width == width && frame->height == height)
		return true;
	const auto* descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
	if (!descriptor || Hardware(frame->format))
		return false;
	frame->crop_left = Offset(frame->width - width, descriptor->log2_chroma_w);
	frame->crop_top = Offset(frame->height - height, descriptor->log2_chroma_h);
	frame->crop_right = frame->width - width - frame->crop_left;
	frame->crop_bottom = frame->height - height - frame->crop_top;
	return av_frame_apply_cropping(frame, AV_FRAME_CROP_UNALIGNED) >= 0
		&& frame->width == width && frame->height == height;
}

const AVHWFramesContext* Input::Frames(const ::AVFrame* source) noexcept {
	if (!source || !Hardware(source->format) || source->width <= 0 || source->height <= 0 || !source->hw_frames_ctx
		|| !source->hw_frames_ctx->data || source->hw_frames_ctx->size < sizeof(AVHWFramesContext))
		return nullptr;
	const auto* frames = reinterpret_cast<const AVHWFramesContext*>(source->hw_frames_ctx->data);
	if (frames->format != source->format || frames->width < source->width || frames->height < source->height
		|| !frames->device_ref || !frames->device_ctx || Select(frames->sw_format, {}) == AV_PIX_FMT_NONE)
		return nullptr;
	return frames;
}

bool Input::Configure(::AVCodecContext* context, const AVCodec* codec,
	const ::AVFrame* source, std::string& error) noexcept {
	error.clear();
	if (!context || !codec || context->hw_frames_ctx || context->hw_device_ctx) {
		error = "invalid or already configured encoder input context";
		return false;
	}
	const int sourceFormat = source ? source->format : context->pix_fmt;
	const auto* sourceFrames = Frames(source);
	if (Hardware(sourceFormat) && !sourceFrames) {
		error = "hardware source has no valid software representation or frames context";
		return false;
	}
	const int software = sourceFrames ? sourceFrames->sw_format : sourceFormat;
	if (sourceFrames) {
		const void* formats = nullptr;
		int count = 0;
		if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &formats, &count) < 0
			|| count < 0 || (formats && count == 0)) {
			error = "failed to query encoder hardware pixel formats";
			return false;
		}
		for (int index = 0; const auto* candidate = avcodec_get_hw_config(codec, index); ++index) {
			if (candidate->pix_fmt != sourceFormat || candidate->device_type != sourceFrames->device_ctx->type)
				continue;
			if (SelectHardware(formats
				? std::span{static_cast<const AVPixelFormat*>(formats), static_cast<std::size_t>(count)}
				: std::span<const AVPixelFormat>{}, std::span{&candidate, std::size_t{1}}) != sourceFormat)
				continue;
			context->pix_fmt = static_cast<AVPixelFormat>(sourceFormat);
			context->sw_pix_fmt = static_cast<AVPixelFormat>(software);
			context->hw_frames_ctx = av_buffer_ref(source->hw_frames_ctx);
			context->hw_device_ctx = av_buffer_ref(sourceFrames->device_ref);
			if (!context->hw_frames_ctx || !context->hw_device_ctx) {
				error = "failed to reference source hardware context";
				return false;
			}
			return true;
		}
	}
	const char* negotiationError = nullptr;
	const int selected = Negotiate(codec, software, negotiationError);
	if (negotiationError) {
		error = negotiationError;
		return false;
	}
	const AVCodecHWConfig* configuration = nullptr;
	for (int index = 0; const auto* candidate = avcodec_get_hw_config(codec, index); ++index) {
		if (!(candidate->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
			|| candidate->device_type == AV_HWDEVICE_TYPE_NONE)
			continue;
		if (candidate->pix_fmt == selected) {
			configuration = candidate;
			break;
		}
	}
	context->pix_fmt = static_cast<AVPixelFormat>(selected);
	context->sw_pix_fmt = static_cast<AVPixelFormat>(selected);
	if (!Hardware(selected))
		return true;
	if (!configuration) {
		error = "encoder hardware format has no generic hardware frames configuration";
		return false;
	}
	AVBufferRef* rawDevice = nullptr;
	const int created = av_hwdevice_ctx_create(&rawDevice, configuration->device_type, nullptr, nullptr, 0);
	BufferOwner device{rawDevice};
	if (created < 0 || !device) {
		const char* name = av_hwdevice_get_type_name(configuration->device_type);
		error = std::string{"hardware device unavailable ("} + (name ? name : "unknown") + "): " + AvError(created);
		return false;
	}
	auto* constraints = av_hwdevice_get_hwframe_constraints(device.get(), nullptr);
	if (!constraints) {
		error = "hardware device cannot validate exact frame constraints";
		return false;
	}
	std::size_t count = 0;
	if (constraints->valid_sw_formats)
		while (constraints->valid_sw_formats[count] != AV_PIX_FMT_NONE)
			++count;
	const int transfer = SelectTransfer(software, std::span{constraints->valid_sw_formats, count});
	const bool dimensions = context->width > 0 && context->height > 0
		&& context->width >= constraints->min_width && context->height >= constraints->min_height
		&& (constraints->max_width <= 0 || context->width <= constraints->max_width)
		&& (constraints->max_height <= 0 || context->height <= constraints->max_height);
	const bool hardware = !constraints->valid_hw_formats || ContainsFormat(constraints->valid_hw_formats, selected);
	av_hwframe_constraints_free(&constraints);
	if (transfer == AV_PIX_FMT_NONE || !dimensions || !hardware) {
		error = "hardware device has no sample-exact input format or compatible frame dimensions";
		return false;
	}
	BufferOwner frames{av_hwframe_ctx_alloc(device.get())};
	if (!frames) {
		error = "failed to allocate hardware frames context";
		return false;
	}
	auto* pool = reinterpret_cast<AVHWFramesContext*>(frames->data);
	pool->format = static_cast<AVPixelFormat>(selected);
	pool->sw_format = static_cast<AVPixelFormat>(transfer);
	pool->width = context->width;
	pool->height = context->height;
	pool->initial_pool_size = 32;
	const int initialized = av_hwframe_ctx_init(frames.get());
	if (initialized < 0) {
		error = "failed to initialize hardware frames context: " + AvError(initialized);
		return false;
	}
	if (TransferFormat(frames.get(), AV_HWFRAME_TRANSFER_DIRECTION_TO, transfer) != transfer) {
		error = "hardware pool does not support the exact upload representation";
		return false;
	}
	context->sw_pix_fmt = static_cast<AVPixelFormat>(transfer);
	context->hw_device_ctx = device.release();
	context->hw_frames_ctx = frames.release();
	return true;
}

bool Input::Download(const ::AVFrame* source, ::AVFrame* destination) noexcept {
	const auto* frames = Frames(source);
	if (!frames || !destination || source == destination)
		return false;
	const int transfer = TransferFormat(source->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, frames->sw_format);
	if (transfer == AV_PIX_FMT_NONE)
		return false;
	av_frame_unref(destination);
	destination->format = transfer;
	destination->width = source->width;
	destination->height = source->height;
	return av_hwframe_transfer_data(destination, source, 0) >= 0 && av_frame_copy_props(destination, source) >= 0;
}

bool Input::Upload(const ::AVCodecContext* context, const ::AVFrame* source, ::AVFrame* destination) noexcept {
	if (!context || !context->hw_frames_ctx || !context->hw_frames_ctx->data || !source || !destination
		|| source == destination || source->width != context->width || source->height != context->height)
		return false;
	const auto* pool = reinterpret_cast<const AVHWFramesContext*>(context->hw_frames_ctx->data);
	if (pool->format != context->pix_fmt || pool->sw_format != source->format
		|| TransferFormat(context->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_TO, pool->sw_format) != pool->sw_format)
		return false;
	av_frame_unref(destination);
	if (av_hwframe_get_buffer(context->hw_frames_ctx, destination, 0) < 0)
		return false;
	destination->width = context->width;
	destination->height = context->height;
	return av_hwframe_transfer_data(destination, source, 0) >= 0 && av_frame_copy_props(destination, source) >= 0;
}