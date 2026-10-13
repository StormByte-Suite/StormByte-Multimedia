add_ffmpeg_test_executable(MultimediaPipelineVideoCodecTests RunPipelineVideoCodecTests)

add_pipeline_test(MultimediaPipelineVideoCodecTests test_nvenc_feature_contract)

foreach(test_name IN ITEMS
	test_svt_av1_encodes_matroska
	test_svt_av1_encodes_mp4
	test_aom_av1_encodes_webm
	test_dav1d_decodes_svt_av1
	test_aom_decodes_svt_av1
	test_libvpx_encodes_vp8_webm
	test_libvpx_encodes_vp9_webm
	test_openh264_encodes_h264_matroska
	test_kvazaar_encodes_hevc_matroska
	test_vaapi_encodes_hevc_main_matroska
	test_libwebp_anim_encodes_webp
	test_libwebp_decodes_animated_webp
)
	add_optional_pipeline_test(MultimediaPipelineVideoCodecTests ${test_name})
	set_tests_properties("pipeline.${test_name}" PROPERTIES TIMEOUT 90 LABELS "multimedia;pipeline;video-codecs")
endforeach()

foreach(test_name IN ITEMS
		test_nvenc_encodes_h264_mp4
		test_nvenc_encodes_h264_10bit_matroska
		test_nvenc_encodes_av1_matroska
	test_x264_encodes_h264_mp4
	test_x265_encodes_hevc_matroska
)
	add_optional_pipeline_test(MultimediaPipelineVideoCodecTests ${test_name})
	set_tests_properties("pipeline.${test_name}" PROPERTIES TIMEOUT 90 LABELS "multimedia;pipeline;video-codecs")
endforeach()
