#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	class Mp4 final: public Policy {
		public:
			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MarkDefaultStream(stream, haveDefaultVideo, haveDefaultAudio);
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::Mp4Policy() noexcept {
	static const Mp4 policy;
	return policy;
}
