if(NOT OCR_BUNDLED_TYPE STREQUAL "static" AND NOT OCR_BUNDLED_TYPE STREQUAL "shared")
	set(OCR_BUNDLED_TYPE "static")
endif()

set(TESSERACT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TESSERACT_COMPONENT "tesseract")

set(_tess_root "${CMAKE_CURRENT_LIST_DIR}/src")
set(_tess_includes "${_tess_root}/include")
if(EXISTS "${_tess_root}/include/tesseract/version.h.in"
		AND EXISTS "${_tess_root}/VERSION")
	file(STRINGS "${_tess_root}/VERSION" _tess_plain)
	string(REGEX REPLACE "^([^.]*)\\..*" "\\1" GENERIC_MAJOR_VERSION "${_tess_plain}")
	string(REGEX REPLACE "^[^.]*\\.([^.]*)\\..*" "\\1" GENERIC_MINOR_VERSION "${_tess_plain}")
	string(REGEX REPLACE "^[^.]*\\.[^.]*\\.([0-9]*).*" "\\1" GENERIC_MICRO_VERSION "${_tess_plain}")
	string(STRIP "${_tess_plain}" PACKAGE_VERSION)
	configure_file(
		"${_tess_root}/include/tesseract/version.h.in"
		"${CMAKE_CURRENT_BINARY_DIR}/include/tesseract/version.h"
		@ONLY
	)
	set(_tess_includes "${CMAKE_CURRENT_BINARY_DIR}/include" "${_tess_includes}")
	unset(_tess_plain)
	unset(GENERIC_MAJOR_VERSION)
	unset(GENERIC_MINOR_VERSION)
	unset(GENERIC_MICRO_VERSION)
	unset(PACKAGE_VERSION)
endif()
target_include_directories(StormByte-Multimedia BEFORE PRIVATE ${_tess_includes})

set(TESSERACT_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-cmake-unversioned-tesseract-output-name.patch"
)

set(TESSERACT_OPTIONS
	"BUILD_TRAINING_TOOLS=OFF"
	"BUILD_TESTS=OFF"
	"INSTALL_CONFIGS=ON"
	"DISABLE_CURL=ON"
	"DISABLE_ARCHIVE=ON"
	"ENABLE_UNITY_BUILD=ON"
)
set(_tesseract_raw_flags "${CMAKE_CXX_FLAGS}")
if(MSVC)
	string(APPEND _tesseract_raw_flags " /DTESSERACT_DISABLE_DEBUG_FONTS /DTESSERACT_IMAGEDATA_AS_PIX")
else()
	string(APPEND _tesseract_raw_flags " -DTESSERACT_DISABLE_DEBUG_FONTS -DTESSERACT_IMAGEDATA_AS_PIX")
endif()
list(APPEND TESSERACT_OPTIONS "CMAKE_CXX_FLAGS=${_tesseract_raw_flags}")
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION STREQUAL "ON")
	list(APPEND TESSERACT_OPTIONS "ENABLE_LTO=ON")
endif()
if(OCR_BUNDLED_TYPE STREQUAL "shared")
	list(APPEND TESSERACT_OPTIONS "-DBUILD_SHARED_LIBS=ON")
else()
	list(APPEND TESSERACT_OPTIONS "-DBUILD_SHARED_LIBS=OFF")
endif()

set(TESSERACT_EXTRA "GIT={RESET;PATCH={${TESSERACT_PATCHES}};TITLE=tesseract unversioned name}")

buildmaster_component(
	tesseract
	"${TESSERACT_COMPONENT}"
	"${TESSERACT_SRC_DIR}"
	"${TESSERACT_OPTIONS}"
	"${OCR_BUNDLED_TYPE}"
	"tesseract"
	"${TESSERACT_EXTRA}"
)
buildmaster_link(tesseract leptonica)
buildmaster_group_add(OCR tesseract)
