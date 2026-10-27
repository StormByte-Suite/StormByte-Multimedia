add_executable(MultimediaPipelineOcrTests ocr_test.cxx)
target_link_libraries(MultimediaPipelineOcrTests PRIVATE multimedia_pipeline_test_support)

add_executable(MultimediaOcrModelTests model_reader_test.cxx
	"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/ocr/engine.cxx"
	"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/ocr/exception.cxx")
target_include_directories(MultimediaOcrModelTests PRIVATE "${PROJECT_SOURCE_DIR}/lib/private")
target_link_libraries(MultimediaOcrModelTests PRIVATE
	multimedia_pipeline_test_support tesseract)
if(WITH_TESSDATA STREQUAL "SYSTEM")
	target_compile_definitions(MultimediaOcrModelTests PRIVATE STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
		"STORMBYTE_MULTIMEDIA_TESSDATA_DIR=\"${TESSDATA_DIR}\"")
else()
	target_link_libraries(MultimediaOcrModelTests PRIVATE tessdata)
	add_pipeline_test(MultimediaOcrModelTests test_ocr_embedded_secondary_model)
endif()
add_pipeline_test(MultimediaOcrModelTests test_ocr_missing_pinned_model)
add_pipeline_test(MultimediaOcrModelTests test_ocr_image_size_limits)

add_optional_pipeline_test(MultimediaPipelineOcrTests test_transcoder_ocr_japanese_pgs_to_subrip)
