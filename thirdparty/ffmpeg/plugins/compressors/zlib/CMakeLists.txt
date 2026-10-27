set(ZLIB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ZLIB_COMPONENT "zlib compression library")
# zlib 1.3.2 names the Windows static archive zs[d].lib while zlib.pc keeps
# -lz, so -lz resolved to the z.dll import library. Keep one name everywhere.
set(ZLIB_PATCH_NAME
	"${CMAKE_CURRENT_LIST_DIR}/zlib-static-name-matches-pc.patch")

# zlib 1.3.2 ignores BUILD_SHARED_LIBS and builds both variants by default.
if(FFMPEG_BUNDLED_TYPE STREQUAL "shared")
	set(ZLIB_SHARED ON)
	set(ZLIB_STATIC OFF)
else()
	set(ZLIB_SHARED OFF)
	set(ZLIB_STATIC ON)
endif()

set(ZLIB_OPTIONS
	"-DZLIB_BUILD_EXAMPLES=OFF"
	"-DZLIB_BUILD_TESTING=OFF"
	"-DBUILD_SHARED_LIBS=${ZLIB_SHARED}"
	"-DZLIB_BUILD_SHARED=${ZLIB_SHARED}"
	"-DZLIB_BUILD_STATIC=${ZLIB_STATIC}"
)

buildmaster_component(
	zlib
	"${ZLIB_COMPONENT}"
	"${ZLIB_SRC_DIR}"
	"${ZLIB_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"z"
	"GIT={RESET;PATCH=${ZLIB_PATCH_NAME};TITLE=zlib static library name}"
)
buildmaster_group_add(ff-compressors zlib)
register_plugin(zlib "zlib=enabled")
