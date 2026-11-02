set(OPENH264_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(OPENH264_COMPONENT "openh264")

buildmaster_component(
	openh264
	"${OPENH264_COMPONENT}"
	"${OPENH264_SRC_DIR}"
	"-Dtests=disabled"
	"${FFMPEG_BUNDLED_TYPE}"
	"openh264"
	"GIT={RESET;PATCH=${CMAKE_CURRENT_LIST_DIR}/0001-golomb-avoid-empty-word-shifts.patch;TITLE=OpenH264 bitstream shifts}"
)
buildmaster_group_add(ff-video openh264)
register_plugin(openh264 "libopenh264=enabled")
