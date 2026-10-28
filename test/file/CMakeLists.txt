add_executable(MultimediaFileTests file_test.cxx)
target_link_libraries(MultimediaFileTests PRIVATE multimedia_test_support)

function(add_file_test test_name)
	add_test(NAME "file.${test_name}" COMMAND MultimediaFileTests "${test_name}")
	set_tests_properties("file.${test_name}" PROPERTIES TIMEOUT 50 LABELS "multimedia;file")
endfunction()

add_file_test(test_bluray_like_mkv_streams_and_cover)
add_file_test(test_anime_like_mkv_languages_and_font)
add_file_test(test_vp9_mp4_and_avi_container_snapshots)
add_file_test(test_stereo_and_51_audio_fixtures)
add_file_test(test_pgs_fixture_subtitles_are_bitmap)
add_file_test(test_mp4_fixture_packed_subtitles_are_text)
add_file_test(test_srt_fixture_subtitles_are_text)
add_file_test(test_random_invalid_file_is_rejected)
add_file_test(test_truncated_file_is_rejected)

add_executable(MultimediaDurationTests duration_test.cxx)
target_link_libraries(MultimediaDurationTests PRIVATE multimedia_pipeline_test_support)
foreach(test_name IN ITEMS
	test_duration_cancel_rewinds_and_retries
	test_unknown_size_duration_cancel_rewinds_and_retries
	test_duration_pause_and_resume
	test_duration_cancel_while_paused
	test_duration_cancel_while_running
	test_supplied_duration_skips_scan_control
	test_duration_hint_does_not_scan
	test_progress_duration_hint_skips_scan
	test_missing_container_duration_scans
)
	add_test(NAME "file.${test_name}" COMMAND MultimediaDurationTests "${test_name}")
	set_tests_properties("file.${test_name}" PROPERTIES TIMEOUT 20 LABELS "multimedia;file;pipeline")
endforeach()

add_ffmpeg_test_executable(MultimediaFileAvioTests RunFileAvioTests)
foreach(test_name IN ITEMS
	test_file_avio_read_retry_before_data
	test_file_avio_read_retry_between_blocks
	test_file_avio_read_error_is_not_retry
	test_file_avio_read_seek_and_invalid_buffer
	test_file_avio_write_backpressure_and_seek
	test_file_avio_write_flush_error
	test_file_avio_write_origin_error
)
	add_test(NAME "file.${test_name}" COMMAND MultimediaFileAvioTests "${test_name}")
	set_tests_properties("file.${test_name}" PROPERTIES TIMEOUT 15 LABELS "multimedia;file;io")
endforeach()
