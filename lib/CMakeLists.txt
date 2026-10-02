include(GNUInstallDirs)

file(GLOB_RECURSE STORMBYTE_MULTIMEDIA_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_LIST_DIR}/public/*.cxx"
	"${CMAKE_CURRENT_LIST_DIR}/private/*.cxx"
)

if(BUILD_SHARED_LIBS)
	set(_mode shared)
else()
	set(_mode static)
endif()

buildmaster_component(
	StormByte-Multimedia
	"StormByte Suite - Multimedia"
	"${STORMBYTE_MULTIMEDIA_SOURCES}"
	""
	${_mode}
	"StormByte-Multimedia"
	"BACKEND=host;ALIAS=StormByte::Multimedia"
)

if(BUILD_SHARED_LIBS)
	target_compile_definitions(StormByte-Multimedia
		PRIVATE StormByte_Multimedia_EXPORTS
		PUBLIC STORMBYTE_MULTIMEDIA_SHARED
	)
endif()

set_target_properties(StormByte-Multimedia PROPERTIES
	LINKER_LANGUAGE CXX
	SOVERSION ${CMAKE_PROJECT_VERSION}
	VERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE ON
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON
)

if(MSVC)
	target_compile_options(StormByte-Multimedia PRIVATE
		/EHsc
		$<$<CONFIG:Release>:/O2 /arch:AVX2 /fp:fast>
		$<$<CONFIG:Debug>:/Od>
	)
	target_compile_definitions(StormByte-Multimedia PUBLIC UNICODE NOMINMAX)
else()
	target_compile_options(StormByte-Multimedia PRIVATE
		-fvisibility=hidden
		$<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>
		$<$<CONFIG:Debug>:-pipe -g -ggdb -Wall -Wextra -Wnon-virtual-dtor -pedantic -pedantic-errors -O0>
	)
endif()

target_compile_definitions(StormByte-Multimedia PRIVATE
	"STORMBYTE_MULTIMEDIA_VERSION=\"${CMAKE_PROJECT_VERSION}\""
)

target_include_directories(StormByte-Multimedia
	SYSTEM BEFORE
		PUBLIC  "${CMAKE_CURRENT_LIST_DIR}/public"
		PRIVATE "${CMAKE_CURRENT_LIST_DIR}/private"
)

add_subdirectory(tables)
