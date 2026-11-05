option(ENABLE_TEST "Enable Unit Tests" OFF)
option(ENABLE_CI_FIXTURES "Enable CI-only package discovery fixtures" OFF)
option(ENABLE_PREVIEW_DEMO "Build the optional SDL2 interactive preview demo" OFF)
if(ENABLE_PREVIEW_DEMO)
	find_package(SDL2 CONFIG REQUIRED)
	add_executable(MultimediaPreviewDemo preview_demo.cxx)
	target_link_libraries(MultimediaPreviewDemo PRIVATE StormByte::Multimedia SDL2::SDL2)
endif()

if(ENABLE_TEST)
	enable_testing()
	add_test(NAME OptimizationTests
		COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/optimizations_test.cmake")
	set_tests_properties(OptimizationTests PROPERTIES TIMEOUT 15)

	if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_executable(GccFortifyLtoTests
			toolchain/fortify_lto_test.cxx
			toolchain/fortify_lto_helper.cxx)
		# C++26 to_string uses std::format; the bug affects C++23 dependencies.
		set_target_properties(GccFortifyLtoTests PROPERTIES
			CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
		add_test(NAME GccFortifyLtoTests COMMAND GccFortifyLtoTests)
		set_tests_properties(GccFortifyLtoTests PROPERTIES TIMEOUT 15)
	endif()

	add_library(multimedia_test_support INTERFACE)
	target_link_libraries(multimedia_test_support INTERFACE StormByte::Multimedia)
	target_compile_definitions(multimedia_test_support INTERFACE
		"STORMBYTE_TEST_FILES_DIR=\"${PROJECT_SOURCE_DIR}/test/files\""
		"STORMBYTE_TEST_OUTPUT_DIR=\"${PROJECT_BINARY_DIR}/test-output\"")
	# Instrumented filters can take several seconds per frame.
	if(PGO_STAGE STREQUAL "generate" OR ENABLE_ASAN)
		target_compile_definitions(multimedia_test_support INTERFACE
			STORMBYTE_MM_TEST_DEADLINE_SEC=360
			STORMBYTE_MM_TEST_VIDEO_LATENESS_SEC=0.25)
	endif()

	file(MAKE_DIRECTORY
		"${PROJECT_BINARY_DIR}/test-output"
		"${PROJECT_BINARY_DIR}/test-output/pipeline"
	)

	add_executable(MultimediaLogTests log_test.cxx
		"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/log.cxx")
	target_include_directories(MultimediaLogTests PRIVATE "${PROJECT_SOURCE_DIR}/lib/private")
	target_link_libraries(MultimediaLogTests PRIVATE multimedia_test_support)
	add_test(NAME logging.test_independent_job_destinations COMMAND MultimediaLogTests)
	set_tests_properties(logging.test_independent_job_destinations
		PROPERTIES TIMEOUT 15 LABELS "multimedia;logging")

	add_executable(MultimediaSimplePlayerTests simple_player_test.cxx)
	target_link_libraries(MultimediaSimplePlayerTests PRIVATE multimedia_test_support)
	foreach(test_name IN ITEMS
		test_empty_display
		test_buffering_policy
		test_audio_adapter_mapping
		test_audio_adapter_samples
		test_audio_native_51
		test_audio_native_mono
		test_audio_native_opus
		test_audio_native_stereo
		test_audio_pacing_pause_drain
		test_close_wakes_idle_workers
		test_audio_paused_seek
		test_av_seek_switch_epochs
		test_hdr_preview_tone_mapping
		test_video_conversion_lifecycle
		test_hdr_preview_preserves_signal
		test_live_audio_selection
		test_live_audio_tracks
		test_skewed_audio_late
		test_skewed_video_late
		test_skewed_audio_late_auto
		test_skewed_video_late_auto
		test_live_audio_backpressure_errors
		test_av_seek_within_frame
		test_explicit_errors
		test_long_subtitle_duration
		test_opus_without_video
		test_packed_avi_seek
		test_recovers_rejected_initial_avi_mp3_packet
		test_recovers_rejected_avi_mp3_packet
		test_recovers_rejected_avi_video_packet
		test_subtitle_decode
		test_subtitle_paused_seek
		test_unknown_duration_seek
		test_faulted_reader_retained
		test_preview_to_transcoder
		test_rewind_close_failures
		test_stop_transfer_destructor)
		add_test(NAME "preview.${test_name}" COMMAND MultimediaSimplePlayerTests "${test_name}")
		set_tests_properties("preview.${test_name}" PROPERTIES TIMEOUT 20 LABELS "multimedia;preview")
		if(PGO_STAGE STREQUAL "generate" OR ENABLE_ASAN)
			set_tests_properties("preview.${test_name}" PROPERTIES TIMEOUT 480)
		endif()
	endforeach()

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
	elseif(WITH_FFMPEG STREQUAL "SYSTEM")
		target_compile_definitions(StormByte-Multimedia-Test PRIVATE STORMBYTE_MM_TEST_FFMPEG_SYSTEM)
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
	add_test(NAME configuration.test_ffmpeg_bm3d_distances
		COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
		"-DWORK=${PROJECT_BINARY_DIR}/test-output/ffmpeg-bm3d"
		"-DC_COMPILER=${CMAKE_C_COMPILER}"
		"-DGENERATOR=${CMAKE_GENERATOR}"
		"-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
		"-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/ffmpeg_bm3d_test.cmake")
	set_tests_properties(configuration.test_ffmpeg_bm3d_distances
		PROPERTIES TIMEOUT 60 LABELS "multimedia;configuration;ffmpeg"
			SKIP_REGULAR_EXPRESSION "SKIPPED:")
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
	if(WITH_FFMPEG STREQUAL "BUNDLED" AND NOT WIN32)
		add_test(NAME configuration.test_opus_arm64_neon
			COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
			"-DWORK=${PROJECT_BINARY_DIR}/test-output/opus-neon"
			"-DC_COMPILER=${CMAKE_C_COMPILER}" "-DGENERATOR=${CMAKE_GENERATOR}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/opus_arm64_neon_test.cmake")
		set_tests_properties(configuration.test_opus_arm64_neon
			PROPERTIES TIMEOUT 90 LABELS "multimedia;configuration;simd"
				SKIP_REGULAR_EXPRESSION "SKIPPED:")
	endif()
	if(WITH_FFMPEG STREQUAL "BUNDLED" AND NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
		add_test(NAME configuration.test_ffmpeg_intrinsics_probes
			COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
			"-DWORK=${PROJECT_BINARY_DIR}/test-output/ffmpeg-intrinsics"
			"-DC_COMPILER=${CMAKE_C_COMPILER}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
			"-DPROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/ffmpeg_intrinsics_test.cmake")
		set_tests_properties(configuration.test_ffmpeg_intrinsics_probes
			PROPERTIES TIMEOUT 600 LABELS "multimedia;configuration;simd;ffmpeg"
				SKIP_REGULAR_EXPRESSION "SKIPPED:")
	endif()
	if(WITH_OCR STREQUAL "BUNDLED" AND NOT WIN32)
		add_test(NAME configuration.test_tesseract_arm64_neon
			COMMAND "${CMAKE_COMMAND}" "-DREPO=${PROJECT_SOURCE_DIR}"
			"-DWORK=${PROJECT_BINARY_DIR}/test-output/tesseract-neon"
			"-DCXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DGENERATOR=${CMAKE_GENERATOR}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/configuration/tesseract_arm64_neon_test.cmake")
		set_tests_properties(configuration.test_tesseract_arm64_neon
			PROPERTIES TIMEOUT 90 LABELS "multimedia;configuration;simd"
				SKIP_REGULAR_EXPRESSION "SKIPPED:")
	endif()
	if(WITH_FFMPEG STREQUAL "BUNDLED" AND FFMPEG_BUNDLED_TYPE STREQUAL "static"
			AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
		add_executable(FFmpegArm64SwscaleTests ffmpeg/arm64_swscale_test.c)
		target_compile_features(FFmpegArm64SwscaleTests PRIVATE c_std_11)
		set_target_properties(FFmpegArm64SwscaleTests PROPERTIES LINKER_LANGUAGE CXX)
		stormbyte_link_ffmpeg(FFmpegArm64SwscaleTests)
		add_test(NAME ffmpeg.test_arm64_swscale_signed_tail COMMAND FFmpegArm64SwscaleTests)
		set_tests_properties(ffmpeg.test_arm64_swscale_signed_tail
			PROPERTIES TIMEOUT 15 LABELS "multimedia;ffmpeg;simd")
	endif()
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

	if(ENABLE_CI_FIXTURES AND BUILD_SHARED_LIBS AND WITH_STORMBYTE STREQUAL "BUNDLED" AND NOT CMAKE_CROSSCOMPILING)
		set(_package_cxx_flags "${CMAKE_CXX_FLAGS}")
		set(_package_link_flags "${CMAKE_EXE_LINKER_FLAGS}")
		if(COMMAND _pgo_strip_profile_flags)
			_pgo_strip_profile_flags("${_package_cxx_flags}" _package_cxx_flags)
			_pgo_strip_profile_flags("${_package_link_flags}" _package_link_flags)
		endif()
		add_test(NAME PackageDiscoveryTests
			COMMAND "${CMAKE_COMMAND}"
				"-DMULTIMEDIA_BUILD_DIR=${PROJECT_BINARY_DIR}"
				"-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_consumer"
				"-DDEPENDENCY_PREFIX=${BUILDMASTER_INSTALL_DIR}"
				"-DMEDIA_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/files/audio/noise_stereo.wav"
				"-DBUILD_CONFIG=$<CONFIG>"
				"-DGENERATOR=${CMAKE_GENERATOR}"
				"-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
				"-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
				"-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
				"-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
				"-DCXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}"
				"-DCXX_FLAGS=${_package_cxx_flags}"
				"-DEXE_LINKER_FLAGS=${_package_link_flags}"
				"-DOSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
				"-DOSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
				"-DEXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/package_discovery_test.cmake")
		set_tests_properties(PackageDiscoveryTests PROPERTIES TIMEOUT 180)
	endif()
endif()
