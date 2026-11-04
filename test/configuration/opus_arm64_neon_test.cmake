cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED REPO OR NOT DEFINED WORK OR WORK STREQUAL ""
		OR NOT DEFINED C_COMPILER OR NOT DEFINED GENERATOR)
	message(FATAL_ERROR "REPO, WORK, C_COMPILER and GENERATOR are required")
endif()

set(OPUS "${REPO}/thirdparty/ffmpeg/plugins/codecs/audio/opus")
function(buildmaster_component)
	set(OPUS_COMPONENT_ARGUMENTS "${ARGN}" PARENT_SCOPE)
endfunction()

function(buildmaster_group_add)
endfunction()

function(register_plugin)
endfunction()

set(FFMPEG_BUNDLED_TYPE static)
set(BUILDMASTER_BINDIR "${WORK}/buildmaster")
set(CMAKE_CURRENT_BINARY_DIR "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
include("${OPUS}/CMakeLists.txt")
foreach(patch IN LISTS OPUS_PATCHES)
	string(FIND "${OPUS_COMPONENT_ARGUMENTS}" "${patch}" registered)
	if(registered LESS 0)
		message(FATAL_ERROR "Opus patch is not registered with BuildMaster: ${patch}")
	endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/patched_checkout.cmake")
prepare_patched_checkout("${OPUS}/src" "${WORK}" ready ${OPUS_PATCHES})
if(NOT ready)
	return()
endif()
file(REMOVE_RECURSE "${WORK}/darwin-arm64" "${WORK}/linux-aarch64"
	"${WORK}/linux-ARM64" "${WORK}/linux-armv7")
file(APPEND "${WORK}/source/CMakeLists.txt" [=[

get_target_property(_stormbyte_neon_definitions opus COMPILE_DEFINITIONS)
file(WRITE "${CMAKE_BINARY_DIR}/stormbyte-neon-definitions.txt"
	"${_stormbyte_neon_definitions}\n")
]=])

function(check_opus_neon name system processor expect_presume expect_rtcd)
	set(CMAKE_SYSTEM_NAME "${system}")
	set(CMAKE_SYSTEM_PROCESSOR "${processor}")
	include("${OPUS}/CMakeLists.txt")
	set(neon_options)
	foreach(option IN LISTS OPUS_OPTIONS)
		if(option MATCHES "^-DOPUS_(USE_NEON|MAY_HAVE_NEON|PRESUME_NEON|DISABLE_INTRINSICS)=")
			list(APPEND neon_options "${option}")
		endif()
	endforeach()
	execute_process(COMMAND "${CMAKE_COMMAND}"
		-S "${WORK}/source" -B "${WORK}/${name}" -G "${GENERATOR}"
		"-DCMAKE_SYSTEM_NAME=${system}" "-DCMAKE_SYSTEM_PROCESSOR=${processor}"
		"-DCMAKE_C_COMPILER=${C_COMPILER}" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
		-DHAVE_ARM_NEON_H=1 -DOPUS_DRED=OFF -DOPUS_OSCE=OFF
		-DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_PROGRAMS=OFF ${neon_options}
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "${name}: Opus configuration failed:\n${output}\n${error}")
	endif()
	if("${output}\n${error}" MATCHES "Runtime cpu capability detection needed for MAY_HAVE_NEON")
		message(FATAL_ERROR "${name}: Opus reported unavailable runtime NEON detection")
	endif()
	file(READ "${WORK}/${name}/stormbyte-neon-definitions.txt" definitions)
	string(STRIP "${definitions}" definitions)
	foreach(definition IN ITEMS OPUS_ARM_MAY_HAVE_NEON OPUS_ARM_MAY_HAVE_NEON_INTR)
		if(NOT definition IN_LIST definitions)
			message(FATAL_ERROR "${name}: missing ${definition}: ${definitions}")
		endif()
	endforeach()
	foreach(definition IN ITEMS OPUS_ARM_PRESUME_NEON OPUS_ARM_PRESUME_NEON_INTR)
		if(expect_presume AND NOT definition IN_LIST definitions)
			message(FATAL_ERROR "${name}: missing ${definition}: ${definitions}")
		elseif(NOT expect_presume AND definition IN_LIST definitions)
			message(FATAL_ERROR "${name}: unexpected ${definition}: ${definitions}")
		endif()
	endforeach()
	if(expect_rtcd AND NOT "OPUS_HAVE_RTCD" IN_LIST definitions)
		message(FATAL_ERROR "${name}: missing runtime CPU detection: ${definitions}")
	elseif(NOT expect_rtcd AND "OPUS_HAVE_RTCD" IN_LIST definitions)
		message(FATAL_ERROR "${name}: unexpected runtime CPU detection: ${definitions}")
	endif()
	message(STATUS "${name}: Opus NEON configuration verified")
endfunction()

check_opus_neon(darwin-arm64 Darwin arm64 TRUE FALSE)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	check_opus_neon(linux-aarch64 Linux aarch64 TRUE TRUE)
	check_opus_neon(linux-ARM64 Linux ARM64 TRUE TRUE)
	check_opus_neon(linux-armv7 Linux armv7 FALSE TRUE)
endif()

file(REMOVE_RECURSE "${WORK}/source" "${WORK}/darwin-arm64"
	"${WORK}/linux-aarch64" "${WORK}/linux-ARM64" "${WORK}/linux-armv7")
file(REMOVE "${WORK}/opus_system_includes.cmake")
