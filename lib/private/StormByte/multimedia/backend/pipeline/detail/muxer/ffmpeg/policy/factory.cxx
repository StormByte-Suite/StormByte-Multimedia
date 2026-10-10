#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::SelectPolicy(
	std::string_view formatName) noexcept {
	if (formatName == "matroska")
		return MatroskaPolicy();
	if (formatName == "webm")
		return WebmPolicy();
	if (formatName == "mp4" || formatName == "mov" || formatName == "ipod"
		|| formatName == "3gp" || formatName == "3g2" || formatName == "psp" || formatName == "ismv")
		return Mp4Policy();
	return GenericPolicy();
}
