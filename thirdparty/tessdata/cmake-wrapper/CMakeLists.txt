cmake_minimum_required(VERSION 3.20)
project(tessdata LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TESSDATA_NAMESPACE     "tessdata")
set(TESSDATA_FUNCTION_NAME "blob")

set(TESSDATA_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
include("${CMAKE_CURRENT_LIST_DIR}/../languages/CMakeLists.txt")

include("${CMAKE_CURRENT_LIST_DIR}/tessdata.cmake")

if(NOT DEFINED STORMBYTE_TESSDATA_GENERATED_SOURCE)
	message(FATAL_ERROR "tessdata: no generated sources")
endif()

add_library(tessdata STATIC ${STORMBYTE_TESSDATA_GENERATED_SOURCE})
set_target_properties(tessdata PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	OUTPUT_NAME tessdata
)

target_include_directories(tessdata
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

include(GNUInstallDirs)
install(TARGETS tessdata
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/tessdata.h"
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
