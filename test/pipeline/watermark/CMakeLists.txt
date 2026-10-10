add_executable(MultimediaPipelineWatermarkTests watermark_test.cxx)
target_link_libraries(MultimediaPipelineWatermarkTests PRIVATE multimedia_pipeline_test_support)

set_source_files_properties(watermark_test.cxx PROPERTIES
	COMPILE_OPTIONS "$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-c23-extensions>"
)

add_optional_pipeline_test(MultimediaPipelineWatermarkTests test_watermark_path_and_embedded_binary)
set_tests_properties(pipeline.test_watermark_path_and_embedded_binary PROPERTIES
	TIMEOUT 90 LABELS "multimedia;pipeline;watermark")
