set(OPENH264_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(OPENH264_COMPONENT "openh264")
set(OPENH264_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-golomb-avoid-empty-word-shifts.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-golomb-use-unsigned-trailing-mask.patch"
)

buildmaster_component(
	openh264
	"${OPENH264_COMPONENT}"
	"${OPENH264_SRC_DIR}"
	"-Dtests=disabled"
	"${FFMPEG_BUNDLED_TYPE}"
	"openh264"
	"GIT={RESET;PATCH={${OPENH264_PATCHES}};TITLE=OpenH264 bitstream shifts}"
)
buildmaster_group_add(ff-video openh264)
register_plugin(openh264 "libopenh264=enabled")
