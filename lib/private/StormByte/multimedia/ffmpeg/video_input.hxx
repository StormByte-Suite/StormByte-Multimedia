#pragma once

#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/hwcontext.h>
	#include <libavutil/mastering_display_metadata.h>
	#include <libavutil/pixdesc.h>
}

/**
 * @namespace StormByte::Multimedia::FFmpeg::VideoInput
 * @brief Private, sample-exact CPU and hardware encoder input preparation.
 */
namespace StormByte::Multimedia::FFmpeg::VideoInput {
	/**
	 * @brief Rounds dimensions up only for verified concrete encoder restrictions.
	 */
	bool Align(::AVCodecContext* context, const AVCodec* codec, std::string& error) noexcept;
	/**
	 * @brief Centers unchanged samples on a chroma-aligned black canvas, or shares compatible input.
	 */
	bool Pad(const AVFrame& source, int width, int height, AVFrame& destination) noexcept;
	/**
	 * @brief Removes the same automatic borders from an analytics frame without copying samples.
	 */
	bool Crop(::AVFrame* frame, int width, int height) noexcept;
	/**
	 * @brief Validates the software representation and geometry of a hardware source.
	 * @param source Borrowed immutable hardware frame.
	 * @return Borrowed frames context, or nullptr when invalid.
	 */
	const AVHWFramesContext* Frames(const ::AVFrame* source) noexcept;

	/**
	 * @brief Negotiates exact CPU input or owns device and pool references in a codec context.
	 * @param context Destination codec context, before opening.
	 * @param codec Selected concrete encoder.
	 * @param source Optional first frame, including its hardware context.
	 * @param error Explicit failure reason.
	 * @return Whether input configuration succeeded.
	 */
	bool Configure(::AVCodecContext* context, const AVCodec* codec, const ::AVFrame* source, std::string& error) noexcept;

	/**
	 * @brief Downloads hardware input to a proven software representation, including properties.
	 * @param source Immutable hardware source.
	 * @param destination Separate empty CPU frame header.
	 * @return Whether exact transfer and property copying succeeded.
	 */
	bool Download(const ::AVFrame* source, ::AVFrame* destination) noexcept;

	/**
	 * @brief Uploads prepared CPU samples to the configured pool, including properties.
	 * @param context Encoder context owning the hardware pool.
	 * @param source Immutable CPU frame with the pool's software representation.
	 * @param destination Separate output frame header.
	 * @return Whether exact upload and property copying succeeded.
	 */
	bool Upload(const ::AVCodecContext* context, const ::AVFrame* source, ::AVFrame* destination) noexcept;

	/**
	 * @brief Restores wrapper metadata without consuming attachment payloads or replacing native data.
	 * @param attachments Original wrapper attachments.
	 * @param destination Private encoder frame header.
	 * @return Whether static HDR payloads have valid native sizes and were copied successfully.
	 */
	inline bool Attach(const Safe::Vector<Pipeline::SideData>& attachments, AVFrame& destination) noexcept {
		for (const auto& item : attachments) {
			int type = 0;
			std::size_t size = 0;
			if (item.Kind() == Pipeline::SideDataKind::MasteringDisplay) {
				type = AV_FRAME_DATA_MASTERING_DISPLAY_METADATA;
				size = sizeof(AVMasteringDisplayMetadata);
			}
			else if (item.Kind() == Pipeline::SideDataKind::ContentLight) {
				type = AV_FRAME_DATA_CONTENT_LIGHT_LEVEL;
				size = sizeof(AVContentLightMetadata);
			}
			else
				continue;
			if (destination.SideData(type))
				continue;
			Safe::Binary bytes;
			if (static_cast<std::size_t>(item.Payload().Available()) != size
				|| !item.Payload().Peek(item.Payload().Available(), bytes))
				return false;
			auto* data = destination.NewSideData(type, static_cast<int>(size));
			if (!data)
				return false;
			std::memcpy(data, bytes.data(), size);
		}
		destination.WriteSideData(attachments);
		return true;
	}

	/**
	 * @brief Recognizes the proven little-endian 10-bit 4:2:0 layout pair.
	 * @param source Source pixel format.
	 * @param destination Destination pixel format.
	 * @return Whether repacking preserves every component sample.
	 */
	inline bool Equivalent(int source, int destination) noexcept {
		return (source == AV_PIX_FMT_YUV420P10LE && destination == AV_PIX_FMT_P010LE)
			|| (source == AV_PIX_FMT_P010LE && destination == AV_PIX_FMT_YUV420P10LE);
	}

	/**
	 * @brief Checks whether a format is explicitly hardware-backed.
	 * @param format Pixel format to inspect.
	 * @return Whether libavutil describes a hardware surface.
	 */
	inline bool Hardware(int format) noexcept {
		const auto* descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(format));
		return descriptor && (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL);
	}

	/**
	 * @brief Honors a device's transfer preference order without changing component samples.
	 * @param source Exact source CPU representation.
	 * @param supported Device software formats in advertised preference order.
	 * @return First identical or proven equivalent format, or AV_PIX_FMT_NONE.
	 */
	inline int SelectTransfer(int source, std::span<const AVPixelFormat> supported) noexcept {
		const auto* descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(source));
		if (!descriptor || Hardware(source))
			return AV_PIX_FMT_NONE;
		for (const auto format : supported)
			if (format == source || Equivalent(source, format))
				return format;
		return AV_PIX_FMT_NONE;
	}

	/**
	 * @brief Selects only an advertised, device-backed hardware frames configuration.
	 * @param supported Encoder pixel formats; empty means unrestricted.
	 * @param configurations Hardware configurations in encoder preference order.
	 * @return Hardware format, or AV_PIX_FMT_NONE if generic upload is unsupported.
	 */
	inline int SelectHardware(std::span<const AVPixelFormat> supported,
		std::span<const AVCodecHWConfig* const> configurations) noexcept {
		for (const auto* configuration : configurations) {
			if (!configuration || !(configuration->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
				|| configuration->device_type == AV_HWDEVICE_TYPE_NONE || !Hardware(configuration->pix_fmt))
				continue;
			if (supported.empty())
				return configuration->pix_fmt;
			for (const auto format : supported)
				if (format == configuration->pix_fmt)
					return format;
		}
		return AV_PIX_FMT_NONE;
	}

	/**
	 * @brief Keeps a supported source format, otherwise chooses only a proven equivalent.
	 * @param source Source pixel format.
	 * @param supported Encoder formats; empty means unrestricted, as specified by libavcodec.
	 * @return Selected format, or AV_PIX_FMT_NONE when no CPU equivalent exists.
	 */
	inline int Select(int source, std::span<const AVPixelFormat> supported) noexcept {
		const auto* descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(source));
		if (!descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
			return AV_PIX_FMT_NONE;
		if (supported.empty())
			return source;
		for (const auto format : supported)
			if (format == source)
				return source;
		for (const auto format : supported)
			if (Equivalent(source, format))
				return format;
		return AV_PIX_FMT_NONE;
	}

	/**
	 * @brief Queries the concrete encoder, without implementation-name heuristics.
	 * @param codec Selected encoder with required features already checked.
	 * @param source Exact CPU representation of the source pixel format.
	 * @param error Failure description, or nullptr on success.
	 * @return Negotiated format, or AV_PIX_FMT_NONE on failure.
	 */
	inline int Negotiate(const AVCodec* codec, int source, const char*& error) noexcept {
		error = nullptr;
		const void* configurations = nullptr;
		int count = 0;
		if (!codec || avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT,
			0, &configurations, &count) < 0 || count < 0) {
			error = "failed to query encoder CPU pixel formats";
			return AV_PIX_FMT_NONE;
		}
		if (configurations && count == 0) {
			error = "encoder advertises no CPU pixel formats";
			return AV_PIX_FMT_NONE;
		}
		const int selected = Select(source, configurations
			? std::span{static_cast<const AVPixelFormat*>(configurations), static_cast<std::size_t>(count)}
			: std::span<const AVPixelFormat>{});
		if (selected != AV_PIX_FMT_NONE)
			return selected;
		if (Select(source, {}) != AV_PIX_FMT_NONE) {
			for (int index = 0; const auto* configuration = avcodec_get_hw_config(codec, index); ++index) {
				const int hardware = SelectHardware(configurations
					? std::span{static_cast<const AVPixelFormat*>(configurations), static_cast<std::size_t>(count)}
					: std::span<const AVPixelFormat>{}, std::span{&configuration, std::size_t{1}});
				if (hardware != AV_PIX_FMT_NONE)
					return hardware;
			}
		}
		error = "encoder has no sample-exact CPU input or supported hardware frames configuration";
		return AV_PIX_FMT_NONE;
	}

	/**
	 * @brief Reads an unaligned little-endian word independently of host endianness.
	 * @param bytes Two readable bytes.
	 * @return Word value.
	 */
	inline unsigned ReadWord(const std::uint8_t* bytes) noexcept {
		return static_cast<unsigned>(bytes[0]) | (static_cast<unsigned>(bytes[1]) << 8);
	}

	/**
	 * @brief Writes an unaligned little-endian word independently of host endianness.
	 * @param bytes Two writable bytes.
	 * @param value Word value.
	 */
	inline void WriteWord(std::uint8_t* bytes, unsigned value) noexcept {
		bytes[0] = static_cast<std::uint8_t>(value);
		bytes[1] = static_cast<std::uint8_t>(value >> 8);
	}

	/**
	 * @brief References unchanged pixels or exactly repacks the proven layout pair.
	 * @param source Untouched source, including native metadata and timing.
	 * @param format Negotiated CPU format.
	 * @param destination Separate frame owner; must not alias source.
	 * @return Whether preparation succeeded, including allocation and stride validation.
	 */
	inline bool Prepare(const AVFrame& source, int format, AVFrame& destination) noexcept {
		if (&source == &destination || !source || !destination)
			return false;
		if (source.Format() == format)
			return destination.Ref(source);
		if (!Equivalent(source.Format(), format) || source.Width() <= 0 || source.Height() <= 0
			|| source.Width() > std::numeric_limits<int>::max() / 4)
			return false;
		const bool planarSource = source.Format() == AV_PIX_FMT_YUV420P10LE;
		const int chromaWidth = source.Width() / 2 + source.Width() % 2;
		for (int plane = 0; plane < (planarSource ? 3 : 2); ++plane) {
			const std::int64_t stride = source.Linesize(plane);
			const int rowBytes = plane == 0 ? source.Width() * 2 : chromaWidth * (planarSource ? 2 : 4);
			if (!source.Data(plane) || (stride < 0 ? -stride : stride) < rowBytes)
				return false;
		}
		if (!destination.AllocVideo(source.Width(), source.Height(), format)
			|| !destination.CopyProps(source))
			return false;
		for (int component = 0; component < 3; ++component) {
			const int width = component == 0 ? source.Width() : chromaWidth;
			const int height = component == 0 ? source.Height() : source.Height() / 2 + source.Height() % 2;
			const int sourcePlane = planarSource || component == 0 ? component : 1;
			const int destinationPlane = !planarSource || component == 0 ? component : 1;
			const int sourceStep = planarSource || component == 0 ? 2 : 4;
			const int destinationStep = !planarSource || component == 0 ? 2 : 4;
			const int sourceOffset = !planarSource && component == 2 ? 2 : 0;
			const int destinationOffset = planarSource && component == 2 ? 2 : 0;
			for (int row = 0; row < height; ++row) {
				const auto* input = source.Data(sourcePlane)
					+ static_cast<std::ptrdiff_t>(row) * source.Linesize(sourcePlane) + sourceOffset;
				auto* output = destination.Data(destinationPlane)
					+ static_cast<std::ptrdiff_t>(row) * destination.Linesize(destinationPlane) + destinationOffset;
				for (int column = 0; column < width; ++column) {
					unsigned sample = ReadWord(input + column * sourceStep);
					if (planarSource && sample > 1023)
						return false;
					sample = planarSource ? sample << 6 : sample >> 6;
					WriteWord(output + column * destinationStep, sample);
				}
			}
		}
		return true;
	}
}