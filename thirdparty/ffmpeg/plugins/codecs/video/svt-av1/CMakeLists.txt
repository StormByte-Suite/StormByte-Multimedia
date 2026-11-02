set(SVTAV1_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SVTAV1_COMPONENT "SVT-AV1")
set(SVTAV1_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-shallow-version.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-subpel-variance-unaligned-reference.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0003-avx2-unaligned-level-and-coefficient-loads.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0004-avx2-unaligned-full-two-tap-coefficients.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0005-sse2-unaligned-four-byte-deblock-stores.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0006-avx2-unaligned-cdef-row-accesses.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0007-avx2-unaligned-four-byte-convolution-and-packing.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0008-sse2-store-second-row-without-alignment-or-overflow.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0009-avx2-load-four-byte-sad-rows-without-alignment.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0010-sse41-load-short-rows-without-scalar-alignment.patch"
)

set(SVTAV1_OPTIONS
	"-DCOMPILE_C_ONLY=OFF"
	"-DNATIVE=OFF"
	"-DBUILD_APPS=OFF"
	"-DBUILD_DEC=OFF"
	"-DBUILD_TESTING=OFF"
	"-DBUILD_SHARED_LIBS=OFF"
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
	list(APPEND SVTAV1_OPTIONS "-DENABLE_AVX512=ON")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
	list(APPEND SVTAV1_OPTIONS "-DENABLE_NEON=ON" "-DENABLE_ARM_CRC32=ON"
		"-DENABLE_NEON_DOTPROD=ON" "-DENABLE_NEON_I8MM=ON" "-DENABLE_SVE=ON" "-DENABLE_SVE2=ON")
endif()
if(FFMPEG_BUNDLED_TYPE STREQUAL "shared")
	list(APPEND SVTAV1_OPTIONS "-DBUILD_SHARED_LIBS=ON")
endif()

buildmaster_component(
	svt-av1
	"${SVTAV1_COMPONENT}"
	"${SVTAV1_SRC_DIR}"
	"${SVTAV1_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"SvtAv1Enc"
	"GIT={RESET;PATCH={${SVTAV1_PATCHES}};TITLE=SVT-AV1 compatibility patches}"
)
buildmaster_group_add(ff-video svt-av1)
register_plugin(svt-av1 "libsvtav1=enabled")
