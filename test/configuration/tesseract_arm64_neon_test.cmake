cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED REPO OR NOT DEFINED WORK OR WORK STREQUAL ""
		OR NOT DEFINED CXX_COMPILER OR NOT DEFINED GENERATOR)
	message(FATAL_ERROR "REPO, WORK, CXX_COMPILER and GENERATOR are required")
endif()

function(target_include_directories)
endfunction()

function(buildmaster_component)
	set(TESSERACT_COMPONENT_ARGUMENTS "${ARGN}" PARENT_SCOPE)
endfunction()

function(buildmaster_group_add)
endfunction()

function(buildmaster_link)
endfunction()

set(TESSERACT "${REPO}/thirdparty/ocr/tesseract")
set(OCR_BUNDLED_TYPE static)
set(CMAKE_CURRENT_BINARY_DIR "${WORK}")
file(MAKE_DIRECTORY "${WORK}/include/tesseract")
include("${TESSERACT}/CMakeLists.txt")
foreach(patch IN LISTS TESSERACT_PATCHES)
	string(FIND "${TESSERACT_COMPONENT_ARGUMENTS}" "${patch}" registered)
	if(registered LESS 0)
		message(FATAL_ERROR "Tesseract patch is not registered with BuildMaster: ${patch}")
	endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/patched_checkout.cmake")
prepare_patched_checkout("${TESSERACT}/src" "${WORK}" ready ${TESSERACT_PATCHES})
if(NOT ready)
	return()
endif()
file(REMOVE_RECURSE "${WORK}/aarch64" "${WORK}/arm64" "${WORK}/ARM64"
	"${WORK}/x86_64" "${WORK}/leptonica")
file(MAKE_DIRECTORY "${WORK}/leptonica")
file(WRITE "${WORK}/leptonica/LeptonicaConfig.cmake"
	"set(Leptonica_FOUND TRUE)\nset(Leptonica_VERSION 1.99.0)\n")
file(WRITE "${WORK}/leptonica/LeptonicaConfigVersion.cmake"
	"set(PACKAGE_VERSION 1.99.0)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n")
file(APPEND "${WORK}/source/CMakeLists.txt" [=[

get_target_property(_stormbyte_tesseract_sources libtesseract SOURCES)
file(WRITE "${CMAKE_BINARY_DIR}/stormbyte-simd.txt"
	"${HAVE_NEON}\n${_stormbyte_tesseract_sources}\n")
]=])

function(check_tesseract_neon architecture expect_neon)
	execute_process(COMMAND "${CMAKE_COMMAND}"
		-S "${WORK}/source" -B "${WORK}/${architecture}" -G "${GENERATOR}"
		"-DCMAKE_SYSTEM_NAME=${CMAKE_HOST_SYSTEM_NAME}"
		"-DCMAKE_SYSTEM_PROCESSOR=${architecture}"
		"-DCMAKE_CXX_COMPILER=${CXX_COMPILER}" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
		"-DLeptonica_DIR=${WORK}/leptonica" -DBUILD_TRAINING_TOOLS=OFF -DBUILD_TESTS=OFF
		-DGRAPHICS_DISABLED=ON -DDISABLE_TIFF=ON -DDISABLE_ARCHIVE=ON -DDISABLE_CURL=ON
		-DENABLE_CCACHE=OFF -DENABLE_PRECOMPILED_HEADERS=OFF -DENABLE_LTO=OFF
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "${architecture}: Tesseract configuration failed:\n${output}\n${error}")
	endif()
	file(READ "${WORK}/${architecture}/stormbyte-simd.txt" simd)
	foreach(source IN ITEMS dotproductneon.cpp intsimdmatrixneon.cpp)
		if(expect_neon AND NOT simd MATCHES "${source}")
			message(FATAL_ERROR "${architecture}: missing NEON source ${source}")
		elseif(NOT expect_neon AND simd MATCHES "${source}")
			message(FATAL_ERROR "${architecture}: unexpected NEON source ${source}")
		endif()
	endforeach()
	if(expect_neon AND NOT simd MATCHES "^TRUE\n")
		message(FATAL_ERROR "${architecture}: Tesseract did not enable NEON")
	elseif(NOT expect_neon AND NOT simd MATCHES "^FALSE\n")
		message(FATAL_ERROR "${architecture}: Tesseract unexpectedly enabled NEON")
	endif()
	message(STATUS "${architecture}: Tesseract SIMD configuration verified")
endfunction()

check_tesseract_neon(aarch64 TRUE)
check_tesseract_neon(arm64 TRUE)
check_tesseract_neon(ARM64 TRUE)
check_tesseract_neon(x86_64 FALSE)

file(REMOVE_RECURSE "${WORK}/source" "${WORK}/aarch64" "${WORK}/arm64"
	"${WORK}/ARM64" "${WORK}/x86_64" "${WORK}/leptonica" "${WORK}/include")
