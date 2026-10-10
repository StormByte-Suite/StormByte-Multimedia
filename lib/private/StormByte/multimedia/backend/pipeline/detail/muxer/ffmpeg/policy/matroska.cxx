#include <StormByte/multimedia/backend/pipeline/detail/muxer/ffmpeg/policy/policy.hxx>
#include <StormByte/multimedia/pipeline/config/audio.hxx>
#include <StormByte/multimedia/pipeline/config/subtitle.hxx>
#include <StormByte/multimedia/pipeline/config/video.hxx>
#include <StormByte/multimedia/pipeline/muxer.hxx>
#include <StormByte/multimedia/pipeline/plan.hxx>
#include <StormByte/multimedia/pipeline/track.hxx>

#include <algorithm>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
}

using namespace StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg;

namespace {
	bool Encodes(const StormByte::Multimedia::Pipeline::Track& track) noexcept {
		const auto* config = track.Config();
		if (const auto* video = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Video*>(config))
			return video->Codec() != nullptr;
		if (const auto* audio = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Audio*>(config))
			return audio->Codec() != nullptr;
		if (const auto* subtitle = dynamic_cast<const StormByte::Multimedia::Pipeline::Config::Subtitle*>(config))
			return subtitle->Codec() != nullptr;
		return false;
	}

	std::int64_t InterleaveDeltaUs(const StormByte::Multimedia::Pipeline::Muxer& owner) noexcept {
		std::size_t remux = 0;
		std::size_t encodeVideo = 0;
		std::size_t encodeOther = 0;
		if (const auto& plan = owner.Plan(); plan) {
			for (const auto& held : plan->Tracks()) {
				if (!held || held->Type() == StormByte::Multimedia::Type::Attachment)
					continue;
				if (!Encodes(*held))
					++remux;
				else if (held->Type() == StormByte::Multimedia::Type::Video)
					++encodeVideo;
				else
					++encodeOther;
			}
		}
		if (encodeVideo != 0 && remux != 0)
			return 60000000;
		return std::clamp<std::int64_t>(250000 + static_cast<std::int64_t>(encodeVideo) * 500000
			+ static_cast<std::int64_t>(remux) * 100000
			+ static_cast<std::int64_t>(encodeOther) * 80000, 250000, 60000000);
	}

	class Matroska final: public Policy {
		public:
			void PrepareStream(::AVStream& stream, bool& haveDefaultVideo,
				bool& haveDefaultAudio) const noexcept override {
				MarkDefaultStream(stream, haveDefaultVideo, haveDefaultAudio);
			}

			void ConfigureHeader(const StormByte::Multimedia::Pipeline::Muxer& owner,
				::AVFormatContext& context, std::int64_t bufferedSpanUs, ::AVDictionary** options) const noexcept override {
				av_dict_set(&context.metadata, "encoding_tool", "StormByte-Multimedia " STORMBYTE_MULTIMEDIA_VERSION, 0);
				const auto capacity = owner.InputCeiling();
				const auto depth = capacity == 0 ? 1 : capacity;
				context.max_interleave_delta = std::max({InterleaveDeltaUs(owner),
					40000 * static_cast<std::int64_t>(depth), bufferedSpanUs});
				av_dict_set(options, "default_mode", "passthrough", 0);
			}
	};
}

const Policy& StormByte::Multimedia::Backend::Pipeline::Detail::Muxer::FFmpeg::MatroskaPolicy() noexcept {
	static const Matroska policy;
	return policy;
}
