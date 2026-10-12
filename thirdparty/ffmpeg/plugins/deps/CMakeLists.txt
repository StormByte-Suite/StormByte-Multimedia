buildmaster_group(ff-deps "Dependencies")
buildmaster_group_add(ff-plugins ff-deps)

if(MSVC)
	add_subdirectory(gzip)
endif()
add_subdirectory(ffnvcodec)
add_subdirectory(samplerate)

set(FFMPEG_PLUGIN_OPTIONS "${FFMPEG_PLUGIN_OPTIONS}" PARENT_SCOPE)
