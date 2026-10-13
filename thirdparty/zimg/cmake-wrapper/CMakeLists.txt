cmake_minimum_required(VERSION 3.16)
project(zimg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Upstream submodule root
set(ZIMG_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ZIMG_SRC      "${ZIMG_UPSTREAM}/src/zimg")

option(ZIMG_ENABLE_SIMD "Enable architecture-specific SIMD" ON)

# Core sources from Makefile.am (libzimg_internal_la_SOURCES) @ release-3.0.6
set(ZIMG_CORE_SOURCES
	${ZIMG_SRC}/api/zimg.cpp

	${ZIMG_SRC}/colorspace/colorspace.cpp
	${ZIMG_SRC}/colorspace/colorspace_param.cpp
	${ZIMG_SRC}/colorspace/gamma.cpp
	${ZIMG_SRC}/colorspace/graph.cpp
	${ZIMG_SRC}/colorspace/matrix3.cpp
	${ZIMG_SRC}/colorspace/operation.cpp
	${ZIMG_SRC}/colorspace/operation_impl.cpp

	${ZIMG_SRC}/common/cpuinfo.cpp
	${ZIMG_SRC}/common/libm_wrapper.cpp
	${ZIMG_SRC}/common/matrix.cpp

	${ZIMG_SRC}/depth/blue.cpp
	${ZIMG_SRC}/depth/depth.cpp
	${ZIMG_SRC}/depth/depth_convert.cpp
	${ZIMG_SRC}/depth/dither.cpp
	${ZIMG_SRC}/depth/quantize.cpp

	${ZIMG_SRC}/graph/basic_filter.cpp
	${ZIMG_SRC}/graph/filtergraph.cpp
	${ZIMG_SRC}/graph/graphbuilder.cpp
	${ZIMG_SRC}/graph/graphnode.cpp

	${ZIMG_SRC}/resize/filter.cpp
	${ZIMG_SRC}/resize/resize.cpp
	${ZIMG_SRC}/resize/resize_impl.cpp

	${ZIMG_SRC}/unresize/bilinear.cpp
	${ZIMG_SRC}/unresize/unresize.cpp
	${ZIMG_SRC}/unresize/unresize_impl.cpp
)

set(ZIMG_IS_X86 FALSE)
set(ZIMG_IS_ARM FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	set(ZIMG_IS_X86 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|armv[0-9]|arm)$")
	set(ZIMG_IS_ARM TRUE)
endif()

set(ZIMG_SIMD_SOURCES)
set(ZIMG_SSE_SOURCES)
set(ZIMG_SSE2_SOURCES)
set(ZIMG_AVX_SOURCES)
set(ZIMG_F16C_SOURCES)
set(ZIMG_AVX2_SOURCES)
set(ZIMG_AVX512_SOURCES)
set(ZIMG_AVX512_VNNI_SOURCES)
set(ZIMG_NEON_SOURCES)

if(ZIMG_ENABLE_SIMD AND ZIMG_IS_X86)
	list(APPEND ZIMG_CORE_SOURCES
		${ZIMG_SRC}/colorspace/x86/operation_impl_x86.cpp
		${ZIMG_SRC}/common/x86/cpuinfo_x86.cpp
		${ZIMG_SRC}/common/x86/x86util.cpp
		${ZIMG_SRC}/depth/x86/depth_convert_x86.cpp
		${ZIMG_SRC}/depth/x86/dither_x86.cpp
		${ZIMG_SRC}/resize/x86/resize_impl_x86.cpp
	)

	set(ZIMG_SSE_SOURCES
		${ZIMG_SRC}/colorspace/x86/operation_impl_sse.cpp
		${ZIMG_SRC}/resize/x86/resize_impl_sse.cpp
	)
	set(ZIMG_SSE2_SOURCES
		${ZIMG_SRC}/colorspace/x86/operation_impl_sse2.cpp
		${ZIMG_SRC}/depth/x86/depth_convert_sse2.cpp
		${ZIMG_SRC}/depth/x86/dither_sse2.cpp
		${ZIMG_SRC}/depth/x86/error_diffusion_sse2.cpp
		${ZIMG_SRC}/depth/x86/f16c_sse2.cpp
		${ZIMG_SRC}/resize/x86/resize_impl_sse2.cpp
	)
	set(ZIMG_AVX_SOURCES
		${ZIMG_SRC}/colorspace/x86/operation_impl_avx.cpp
		${ZIMG_SRC}/resize/x86/resize_impl_avx.cpp
	)
	set(ZIMG_F16C_SOURCES
		${ZIMG_SRC}/depth/x86/f16c_ivb.cpp
	)
	set(ZIMG_AVX2_SOURCES
		${ZIMG_SRC}/colorspace/x86/operation_impl_avx2.cpp
		${ZIMG_SRC}/depth/x86/depth_convert_avx2.cpp
		${ZIMG_SRC}/depth/x86/dither_avx2.cpp
		${ZIMG_SRC}/depth/x86/error_diffusion_avx2.cpp
		${ZIMG_SRC}/resize/x86/resize_impl_avx2.cpp
	)
	set(ZIMG_AVX512_SOURCES
		${ZIMG_SRC}/colorspace/x86/gamma_constants_avx512.cpp
		${ZIMG_SRC}/colorspace/x86/operation_impl_avx512.cpp
		${ZIMG_SRC}/depth/x86/depth_convert_avx512.cpp
		${ZIMG_SRC}/depth/x86/dither_avx512.cpp
		${ZIMG_SRC}/resize/x86/resize_impl_avx512.cpp
	)
	set(ZIMG_AVX512_VNNI_SOURCES
		${ZIMG_SRC}/resize/x86/resize_impl_avx512_vnni.cpp
	)

	list(APPEND ZIMG_SIMD_SOURCES
		${ZIMG_SSE_SOURCES}
		${ZIMG_SSE2_SOURCES}
		${ZIMG_AVX_SOURCES}
		${ZIMG_F16C_SOURCES}
		${ZIMG_AVX2_SOURCES}
		${ZIMG_AVX512_SOURCES}
		${ZIMG_AVX512_VNNI_SOURCES}
	)
endif()

if(ZIMG_ENABLE_SIMD AND ZIMG_IS_ARM)
	list(APPEND ZIMG_CORE_SOURCES
		${ZIMG_SRC}/colorspace/arm/operation_impl_arm.cpp
		${ZIMG_SRC}/common/arm/cpuinfo_arm.cpp
		${ZIMG_SRC}/common/arm/neon_util.cpp
		${ZIMG_SRC}/depth/arm/depth_convert_arm.cpp
		${ZIMG_SRC}/depth/arm/dither_arm.cpp
		${ZIMG_SRC}/resize/arm/resize_impl_arm.cpp
	)
	set(ZIMG_NEON_SOURCES
		${ZIMG_SRC}/colorspace/arm/operation_impl_neon.cpp
		${ZIMG_SRC}/depth/arm/depth_convert_neon.cpp
		${ZIMG_SRC}/depth/arm/dither_neon.cpp
		${ZIMG_SRC}/depth/arm/f16c_neon.cpp
		${ZIMG_SRC}/resize/arm/resize_impl_neon.cpp
	)
	list(APPEND ZIMG_SIMD_SOURCES ${ZIMG_NEON_SOURCES})
endif()

add_library(zimg ${ZIMG_CORE_SOURCES} ${ZIMG_SIMD_SOURCES})

if(ZIMG_ENABLE_SIMD AND ZIMG_IS_X86)
	target_compile_definitions(zimg PRIVATE ZIMG_X86 ZIMG_X86_AVX512)
elseif(ZIMG_ENABLE_SIMD AND ZIMG_IS_ARM)
	target_compile_definitions(zimg PRIVATE ZIMG_ARM)
endif()

if(MSVC)
	target_compile_options(zimg PRIVATE /EHsc)
endif()

set_target_properties(zimg PROPERTIES
	OUTPUT_NAME zimg
	VERSION 2.0.0
	SOVERSION 2
)

target_include_directories(zimg
	PUBLIC
		$<BUILD_INTERFACE:${ZIMG_SRC}/api>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${ZIMG_SRC}
)

target_compile_definitions(zimg PRIVATE $<$<CONFIG:Release>:NDEBUG>)

if(ZIMG_ENABLE_SIMD AND ZIMG_IS_X86)
	if(NOT MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		set_source_files_properties(${ZIMG_SSE_SOURCES} PROPERTIES COMPILE_OPTIONS "-msse")
		set_source_files_properties(${ZIMG_SSE2_SOURCES} PROPERTIES COMPILE_OPTIONS "-msse2")
		set_source_files_properties(${ZIMG_AVX_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx")
		set_source_files_properties(${ZIMG_F16C_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx;-mf16c")
		set_source_files_properties(${ZIMG_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx2;-mf16c;-mfma")
		set_source_files_properties(${ZIMG_AVX512_SOURCES} PROPERTIES
			COMPILE_OPTIONS "-mavx512f;-mavx512cd;-mavx512vl;-mavx512bw;-mavx512dq")
		set_source_files_properties(${ZIMG_AVX512_VNNI_SOURCES} PROPERTIES
			COMPILE_OPTIONS "-mavx512f;-mavx512cd;-mavx512vl;-mavx512bw;-mavx512dq;-mavx512vnni")
		if(MSVC)
			foreach(source IN LISTS ZIMG_SIMD_SOURCES)
				get_source_file_property(options "${source}" COMPILE_OPTIONS)
				list(TRANSFORM options PREPEND "/clang:")
				set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS "${options}")
			endforeach()
		endif()
	else()
		if(CMAKE_SIZEOF_VOID_P EQUAL 4)
			set_source_files_properties(${ZIMG_SSE_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:SSE")
			set_source_files_properties(${ZIMG_SSE2_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:SSE2")
		endif()
		set_source_files_properties(${ZIMG_AVX_SOURCES} ${ZIMG_F16C_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX")
		set_source_files_properties(${ZIMG_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
		set_source_files_properties(${ZIMG_AVX512_SOURCES} ${ZIMG_AVX512_VNNI_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX512")
	endif()
endif()

if(ZIMG_ENABLE_SIMD AND ZIMG_IS_ARM AND NOT MSVC)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv[0-7]")
		set_source_files_properties(${ZIMG_NEON_SOURCES} PROPERTIES COMPILE_OPTIONS "-mfpu=neon")
	endif()
endif()

include(GNUInstallDirs)

install(TARGETS zimg
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
	${ZIMG_SRC}/api/zimg.h
	${ZIMG_SRC}/api/zimg++.hpp
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(VERSION "3.0.6")
if(MSVC)
	set(STL_LIBS "")
else()
	set(STL_LIBS "-lstdc++")
endif()

configure_file(
	"${ZIMG_UPSTREAM}/zimg.pc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/zimg.pc"
	@ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/zimg.pc"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)