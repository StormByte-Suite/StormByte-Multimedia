add_executable(MultimediaPipelineOcrTests ocr_test.cxx)
target_link_libraries(MultimediaPipelineOcrTests PRIVATE multimedia_pipeline_test_support)

if(WITH_OCR STREQUAL "BUNDLED")
	add_executable(MultimediaOcrModelTests model_reader_test.cxx
		"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/ocr/engine.cxx"
		"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/ocr/exception.cxx")
	target_include_directories(MultimediaOcrModelTests PRIVATE "${PROJECT_SOURCE_DIR}/lib/private")
	target_link_libraries(MultimediaOcrModelTests PRIVATE multimedia_pipeline_test_support)
	add_pipeline_test(MultimediaOcrModelTests test_ocr_embedded_secondary_model)
	add_pipeline_test(MultimediaOcrModelTests test_ocr_missing_pinned_model)
endif()

add_optional_pipeline_test(MultimediaPipelineOcrTests test_transcoder_ocr_japanese_pgs_to_subrip)
