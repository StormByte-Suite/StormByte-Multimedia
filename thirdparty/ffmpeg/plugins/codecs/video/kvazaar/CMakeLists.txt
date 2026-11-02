set(KVAZAAR_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(KVAZAAR_COMPONENT "kvazaar")
set(KVAZAAR_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-kvazaar-external-pthread-msvc.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-cmake-pc-libs-private-libm-pthread.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0003-cmake-pc-cflags-static-lib.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0004-cmake-pc-meson110-libs-not-private.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0005-win-pragma-comment-lib-pthread-when-KVZ_STATIC_LIB.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0006-msvc-default-thread-attributes.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0007-version-header-in-binary-dir.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0008-clang-cl-sse41-target.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0009-avx2-unaligned-eight-pixel-loads.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0010-avx2-unaligned-reconstruction-stores.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0011-avx2-unaligned-dc-reference-loads.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0012-avx2-unaligned-four-pixel-reference-loads.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0013-avx2-match-quant-strategy-signature.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0014-avx2-match-sao-edge-strategy-signature.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0015-avx2-unaligned-interpolation-row-accesses.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0016-avx2-unaligned-subpixel-block-copies.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0017-sse41-unaligned-sad-fragment-loads.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0018-avx2-limit-chroma-filter-loads-to-four-bytes.patch"
)

set(KVAZAAR_OPTIONS
	"-DBUILD_SHARED_LIBS=OFF"
	"-DBUILD_TESTS=OFF"
	"-DBUILD_KVAZAAR_BINARY=OFF"
)
if(FFMPEG_BUNDLED_TYPE STREQUAL "shared")
	list(APPEND KVAZAAR_OPTIONS "-DBUILD_SHARED_LIBS=ON")
endif()

buildmaster_component(
	kvazaar
	"${KVAZAAR_COMPONENT}"
	"${KVAZAAR_SRC_DIR}"
	"${KVAZAAR_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"kvazaar"
	"GIT={RESET;PATCH={${KVAZAAR_PATCHES}};TITLE=kvazaar CMake}"
)
if(MSVC)
	buildmaster_link(kvazaar pthread-win32)
endif()
buildmaster_group_add(ff-video kvazaar)
register_plugin(kvazaar "libkvazaar=enabled")
