list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(_tessdata_opts "")
if(WITH_OCR STREQUAL "SYSTEM")
	find_package(Tessdata REQUIRED)
	message(STATUS "tessdata ${TESSDATA_LANGUAGE_COUNT} languages [SYSTEM] dir=${TESSDATA_DIR}")
	set(_tessdata_opts
		"WITH_OCR=SYSTEM;CMAKE_MODULE_PATH=${CMAKE_SOURCE_DIR}/cmake"
	)
else()
	message(STATUS "tessdata best [BUNDLED]: languages selected in cmake-wrapper/CMakeLists.txt")
	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/cmake-wrapper/include"
	)
endif()

buildmaster_component(
	tessdata
	"Tesseract traineddata"
	"${CMAKE_CURRENT_LIST_DIR}/cmake-wrapper"
	"${_tessdata_opts}"
	static
	"tessdata"
)

buildmaster_link(StormByte-Multimedia tessdata)
buildmaster_group_add(OCR tessdata)
