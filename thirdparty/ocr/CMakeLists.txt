set(WITH_OCR "BUNDLED" CACHE STRING "Use Tesseract OCR (SYSTEM or BUNDLED)")
set_property(CACHE WITH_OCR PROPERTY STRINGS BUNDLED SYSTEM)
if(WIN32)
	set(WITH_OCR "BUNDLED" CACHE STRING "Use Tesseract OCR (SYSTEM or BUNDLED)" FORCE)
endif()
if(NOT WITH_OCR STREQUAL "SYSTEM" AND NOT WITH_OCR STREQUAL "BUNDLED")
	set(WITH_OCR "BUNDLED" CACHE STRING "Use Tesseract OCR (SYSTEM or BUNDLED)" FORCE)
endif()

if(NOT OCR_BUNDLED_TYPE STREQUAL "static" AND NOT OCR_BUNDLED_TYPE STREQUAL "shared")
	set(OCR_BUNDLED_TYPE "static" CACHE STRING "Build OCR deps as static or shared" FORCE)
endif()
if(MSVC)
	set(OCR_BUNDLED_TYPE "static" CACHE STRING "Build OCR deps as static or shared" FORCE)
endif()

buildmaster_group(OCR "OCR")

if(WITH_OCR STREQUAL "SYSTEM")
	find_path(Tesseract_INCLUDE_DIR
		NAMES tesseract/baseapi.h
		REQUIRED
	)
	find_library(Tesseract_LIBRARY
		NAMES tesseract libtesseract
		REQUIRED
	)
	add_library(tesseract INTERFACE)
	target_include_directories(tesseract INTERFACE "${Tesseract_INCLUDE_DIR}")
	target_link_libraries(tesseract INTERFACE "${Tesseract_LIBRARY}")
else()
	add_subdirectory(leptonica)
	add_subdirectory(tesseract)
endif()

buildmaster_link(StormByte-Multimedia tesseract)
