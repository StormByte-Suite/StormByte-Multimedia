if(NOT DEFINED WITH_ZIMG)
	if(WIN32)
		set(WITH_ZIMG "BUNDLED" CACHE STRING "Use zimg (SYSTEM or BUNDLED)" FORCE)
	else()
		set(WITH_ZIMG "BUNDLED" CACHE STRING "Use zimg (SYSTEM or BUNDLED)")
	endif()
endif()
set_property(CACHE WITH_ZIMG PROPERTY STRINGS SYSTEM BUNDLED)

if(WITH_ZIMG STREQUAL "SYSTEM")
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
	find_package(zimg REQUIRED)
	add_library(zimg INTERFACE)
	target_link_libraries(zimg INTERFACE zimg::zimg)
	message(STATUS "zimg [SYSTEM]")
elseif(WITH_ZIMG STREQUAL "BUNDLED")
	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/cmake-wrapper/src/src/zimg/api"
	)
	buildmaster_component(
		zimg
		"zimg"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake-wrapper"
		"-DZIMG_ENABLE_SIMD=ON"
		"static"
		"zimg"
		""
	)
else()
	message(FATAL_ERROR "WITH_ZIMG must be SYSTEM or BUNDLED (got '${WITH_ZIMG}')")
endif()

buildmaster_link(StormByte-Multimedia zimg)
