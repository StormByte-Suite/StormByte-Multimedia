set(MP3LAME_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake-port")
set(MP3LAME_COMPONENT "MP3 encoder LAME")
set(MP3LAME_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-bitstream-use-unsigned-reservoir-shifts.patch"
)

set(MP3LAME_OPTIONS "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	list(APPEND MP3LAME_OPTIONS "-DCMAKE_ASM_NASM_FLAGS=-DPIC -w-pp-macro-params-legacy")
endif()
if(FFMPEG_BUNDLED_TYPE STREQUAL "shared")
	list(APPEND MP3LAME_OPTIONS "-DBUILD_SHARED_LIBS=ON")
else()
	# cmake/CMakeLists.txt defaults BUILD_SHARED_LIBS=ON. Without this,
	# meson resolves /usr/lib64/libmp3lame.so.0 (distro soname) and a
	# "static" ffmpeg is not static.
	list(APPEND MP3LAME_OPTIONS "-DBUILD_SHARED_LIBS=OFF")
endif()

buildmaster_component(
	mp3lame
	"${MP3LAME_COMPONENT}"
	"${MP3LAME_SRC_DIR}"
	"${MP3LAME_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"mp3lame"
	"GIT={ROOT=src;RESET;PATCH={${MP3LAME_PATCHES}};TITLE=LAME signed-shift fixes}"
)
buildmaster_group_add(ff-audio mp3lame)
register_plugin(mp3lame "libmp3lame=enabled")
