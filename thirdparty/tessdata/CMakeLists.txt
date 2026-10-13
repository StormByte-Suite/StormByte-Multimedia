list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(NOT DEFINED WITH_TESSDATA)
	set(WITH_TESSDATA "BUNDLED" CACHE STRING "Use tessdata models (SYSTEM or BUNDLED)")
endif()
set_property(CACHE WITH_TESSDATA PROPERTY STRINGS SYSTEM BUNDLED)
include("${CMAKE_CURRENT_LIST_DIR}/languages/CMakeLists.txt")

if(WITH_TESSDATA STREQUAL "SYSTEM")
	find_package(Tessdata REQUIRED)
	get_filename_component(TESSDATA_DIR "${TESSDATA_DIR}" ABSOLUTE)
	file(TO_CMAKE_PATH "${TESSDATA_DIR}" TESSDATA_DIR)
	message(STATUS "tessdata ${TESSDATA_LANGUAGE_COUNT} languages [SYSTEM] dir=${TESSDATA_DIR}")
	target_compile_definitions(StormByte-Multimedia PRIVATE
		STORMBYTE_MULTIMEDIA_TESSDATA_SYSTEM
		"STORMBYTE_MULTIMEDIA_TESSDATA_DIR=\"${TESSDATA_DIR}\"")
	get_target_property(_tessdata_sources StormByte-Multimedia SOURCES)
	list(FILTER _tessdata_sources EXCLUDE REGEX "/ocr/tessdata\\.cxx$")
	set_property(TARGET StormByte-Multimedia PROPERTY SOURCES "${_tessdata_sources}")
elseif(WITH_TESSDATA STREQUAL "BUNDLED")
	message(STATUS "tessdata best [BUNDLED]: languages selected in languages/CMakeLists.txt")
	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/cmake-wrapper/include"
	)
	buildmaster_component(
		tessdata
		"Tesseract traineddata"
		"${CMAKE_CURRENT_LIST_DIR}/cmake-wrapper"
		""
		static
		"tessdata"
	)
	buildmaster_link(StormByte-Multimedia tessdata)
else()
	message(FATAL_ERROR "WITH_TESSDATA must be SYSTEM or BUNDLED (got '${WITH_TESSDATA}')")
endif()
