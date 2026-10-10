#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	class Webm final: public Policy {
		public:
			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MatroskaPolicy().PrepareStream(stream, haveDefaultVideo, haveDefaultAudio);
			}

			bool WriteAttachments(StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext&, const StormByte::Multimedia::Attachments& attachments) const noexcept override {
				if (attachments.empty())
					return true;
				owner.Fail("WebM output does not support file attachments");
				return false;
			}

			void ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, std::int64_t bufferedSpanUs, ::AVDictionary** options) const noexcept override {
				MatroskaPolicy().ConfigureHeader(owner, context, bufferedSpanUs, options);
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::WebmPolicy() noexcept {
	static const Webm policy;
	return policy;
}
