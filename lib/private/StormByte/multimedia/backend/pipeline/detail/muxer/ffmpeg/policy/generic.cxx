#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/attachment.hxx>
#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>

extern "C" {
	#include <libavformat/avformat.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

Policy::Policy() noexcept = default;

Policy::~Policy() noexcept = default;

void Policy::PrepareStream(::AVStream&, bool&, bool&) const noexcept {}

bool Policy::WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
	::AVFormatContext& context, const StormByte::Multimedia::Attachments& attachments) const noexcept {
	return Attachment::Write(owner, &context, attachments);
}

bool Policy::WriteHeaderPackets(StormByte::Multimedia::Pipeline::Muxer&, ::AVFormatContext&) const noexcept {
	return true;
}

void Policy::ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer&,
	::AVFormatContext&, std::int64_t, ::AVDictionary**) const noexcept {}

void Policy::MarkDefaultStream(::AVStream& stream, bool& haveDefaultVideo,
	bool& haveDefaultAudio) noexcept {
	if (!stream.codecpar)
		return;
	if (stream.codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !haveDefaultVideo) {
		stream.disposition |= AV_DISPOSITION_DEFAULT;
		haveDefaultVideo = true;
	}
	if (stream.codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !haveDefaultAudio) {
		stream.disposition |= AV_DISPOSITION_DEFAULT;
		haveDefaultAudio = true;
	}
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::GenericPolicy() noexcept {
	static const Policy policy;
	return policy;
}
