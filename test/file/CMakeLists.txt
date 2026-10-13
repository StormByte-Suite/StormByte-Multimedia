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

add_executable(MultimediaFileAvioTests file_avio_test.cxx
	"${PROJECT_SOURCE_DIR}/lib/private/StormByte/multimedia/backend/file_avio.cxx")
target_include_directories(MultimediaFileAvioTests PRIVATE "${PROJECT_SOURCE_DIR}/lib/private")
target_link_libraries(MultimediaFileAvioTests PRIVATE multimedia_test_support)
if(TARGET ffmpeg-pack)
	target_link_libraries(MultimediaFileAvioTests PRIVATE ffmpeg-pack)
else()
	target_link_libraries(MultimediaFileAvioTests PRIVATE ffmpeg)
endif()
foreach(test_name IN ITEMS
	test_file_avio_read_retry_before_data
	test_file_avio_read_retry_between_blocks
	test_file_avio_read_error_is_not_retry
	test_file_avio_write_backpressure_and_seek
	test_file_avio_write_flush_error
	test_file_avio_write_origin_error
)
	add_test(NAME "file.${test_name}" COMMAND MultimediaFileAvioTests "${test_name}")
	set_tests_properties("file.${test_name}" PROPERTIES TIMEOUT 15 LABELS "multimedia;file;io")
endforeach()
