if(NOT DEFINED WITH_EBUR128)
	if(WIN32)
		set(WITH_EBUR128 "BUNDLED" CACHE STRING "Use libebur128 (SYSTEM or BUNDLED)" FORCE)
	else()
		set(WITH_EBUR128 "BUNDLED" CACHE STRING "Use libebur128 (SYSTEM or BUNDLED)")
	endif()
endif()
set_property(CACHE WITH_EBUR128 PROPERTY STRINGS SYSTEM BUNDLED)

if(WITH_EBUR128 STREQUAL "SYSTEM")
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
	find_package(EBUR128 REQUIRED)
	add_library(ebur128 INTERFACE)
	target_link_libraries(ebur128 INTERFACE EBUR128::ebur128)
	message(STATUS "libebur128 [SYSTEM]")
elseif(WITH_EBUR128 STREQUAL "BUNDLED")
	set(EBUR128_PATCH "${CMAKE_CURRENT_LIST_DIR}/0001-cmake-minimum-3.5.patch")
	buildmaster_component(
		ebur128
		"libebur128"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"-DBUILD_SHARED_LIBS=OFF;-DENABLE_TESTS=OFF"
		"static"
		"ebur128"
		"GIT={RESET;PATCH=${EBUR128_PATCH};TITLE=CMake 4}"
	)
else()
	message(FATAL_ERROR "WITH_EBUR128 must be SYSTEM or BUNDLED (got '${WITH_EBUR128}')")
endif()

buildmaster_link(StormByte-Multimedia ebur128)
