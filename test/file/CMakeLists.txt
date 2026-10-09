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
add_file_test(test_random_invalid_file_is_rejected)
add_file_test(test_truncated_file_is_rejected)
