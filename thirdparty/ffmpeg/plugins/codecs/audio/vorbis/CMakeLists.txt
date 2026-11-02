set(VORBIS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(VORBIS_COMPONENT "Vorbis codec")
set(VORBIS_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-cmake4-minimum-version.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-cmake-declare-libm-in-pkg-config-when-it-exists.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0003-psy-avoid-negative-bark-shift.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0004-psy-bound-noise-offset-interpolation.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0005-floor1-avoid-negative-deviation-shift.patch"
)

set(VORBIS_OPTIONS
	"-DBUILD_TESTING=OFF"
	"-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	list(APPEND VORBIS_OPTIONS "-DCMAKE_ASM_NASM_FLAGS=-DPIC -w-pp-macro-params-legacy")
endif()

buildmaster_component(
	vorbis
	"${VORBIS_COMPONENT}"
	"${VORBIS_SRC_DIR}"
	"${VORBIS_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"vorbis;vorbisenc;vorbisfile"
	"GIT={RESET;PATCH={${VORBIS_PATCHES}};TITLE=Vorbis CMake}"
)
buildmaster_link(vorbis ogg)
buildmaster_group_add(ff-audio vorbis)
register_plugin(vorbis "libvorbis=enabled libvorbisenc=enabled")
