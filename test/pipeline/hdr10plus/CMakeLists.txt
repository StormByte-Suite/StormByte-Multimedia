add_executable(MultimediaPipelineHdr10PlusTests hdr10plus_test.cxx
	"${PROJECT_SOURCE_DIR}/lib/tables/decoder/video.cxx"
	"${PROJECT_SOURCE_DIR}/lib/tables/encoder/video.cxx")
target_include_directories(MultimediaPipelineHdr10PlusTests PRIVATE "${PROJECT_SOURCE_DIR}/lib")
target_link_libraries(MultimediaPipelineHdr10PlusTests PRIVATE multimedia_pipeline_test_support)
if(TARGET ffmpeg-pack)
	target_link_libraries(MultimediaPipelineHdr10PlusTests PRIVATE ffmpeg-pack)
else()
	target_link_libraries(MultimediaPipelineHdr10PlusTests PRIVATE ffmpeg)
endif()

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