include(helpers.cmake)

buildmaster_group(ff-plugins "FFmpeg plugins")

set(_FFMPEG_PLUGINS_OPTS "REQUIRE_TOOL=pkgconfig")

if(CMAKE_INTERPROCEDURAL_OPTIMIZATION
		OR (CMAKE_BUILD_TYPE STREQUAL "Release"
			AND CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE))
	string(APPEND _FFMPEG_PLUGINS_OPTS ";IPO=fat")
endif()

buildmaster_meta(
	ffmpeg-plugins
	"FFmpeg plugins"
	"${_FFMPEG_PLUGINS_OPTS}"
)
buildmaster_group_add(ff-plugins ffmpeg-plugins)

set(FFMPEG_PLUGIN_OPTIONS "")

add_subdirectory(codecs)
add_subdirectory(compressors)
add_subdirectory(containers)
add_subdirectory(deps)
add_subdirectory(filters)

set(FFMPEG_PLUGIN_OPTIONS "${FFMPEG_PLUGIN_OPTIONS}" PARENT_SCOPE)
