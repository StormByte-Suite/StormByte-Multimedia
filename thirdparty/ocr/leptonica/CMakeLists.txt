if(NOT OCR_BUNDLED_TYPE STREQUAL "static" AND NOT OCR_BUNDLED_TYPE STREQUAL "shared")
	set(OCR_BUNDLED_TYPE "static")
endif()

set(LEPTONICA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LEPTONICA_COMPONENT "leptonica")

# Public includes are <leptonica/...> while the checkout keeps headers in src/.
# Publish that layout, plus generated endianness.h, before any install prefix.
set(_lept_src "${CMAKE_CURRENT_LIST_DIR}/src")
set(_lept_api "${CMAKE_CURRENT_BINARY_DIR}/leptonica-api")
set(_lept_gen "${CMAKE_CURRENT_BINARY_DIR}/leptonica-gen")
file(MAKE_DIRECTORY "${_lept_api}/leptonica" "${_lept_gen}")
if(EXISTS "${_lept_src}/src/allheaders.h")
	file(TO_CMAKE_PATH "${_lept_src}/src/allheaders.h" _lept_all)
	set(_lept_wrapper "#include \"${_lept_all}\"\n")
	set(_lept_wrapper_out "${_lept_api}/leptonica/allheaders.h")
	if(EXISTS "${_lept_wrapper_out}")
		file(READ "${_lept_wrapper_out}" _lept_wrapper_old)
	else()
		set(_lept_wrapper_old "")
	endif()
	if(NOT _lept_wrapper_old STREQUAL _lept_wrapper)
		file(WRITE "${_lept_wrapper_out}" "${_lept_wrapper}")
	endif()
endif()
if(EXISTS "${_lept_src}/src/endianness.h.in")
	if(CMAKE_SCRIPT_MODE_FILE)
		set(_lept_big_endian 0)
	else()
		if(NOT COMMAND test_big_endian)
			include(TestBigEndian)
		endif()
		test_big_endian(_lept_big_endian)
	endif()
	if(_lept_big_endian)
		set(ENDIANNESS L_BIG_ENDIAN)
	else()
		set(ENDIANNESS L_LITTLE_ENDIAN)
	endif()
	set(APPLE_UNIVERSAL_BUILD "defined (__APPLE_CC__)")
	configure_file(
		"${_lept_src}/src/endianness.h.in"
		"${_lept_gen}/endianness.h"
		@ONLY
	)
	unset(ENDIANNESS)
	unset(APPLE_UNIVERSAL_BUILD)
endif()
target_include_directories(StormByte-Multimedia BEFORE PRIVATE
	"${_lept_api}"
	"${_lept_gen}"
)

set(LEPTONICA_OPTIONS
	"-DSW_BUILD=OFF"
	"-DBUILD_PROG=OFF"
	"-DENABLE_ZLIB=OFF"
	"-DENABLE_PNG=OFF"
	"-DENABLE_JPEG=OFF"
	"-DENABLE_TIFF=OFF"
	"-DENABLE_GIF=OFF"
	"-DENABLE_WEBP=OFF"
	"-DENABLE_OPENJPEG=OFF"
	"-DSTRICT_CONF=ON"
)
if(OCR_BUNDLED_TYPE STREQUAL "shared")
	list(APPEND LEPTONICA_OPTIONS "-DBUILD_SHARED_LIBS=ON")
else()
	list(APPEND LEPTONICA_OPTIONS "-DBUILD_SHARED_LIBS=OFF")
endif()

set(LEPTONICA_PATCHES
	${CMAKE_CURRENT_LIST_DIR}/0001-msvc-unversioned-output-name.patch
	${CMAKE_CURRENT_LIST_DIR}/0002-always-create-lept.pc.patch
	${CMAKE_CURRENT_LIST_DIR}/0003-config-find-dependency-image-libs.patch
)

set(LEPTONICA_EXTRA "GIT={RESET;PATCH={${LEPTONICA_PATCHES}};TITLE=Leptonica patches}")

buildmaster_component(
	leptonica
	"${LEPTONICA_COMPONENT}"
	"${LEPTONICA_SRC_DIR}"
	"${LEPTONICA_OPTIONS}"
	"${OCR_BUNDLED_TYPE}"
	"leptonica"
	"${LEPTONICA_EXTRA}"
)
buildmaster_group_add(OCR leptonica)
