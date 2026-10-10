#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>
#include <StormByte/multimedia/ffmpeg/AVFrame.hxx>
#include <StormByte/multimedia/ffmpeg/typedefs.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>

#include <climits>
#include <cstring>
#include <format>
#include <span>

extern "C" {
	#include <libavcodec/packet.h>
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	std::span<const std::byte> UnreadSpan(const StormByte::Buffer::FIFO& fifo) noexcept {
		const auto& stored = fifo.Data();
		const auto available = static_cast<std::size_t>(fifo.Available());
		if (available == 0 || available > stored.size())
			return {};
		return {stored.data() + (stored.size() - available), available};
	}

	class Mp4 final: public Policy {
		public:
			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MarkDefaultStream(stream, haveDefaultVideo, haveDefaultAudio);
			}

			bool WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) const noexcept override {
				for (const auto& attachment : attachments) {
					const auto mime = attachment.MimeType();
					const auto type = mime ? static_cast<std::string_view>(mime.value()) : std::string_view{};
					AVCodecID codec = AV_CODEC_ID_NONE;
					std::string_view hint;
					if (type == "image/png") {
						codec = AV_CODEC_ID_PNG;
						hint = ".png";
					}
					else if (type == "image/jpeg" || type == "image/jpg") {
						codec = AV_CODEC_ID_MJPEG;
						hint = ".jpg";
					}
					else if (type == "image/bmp") {
						codec = AV_CODEC_ID_BMP;
						hint = ".bmp";
					}
					if (codec == AV_CODEC_ID_NONE) {
						owner.Fail(std::format("MP4 attachment cannot be represented as cover art: {}", type));
						return false;
					}
					const auto bytes = UnreadSpan(attachment.Payload());
					if (bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE)) {
						owner.Fail("MP4 cover image payload is empty or too large");
						return false;
					}
					const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
					const auto image = StormByte::Multimedia::FFmpeg::AVFrame::DecodeImage(data, bytes.size(), hint);
					if (!image) {
						owner.Fail("MP4 cover image could not be decoded");
						return false;
					}
					AVStream* stream = avformat_new_stream(&context, nullptr);
					if (!stream || av_new_packet(&stream->attached_pic, static_cast<int>(bytes.size())) < 0) {
						owner.Fail("could not allocate MP4 cover stream or packet");
						return false;
					}
					stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
					stream->codecpar->codec_id = codec;
					stream->codecpar->width = image.Width();
					stream->codecpar->height = image.Height();
					stream->disposition = AV_DISPOSITION_ATTACHED_PIC;
					stream->time_base = ::AVRational{1, 1000};
					std::memcpy(stream->attached_pic.data, bytes.data(), bytes.size());
					stream->attached_pic.stream_index = stream->index;
					stream->attached_pic.pts = 0;
					stream->attached_pic.dts = 0;
					stream->attached_pic.flags |= AV_PKT_FLAG_KEY;
					if (const auto name = attachment.FileName(); name)
						av_dict_set(&stream->metadata, "title", name->data(), 0);
				}
				return true;
			}

			bool WriteHeaderPackets(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context) const noexcept override {
				for (unsigned index = 0; index < context.nb_streams; ++index) {
					const auto* stream = context.streams[index];
					if (stream->disposition != AV_DISPOSITION_ATTACHED_PIC)
						continue;
					AVPacket* packet = av_packet_clone(&stream->attached_pic);
					if (!packet) {
						owner.Fail("could not allocate MP4 cover output packet");
						return false;
					}
					packet->stream_index = static_cast<int>(index);
					const int result = av_interleaved_write_frame(&context, packet);
					av_packet_free(&packet);
					if (result < 0) {
						owner.Fail(std::format("MP4 cover write failed: {}",
							static_cast<std::string_view>(StormByte::Multimedia::FFmpeg::ErrorToString(result))));
						return false;
					}
				}
				return true;
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::Mp4Policy() noexcept {
	static const Mp4 policy;
	return policy;
}
