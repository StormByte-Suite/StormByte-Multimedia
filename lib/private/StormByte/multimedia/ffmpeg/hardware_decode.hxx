#pragma once

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/hwcontext.h>
	#include <libavutil/pixdesc.h>
}

namespace StormByte::Multimedia::FFmpeg::HardwareDecode {
	inline int Priority(AVHWDeviceType type) noexcept {
#if defined(_WIN32)
		switch (type) {
			case AV_HWDEVICE_TYPE_D3D11VA: return 0;
			case AV_HWDEVICE_TYPE_DXVA2: return 1;
			case AV_HWDEVICE_TYPE_CUDA: return 2;
			default: return 3;
		}
#elif defined(__APPLE__)
		return type == AV_HWDEVICE_TYPE_VIDEOTOOLBOX ? 0 : 3;
#else
		switch (type) {
			case AV_HWDEVICE_TYPE_CUDA: return 0;
			case AV_HWDEVICE_TYPE_VAAPI: return 1;
			case AV_HWDEVICE_TYPE_VDPAU: return 2;
			default: return 3;
		}
#endif
	}

	inline AVPixelFormat SelectFormat(AVCodecContext* context, const AVPixelFormat* formats) noexcept {
		if (!context || !formats || !context->hw_device_ctx)
			return formats ? formats[0] : AV_PIX_FMT_NONE;

		const auto* device = reinterpret_cast<const AVHWDeviceContext*>(context->hw_device_ctx->data);
		if (!device)
			return formats[0];
		for (int index = 0; const AVCodecHWConfig* config = avcodec_get_hw_config(context->codec, index); ++index) {
			if (config->device_type != device->type
				|| !(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
				continue;
			for (const AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE; ++format)
				if (*format == config->pix_fmt)
					return *format;
		}
		for (const AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE; ++format) {
			const auto* descriptor = av_pix_fmt_desc_get(*format);
			if (descriptor && !(descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
				return *format;
		}
		return formats[0];
	}

	inline AVPixelFormat GetFormat(AVCodecContext* context, const AVPixelFormat* formats) noexcept {
		return SelectFormat(context, formats);
	}

	inline bool Hardware(AVPixelFormat format) noexcept {
		const auto* descriptor = av_pix_fmt_desc_get(format);
		return descriptor && (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL);
	}

	inline bool Configure(AVCodecContext& context, const AVCodec* codec) noexcept {
		if (!codec || context.hw_device_ctx)
			return false;

		// MPEG-4 Part 2 stays in software: GMC can appear after initialization.
		if (codec->id == AV_CODEC_ID_MPEG4)
			return false;

		for (int priority = 0; priority <= 3; ++priority) {
			for (int index = 0; const AVCodecHWConfig* config = avcodec_get_hw_config(codec, index); ++index) {
				const auto type = config->device_type;
				if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
					|| type == AV_HWDEVICE_TYPE_NONE || Priority(type) != priority)
					continue;
				bool alreadyTried = false;
				for (int previous = 0; previous < index; ++previous) {
					const AVCodecHWConfig* prior = avcodec_get_hw_config(codec, previous);
					if (prior && prior->device_type == type
						&& (prior->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
						alreadyTried = true;
						break;
					}
				}
				if (alreadyTried)
					continue;
				AVBufferRef* device = nullptr;
				if (av_hwdevice_ctx_create(&device, type, nullptr, nullptr, 0) < 0)
					continue;
				context.hw_device_ctx = device;
				context.get_format = GetFormat;
				return true;
			}
		}
		return false;
	}

	inline bool Download(const ::AVFrame& source, ::AVFrame& destination) noexcept {
		if (!source.hw_frames_ctx || !Hardware(static_cast<AVPixelFormat>(source.format))
			|| av_hwframe_transfer_data(&destination, &source, 0) < 0)
			return false;
		return av_frame_copy_props(&destination, &source) >= 0;
	}
}
