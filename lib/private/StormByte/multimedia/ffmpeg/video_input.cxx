#include <StormByte/multimedia/ffmpeg/video_input.hxx>

#include <memory>

extern "C" {
	#include <libavutil/error.h>
	#include <libavutil/mem.h>
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