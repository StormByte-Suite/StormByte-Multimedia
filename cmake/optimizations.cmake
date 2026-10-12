# Optimizations
#
# Windows: StormByte-Multimedia is clang-cl (#embed). FFmpeg / plugins
# stay MSVC. No LTO across those triples.
#
# Release defaults are added only when the corresponding global language
# flags are empty, so explicit toolchain/user flags remain authoritative.
# Keep the CPU baseline global for single-config Release builds so bundled
# dependencies inherit it; multi-config builds keep it in Release only.
# Never add /fp:fast or /EHsc here: they break zimg.
#
# x86-64: x86-64-v3 (AVX2+FMA+BMI). Apple Silicon: apple-m1 (runs on
# M1 through current M-series). Use -O2; -O3 miscompiles codec/color
# paths. Linux toolchain flags, when present, are left untouched.

if(WIN32 OR APPLE)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
else()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(_sb_has_release FALSE)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
	set(_sb_has_release TRUE)
elseif(CMAKE_CONFIGURATION_TYPES)
	list(FIND CMAKE_CONFIGURATION_TYPES "Release" _sb_release_index)
	if(NOT _sb_release_index EQUAL -1)
		set(_sb_has_release TRUE)
	endif()
endif()

if(_sb_has_release)
	set(_sb_release_optimization "")
	set(_sb_release_cpu "")
	set(_sb_c_flags_custom FALSE)
	set(_sb_cxx_flags_custom FALSE)
	string(STRIP "${CMAKE_C_FLAGS_RELEASE_INIT}" _sb_c_flags_release_init)
	string(STRIP "${CMAKE_CXX_FLAGS_RELEASE_INIT}" _sb_cxx_flags_release_init)
	if(NOT CMAKE_C_FLAGS STREQUAL "")
		set(_sb_c_flags_custom TRUE)
	elseif(NOT "${CMAKE_C_FLAGS_RELEASE}" STREQUAL "${_sb_c_flags_release_init}")
		set(_sb_c_flags_custom TRUE)
	endif()
	if(NOT CMAKE_CXX_FLAGS STREQUAL "")
		set(_sb_cxx_flags_custom TRUE)
	elseif(NOT "${CMAKE_CXX_FLAGS_RELEASE}" STREQUAL "${_sb_cxx_flags_release_init}")
		set(_sb_cxx_flags_custom TRUE)
	endif()

	if(MSVC)
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x86)$")
			set(_sb_release_cpu "/arch:AVX2")
		endif()
	else()
		set(_sb_release_optimization "-O2")
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x86)$")
			set(_sb_release_cpu "-march=x86-64-v3")
		elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
			set(_sb_release_cpu "-mcpu=apple-m1")
		endif()
	endif()

	if(NOT _sb_c_flags_custom)
		if(CMAKE_BUILD_TYPE STREQUAL "Release")
			set(CMAKE_C_FLAGS "${_sb_release_cpu}" CACHE STRING "C compiler flags" FORCE)
		else()
			string(APPEND CMAKE_C_FLAGS_RELEASE " ${_sb_release_cpu}")
		endif()
		if(NOT MSVC)
			if(CMAKE_C_FLAGS_RELEASE MATCHES "(^| )-O3( |$)")
				string(REGEX REPLACE "(^| )-O3( |$)" " " CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
				string(APPEND CMAKE_C_FLAGS_RELEASE " ${_sb_release_optimization}")
			elseif(NOT CMAKE_C_FLAGS_RELEASE MATCHES "(^| )-O([0-3]|s|fast|g|z)( |$)")
				string(APPEND CMAKE_C_FLAGS_RELEASE " ${_sb_release_optimization}")
			endif()
			string(STRIP "${CMAKE_C_FLAGS_RELEASE}" CMAKE_C_FLAGS_RELEASE)
		endif()
	endif()
	if(NOT _sb_cxx_flags_custom)
		if(CMAKE_BUILD_TYPE STREQUAL "Release")
			set(CMAKE_CXX_FLAGS "${_sb_release_cpu}" CACHE STRING "CXX compiler flags" FORCE)
		else()
			string(APPEND CMAKE_CXX_FLAGS_RELEASE " ${_sb_release_cpu}")
		endif()
		if(NOT MSVC)
			if(CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )-O3( |$)")
				string(REGEX REPLACE "(^| )-O3( |$)" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
				string(APPEND CMAKE_CXX_FLAGS_RELEASE " ${_sb_release_optimization}")
			elseif(NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )-O([0-3]|s|fast|g|z)( |$)")
				string(APPEND CMAKE_CXX_FLAGS_RELEASE " ${_sb_release_optimization}")
			endif()
			string(STRIP "${CMAKE_CXX_FLAGS_RELEASE}" CMAKE_CXX_FLAGS_RELEASE)
		endif()
	endif()
endif()
