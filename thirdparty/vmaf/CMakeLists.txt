if(NOT DEFINED WITH_VMAF)
	if(WIN32)
		set(WITH_VMAF "BUNDLED" CACHE STRING "Use VMAF libraries (SYSTEM or BUNDLED)" FORCE)
	else()
		set(WITH_VMAF "BUNDLED" CACHE STRING "Use VMAF libraries (SYSTEM or BUNDLED)")
	endif()
endif()
include("${CMAKE_SOURCE_DIR}/cmake/align_vmaf.cmake")
set_property(CACHE WITH_VMAF PROPERTY STRINGS SYSTEM BUNDLED)

if(WITH_VMAF STREQUAL "SYSTEM")
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
	find_package(Vmaf REQUIRED)
	add_library(vmaf INTERFACE)
	target_link_libraries(vmaf INTERFACE VMAF::vmaf)
	message(STATUS "VMAF ${VMAF_VERSION} [SYSTEM] include=${VMAF_INCLUDE_DIRS}")
elseif(WITH_VMAF STREQUAL "BUNDLED")
	set(VMAF_GIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${VMAF_GIT_DIR}/libvmaf/include"
	)

	set(VMAF_COMPONENT "VMAF library")
	set(VMAF_PATCHES
		"${CMAKE_CURRENT_LIST_DIR}/0001-clang-cl-math-and-warnings.patch"
		"${CMAKE_CURRENT_LIST_DIR}/0002-libvmaf-rename-static-close.patch"
	)
	set(VMAF_OPTIONS
		"-Denable_tests=false"
		"-Denable_docs=false"
		"-Denable_tools=false"
		"-Denable_asm=true"
		"-Dbuilt_in_models=true"
		"-Denable_cuda=false"
		"-Denable_nvtx=false"
		"-Denable_nvcc=false"
	)
	SET(VMAF_OPTS_WINDOWS)
	if(WIN32)
		list(APPEND VMAF_OPTIONS "-Dbundled_winpthreads=false")
		# svm.cpp is C++ and includes the MSVC STL. libvmaf asks for C++11.
		# VS 18 <ratio> uses digit separators, which that mode rejects.
		list(APPEND VMAF_OPTIONS "-Dcpp_std=c++17")
	endif()
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
		list(APPEND VMAF_OPTIONS "-Denable_avx512=true")
	else()
		list(APPEND VMAF_OPTIONS "-Denable_avx512=false")
	endif()

	buildmaster_component(
		vmaf
		"${VMAF_COMPONENT}"
		"${VMAF_GIT_DIR}"
		"${VMAF_OPTIONS}"
		"static"
		"vmaf"
		"SOURCE=libvmaf;GIT={RESET;PATCH={${VMAF_PATCHES}};TITLE=VMAF compatibility patches};${VMAF_OPTS_WINDOWS}"
	)
	if(MSVC)
		buildmaster_link(vmaf pthread-win32)
	endif()
else()
	message(FATAL_ERROR "WITH_VMAF must be SYSTEM or BUNDLED (got '${WITH_VMAF}')")
endif()
buildmaster_link(StormByte-Multimedia vmaf)
