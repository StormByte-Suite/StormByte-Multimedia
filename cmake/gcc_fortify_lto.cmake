if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
		OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
		OR NOT ENABLE_LTO)
	return()
endif()

function(_multimedia_check_gcc_fortify_lto)
	set(_static_only FALSE)
	if(CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
		set(_static_only TRUE)
	endif()
	set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
	set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
	set(_probe "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/probes/fortify_lto.cxx")
	# GCC's C++26 to_string avoids vsnprintf; C++23 code such as bundled
	# Tesseract still reaches it. The keywords override CMAKE_CXX_STANDARD.
	set(_probe_std CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
	set(_probe_flags
		"-DCMAKE_BUILD_TYPE=Release"
		"-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON")

	try_compile(_works SOURCES "${_probe}"
		CMAKE_FLAGS ${_probe_flags}
		${_probe_std}
		NO_CACHE OUTPUT_VARIABLE _output)
	if(_works)
		return()
	endif()

	if(NOT _output MATCHES "fortify/stdio[.]h"
			OR NOT _output MATCHES "vsnprintf"
			OR NOT _output MATCHES "always_inline")
		if(_static_only)
			message(WARNING
				"Could not check GCC LTO/fortify compatibility with an executable. "
				"Static-only toolchains may need -fno-builtin-vsnprintf at compile "
				"time: https://gitlab.alpinelinux.org/alpine/aports/-/issues/13735\n${_output}")
			return()
		endif()
		message(FATAL_ERROR "GCC LTO compatibility probe failed:\n${_output}")
	endif()

	# Alpine's fortify wrapper and GCC's C++23 to_string conflict under LTO.
	if(CMAKE_CXX_FLAGS STREQUAL "")
		set(CMAKE_CXX_FLAGS "-fno-builtin-vsnprintf")
	else()
		string(APPEND CMAKE_CXX_FLAGS " -fno-builtin-vsnprintf")
	endif()
	try_compile(_workaround_works SOURCES "${_probe}"
		CMAKE_FLAGS ${_probe_flags}
		${_probe_std}
		NO_CACHE OUTPUT_VARIABLE _workaround_output)
	if(NOT _workaround_works)
		message(FATAL_ERROR
			"GCC LTO/fortify workaround did not fix the compiler probe:\n${_workaround_output}")
	endif()

	get_property(_flags_help CACHE CMAKE_CXX_FLAGS PROPERTY HELPSTRING)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "${_flags_help}" FORCE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
	message(STATUS
		"GCC LTO/fortify workaround: -fno-builtin-vsnprintf "
		"(LTO and fortify remain enabled; Alpine issue 13735)")
endfunction()

_multimedia_check_gcc_fortify_lto()
