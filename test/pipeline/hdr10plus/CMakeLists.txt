add_ffmpeg_test_executable(MultimediaPipelineHdr10PlusTests RunPipelineHdr10PlusTests)

foreach(test_name IN ITEMS
	test_file_hdr10plus_detection
	test_hdr10plus_capabilities
	test_remux_hdr10plus)
	add_pipeline_test(MultimediaPipelineHdr10PlusTests ${test_name})
endforeach()

foreach(test_name IN ITEMS
	test_aom_hdr10plus
	test_hevc_av1_hevc_hdr10plus
	test_native_vp9_hdr10plus
	test_vp9_hdr10plus
	test_x265_hdr10plus)
	add_optional_pipeline_test(MultimediaPipelineHdr10PlusTests ${test_name})
endforeach()
