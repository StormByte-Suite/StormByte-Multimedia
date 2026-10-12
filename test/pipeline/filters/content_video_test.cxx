#include "../helpers.hxx"

#include <StormByte/multimedia/backend/pipeline/detail/content/video.hxx>

#include <array>

extern "C" {
	#include <libavutil/frame.h>
	#include <libavutil/pixfmt.h>
}

using VideoContent = StormByte::Multimedia::Backend::Pipeline::Detail::Content::Video;

namespace {
	class NativeFrame final {
		public:
			NativeFrame(): m_frame(av_frame_alloc()) {}

			NativeFrame(const NativeFrame&) = delete;

			NativeFrame(NativeFrame&&) = delete;

			~NativeFrame() { av_frame_free(&m_frame); }

			NativeFrame& operator=(const NativeFrame&) = delete;

			NativeFrame& operator=(NativeFrame&&) = delete;

			::AVFrame* Get() const noexcept { return m_frame; }

		private:
			::AVFrame* m_frame;
	};

	constexpr std::array MetadataTypes{
		AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL,
		AV_FRAME_DATA_DYNAMIC_HDR_PLUS, AV_FRAME_DATA_DOVI_METADATA, AV_FRAME_DATA_DOVI_RPU_BUFFER
	};

	void HdrColor(::AVFrame& frame) {
		frame.width = 16;
		frame.height = 16;
		frame.format = AV_PIX_FMT_YUV420P10LE;
		frame.color_range = AVCOL_RANGE_MPEG;
		frame.colorspace = AVCOL_SPC_BT2020_NCL;
		frame.color_primaries = AVCOL_PRI_BT2020;
		frame.color_trc = AVCOL_TRC_SMPTE2084;
	}
}

int test_video_content_keeps_compatible_hdr_and_dovi() {
	NativeFrame source;
	NativeFrame replacement;
	TEST_REQUIRE(source.Get() && replacement.Get());
	HdrColor(*source.Get());
	HdrColor(*replacement.Get());
	for (const auto type : MetadataTypes)
		TEST_REQUIRE(av_frame_new_side_data(source.Get(), type, 8));
	TEST_REQUIRE(av_frame_copy_props(replacement.Get(), source.Get()) == 0);
	VideoContent content;
	content.Put(source.Get(), replacement.Get());
	for (const auto type : MetadataTypes)
		TEST_REQUIRE(av_frame_get_side_data(replacement.Get(), type));
	return 0;
}

int test_video_content_drops_hdr_and_dovi_after_tonemap() {
	NativeFrame source;
	NativeFrame replacement;
	TEST_REQUIRE(source.Get() && replacement.Get());
	HdrColor(*source.Get());
	HdrColor(*replacement.Get());
	for (const auto type : MetadataTypes)
		TEST_REQUIRE(av_frame_new_side_data(source.Get(), type, 8));
	TEST_REQUIRE(av_frame_copy_props(replacement.Get(), source.Get()) == 0);
	replacement.Get()->format = AV_PIX_FMT_YUV420P;
	replacement.Get()->colorspace = AVCOL_SPC_BT709;
	replacement.Get()->color_primaries = AVCOL_PRI_BT709;
	replacement.Get()->color_trc = AVCOL_TRC_BT709;
	VideoContent content;
	content.Put(source.Get(), replacement.Get());
	for (const auto type : MetadataTypes)
		TEST_REQUIRE(!av_frame_get_side_data(replacement.Get(), type));
	TEST_REQUIRE(replacement.Get()->colorspace == AVCOL_SPC_BT709);
	return 0;
}

int test_video_content_restores_compatible_missing_metadata() {
	NativeFrame source;
	NativeFrame replacement;
	TEST_REQUIRE(source.Get() && replacement.Get());
	HdrColor(*source.Get());
	replacement.Get()->width = 16;
	replacement.Get()->height = 16;
	replacement.Get()->format = source.Get()->format;
	for (const auto type : {AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL,
		AV_FRAME_DATA_DOVI_METADATA, AV_FRAME_DATA_DOVI_RPU_BUFFER})
		TEST_REQUIRE(av_frame_new_side_data(source.Get(), type, 8));
	VideoContent content;
	content.Put(source.Get(), replacement.Get());
	TEST_REQUIRE(replacement.Get()->colorspace == source.Get()->colorspace);
	TEST_REQUIRE(replacement.Get()->color_trc == source.Get()->color_trc);
	TEST_REQUIRE(replacement.Get()->color_primaries == source.Get()->color_primaries);
	TEST_REQUIRE(replacement.Get()->color_range == source.Get()->color_range);
	for (const auto type : {AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL,
		AV_FRAME_DATA_DOVI_METADATA, AV_FRAME_DATA_DOVI_RPU_BUFFER})
		TEST_REQUIRE(av_frame_get_side_data(replacement.Get(), type));
	return 0;
}

int test_video_content_keeps_profile5_color_encoding() {
	NativeFrame source;
	NativeFrame replacement;
	TEST_REQUIRE(source.Get() && replacement.Get());
	HdrColor(*source.Get());
	HdrColor(*replacement.Get());
	source.Get()->colorspace = AVCOL_SPC_IPT_C2;
	source.Get()->color_range = AVCOL_RANGE_JPEG;
	replacement.Get()->colorspace = AVCOL_SPC_UNSPECIFIED;
	replacement.Get()->color_range = AVCOL_RANGE_UNSPECIFIED;
	replacement.Get()->color_primaries = AVCOL_PRI_UNSPECIFIED;
	replacement.Get()->color_trc = AVCOL_TRC_UNSPECIFIED;
	TEST_REQUIRE(av_frame_new_side_data(source.Get(), AV_FRAME_DATA_DOVI_RPU_BUFFER, 8));
	TEST_REQUIRE(!VideoContent::HdrCompatible(source.Get()));
	VideoContent content;
	content.Put(source.Get(), replacement.Get());
	TEST_REQUIRE(av_frame_get_side_data(replacement.Get(), AV_FRAME_DATA_DOVI_RPU_BUFFER));
	TEST_REQUIRE(replacement.Get()->colorspace == AVCOL_SPC_IPT_C2);
	TEST_REQUIRE(replacement.Get()->color_range == AVCOL_RANGE_JPEG);
	replacement.Get()->format = AV_PIX_FMT_YUV420P;
	content.Put(source.Get(), replacement.Get());
	TEST_REQUIRE(!av_frame_get_side_data(replacement.Get(), AV_FRAME_DATA_DOVI_RPU_BUFFER));
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_video_content_keeps_compatible_hdr_and_dovi", test_video_content_keeps_compatible_hdr_and_dovi},
		TestEntry{"test_video_content_drops_hdr_and_dovi_after_tonemap", test_video_content_drops_hdr_and_dovi_after_tonemap},
		TestEntry{"test_video_content_restores_compatible_missing_metadata", test_video_content_restores_compatible_missing_metadata},
		TestEntry{"test_video_content_keeps_profile5_color_encoding", test_video_content_keeps_profile5_color_encoding},
	};
	return RunSelectedTest(argc, argv, tests);
}