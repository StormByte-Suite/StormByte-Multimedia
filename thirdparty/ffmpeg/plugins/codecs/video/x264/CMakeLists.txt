plugin_require(WITH_GPL "libx264=disabled libx264_encoder=disabled libx264rgb_encoder=disabled")

set(X264_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(X264_OPTIONS
	bashcompletion=false
	cli=false
	opencl=false
	bit-depth=all
	asm=enabled
)

set(X264_OPTSTR "GIT={RESET;PATCH=${CMAKE_CURRENT_LIST_DIR}/0001-shallow-revision.patch;TITLE=x264 shallow revision}")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR APPLE)
	# GCC: x264 uses -mpreferred-stack-boundary=6 (64 bytes). LTO can apply
	# that assumption to other dependencies and omit required AVX2 stack
	# realignment, causing crashes in VMAF, libvpx and Tesseract.
	# Apple Clang: LTO folds the aarch64 CPU detect into the rest of the
	# link. The detect comes back with no flags and x264_encoder_open dies
	# before it prints the profile. That same link takes the elementary
	# HEVC teardown with it. SIMD stays on.
	string(APPEND X264_OPTSTR ";IPO=off")
endif()

buildmaster_component(
	x264
	"X264 codec"
	"${X264_SRC_DIR}"
	"${X264_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"x264"
	"${X264_OPTSTR}"
)
buildmaster_group_add(ff-video x264)
register_plugin(x264 "libx264=enabled libx264_encoder=enabled libx264rgb_encoder=enabled")
