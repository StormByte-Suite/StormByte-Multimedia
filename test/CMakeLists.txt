option(ENABLE_TEST "Enable Unit Tests" OFF)
if(ENABLE_TEST)
	enable_testing()
	add_library(multimedia_test_support INTERFACE)
	target_link_libraries(multimedia_test_support INTERFACE StormByte::Multimedia)
	target_compile_definitions(multimedia_test_support INTERFACE
		"STORMBYTE_TEST_FILES_DIR=\"${PROJECT_SOURCE_DIR}/test/files\""
		"STORMBYTE_TEST_OUTPUT_DIR=\"${PROJECT_BINARY_DIR}/test-output\"")

	file(MAKE_DIRECTORY
		"${PROJECT_BINARY_DIR}/test-output"
		"${PROJECT_BINARY_DIR}/test-output/pipeline"
	)

	add_subdirectory(registry)
	add_subdirectory(file)
	add_subdirectory(pipeline)
	add_test(NAME configuration.test_ffmpeg_vmaf_modes_match
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/ffmpeg_vmaf_test.cmake")
	set_tests_properties(configuration.test_ffmpeg_vmaf_modes_match
		PROPERTIES TIMEOUT 15 LABELS "multimedia;configuration")
	add_test(NAME configuration.test_bundled_checkout_includes
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/bundled_includes_test.cmake")
	set_tests_properties(configuration.test_bundled_checkout_includes
		PROPERTIES TIMEOUT 30 LABELS "multimedia;configuration")
endif()
