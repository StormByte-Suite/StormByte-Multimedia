option(ENABLE_TEST "Enable Unit Tests" OFF)
if(ENABLE_TEST)
	enable_testing()
	add_library(multimedia_test_support INTERFACE)
	target_link_libraries(multimedia_test_support INTERFACE StormByte::Multimedia)
	target_compile_definitions(multimedia_test_support INTERFACE
		"STORMBYTE_TEST_FILES_DIR=\"${PROJECT_SOURCE_DIR}/test/files\""
		"STORMBYTE_TEST_OUTPUT_DIR=\"${PROJECT_BINARY_DIR}/test-output\"")
	# GCC -fprofile-generate made bm3d ~5s/frame and libaom miss the 30s wait.
	if(PGO_STAGE STREQUAL "generate")
		target_compile_definitions(multimedia_test_support INTERFACE STORMBYTE_MM_TEST_DEADLINE_SEC=360)
	endif()

	file(MAKE_DIRECTORY
		"${PROJECT_BINARY_DIR}/test-output"
		"${PROJECT_BINARY_DIR}/test-output/pipeline"
	)

	# One shared library absorbs every private translation unit and test
	# body that calls FFmpeg. On Linux, bfd rescans the six static
	# archives here and inside StormByte-Multimedia, not once per
	# executable. Runners are dllimport/dllexport (visibility default
	# elsewhere). Executables link this library and Multimedia only.
	add_library(StormByte-Multimedia-Test SHARED
		file/file_avio_test.cxx
		pipeline/helpers.cxx
		pipeline/video/video_test.cxx
		pipeline/video_input/video_input_test.cxx
		pipeline/video_codecs/video_codec_test.cxx
		pipeline/filters/content_video_test.cxx
		pipeline/hdr10plus/hdr10plus_test.cxx
		pipeline/interleave/interleave_test.cxx
		"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/backend/file_avio.cxx"
		"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/ffmpeg/video_input.cxx"
		"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/backend/pipeline/detail/content/video.cxx"
		"${PROJECT_SOURCE_DIR}/lib/tables/decoder/video.cxx"
		"${PROJECT_SOURCE_DIR}/lib/tables/encoder/video.cxx"
	)
	target_link_libraries(StormByte-Multimedia-Test PUBLIC multimedia_test_support)
	stormbyte_link_ffmpeg(StormByte-Multimedia-Test)
	target_include_directories(StormByte-Multimedia-Test
		PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}"
			"${PROJECT_SOURCE_DIR}/lib"
		SYSTEM PRIVATE
			"${PROJECT_SOURCE_DIR}/lib/private"
	)
	if(WITH_FFMPEG STREQUAL "BUNDLED")
		target_include_directories(StormByte-Multimedia-Test BEFORE PRIVATE
			"${PROJECT_SOURCE_DIR}/thirdparty/ffmpeg/src")
	endif()
	target_compile_definitions(StormByte-Multimedia-Test
		PRIVATE StormByte_Multimedia_Test_EXPORTS
		INTERFACE STORMBYTE_MULTIMEDIA_TEST_SHARED
	)
	set_target_properties(StormByte-Multimedia-Test PROPERTIES
		LINKER_LANGUAGE CXX
		POSITION_INDEPENDENT_CODE ON
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN ON
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
	)
	foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
		string(TOUPPER "${_cfg}" _cfg_u)
		set_target_properties(StormByte-Multimedia-Test PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY_${_cfg_u} "${CMAKE_BINARY_DIR}/lib"
			RUNTIME_OUTPUT_DIRECTORY_${_cfg_u} "${CMAKE_BINARY_DIR}/lib"
			ARCHIVE_OUTPUT_DIRECTORY_${_cfg_u} "${CMAKE_BINARY_DIR}/lib"
		)
	endforeach()

	function(add_ffmpeg_test_executable target runner)
		add_executable(${target} "${PROJECT_SOURCE_DIR}/test/runner_main.cxx")
		target_compile_definitions(${target} PRIVATE "STORMBYTE_TEST_RUNNER=${runner}")
		target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/test")
		target_link_libraries(${target} PRIVATE StormByte-Multimedia-Test StormByte::Multimedia)
	endfunction()

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
	add_test(NAME configuration.test_tessdata_language_selection
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		"-DWORK=${PROJECT_BINARY_DIR}/test-output/tessdata-selection"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/tessdata_selection_test.cmake")
	set_tests_properties(configuration.test_tessdata_language_selection
		PROPERTIES TIMEOUT 15 LABELS "multimedia;configuration;ocr")
	add_test(NAME configuration.test_tessdata_modes
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		"-DWORK=${PROJECT_BINARY_DIR}/test-output/tessdata-modes"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/tessdata_modes_test.cmake")
	set_tests_properties(configuration.test_tessdata_modes
		PROPERTIES TIMEOUT 30 LABELS "multimedia;configuration;ocr")
	add_test(NAME configuration.test_simd_options
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/simd_options_test.cmake")
	set_tests_properties(configuration.test_simd_options
		PROPERTIES TIMEOUT 15 LABELS "multimedia;configuration")
	add_test(NAME configuration.test_x265_shallow_version
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		"-DWORK=${PROJECT_BINARY_DIR}/test-output/x265-shallow-version"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/x265_shallow_version_test.cmake")
	set_tests_properties(configuration.test_x265_shallow_version
		PROPERTIES TIMEOUT 30 LABELS "multimedia;configuration" SKIP_REGULAR_EXPRESSION "SKIPPED:")
	add_test(NAME configuration.test_shallow_codec_versions
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		"-DWORK=${PROJECT_BINARY_DIR}/test-output/shallow-codec-versions"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/shallow_codec_versions_test.cmake")
	set_tests_properties(configuration.test_shallow_codec_versions
		PROPERTIES TIMEOUT 60 LABELS "multimedia;configuration" SKIP_REGULAR_EXPRESSION "SKIPPED:")
	add_test(NAME configuration.test_ocr_raw_options
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		"-DWORK=${PROJECT_BINARY_DIR}/test-output/ocr-raw-options"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/ocr_raw_options_test.cmake")
	set_tests_properties(configuration.test_ocr_raw_options
		PROPERTIES TIMEOUT 15 LABELS "multimedia;configuration;ocr")
endif()
