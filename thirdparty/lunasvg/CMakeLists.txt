if(NOT DEFINED WITH_LUNASVG)
	set(WITH_LUNASVG "BUNDLED" CACHE STRING "Use LunaSVG (SYSTEM or BUNDLED)")
endif()
set_property(CACHE WITH_LUNASVG PROPERTY STRINGS SYSTEM BUNDLED)

if(WITH_LUNASVG STREQUAL "SYSTEM")
	find_package(lunasvg 3.5.0 CONFIG REQUIRED)
	add_library(lunasvg INTERFACE)
	target_link_libraries(lunasvg INTERFACE lunasvg::lunasvg)
	message(STATUS "LunaSVG ${lunasvg_VERSION} [SYSTEM]")
elseif(WITH_LUNASVG STREQUAL "BUNDLED")
	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src/include"
	)
	target_compile_definitions(StormByte-Multimedia PRIVATE LUNASVG_BUILD_STATIC)

	buildmaster_component(
		lunasvg
		"LunaSVG"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"-DBUILD_SHARED_LIBS=OFF;-DUSE_SYSTEM_PLUTOVG=OFF;-DLUNASVG_BUILD_EXAMPLES=OFF;-DPLUTOVG_BUILD_EXAMPLES=OFF;-DLUNASVG_DISABLE_LOAD_SYSTEM_FONTS=ON;-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD=ON"
		"static"
		"lunasvg;plutovg"
		"BACKEND=cmake"
	)
else()
	message(FATAL_ERROR "WITH_LUNASVG must be SYSTEM or BUNDLED (got '${WITH_LUNASVG}')")
endif()

buildmaster_link(StormByte-Multimedia lunasvg)
