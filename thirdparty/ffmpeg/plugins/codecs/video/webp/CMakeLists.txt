set(WEBP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(WEBP_COMPONENT "libwebp")
set(WEBP_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-cmake-pkgconfig-lwebp-not-llibwebp.patch"
)

set(WEBP_OPTIONS
	"-DWEBP_BUILD_ANIM_UTILS=OFF"
	"-DWEBP_BUILD_CWEBP=OFF"
	"-DWEBP_BUILD_DWEBP=OFF"
	"-DWEBP_BUILD_GIF2WEBP=OFF"
	"-DWEBP_BUILD_IMG2WEBP=OFF"
	"-DWEBP_BUILD_VWEBP=OFF"
	"-DWEBP_BUILD_WEBPINFO=OFF"
	"-DWEBP_BUILD_WEBPMUX=OFF"
	"-DWEBP_BUILD_EXTRAS=OFF"
	"-DWEBP_BUILD_TESTS=OFF"
	"-DBUILD_SHARED_LIBS=OFF"
)
if(FFMPEG_BUNDLED_TYPE STREQUAL "shared")
	list(APPEND WEBP_OPTIONS "-DBUILD_SHARED_LIBS=ON")
endif()

buildmaster_component(
	webp
	"${WEBP_COMPONENT}"
	"${WEBP_SRC_DIR}"
	"${WEBP_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"webpdemux;webpmux;webp;sharpyuv"
	"GIT={RESET;PATCH={${WEBP_PATCHES}};TITLE=libwebp pkg-config}"
)
buildmaster_group_add(ff-video webp)
register_plugin(webp "libwebp=enabled")
