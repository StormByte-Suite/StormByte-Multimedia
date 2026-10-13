cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED REPO OR NOT DEFINED WORK)
	message(FATAL_ERROR "REPO and WORK are required")
endif()
find_package(Git QUIET)
set(X265 "${REPO}/thirdparty/ffmpeg/plugins/codecs/video/x265")
if(NOT GIT_FOUND)
	message(STATUS "SKIPPED: Git is unavailable")
	return()
endif()
set(checkout_result 1)
if(IS_DIRECTORY "${X265}/src")
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${X265}/src" rev-parse --show-toplevel
		RESULT_VARIABLE checkout_result OUTPUT_VARIABLE checkout_root
		OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(checkout_result EQUAL 0)
	file(REAL_PATH "${X265}/src" expected_root)
	file(REAL_PATH "${checkout_root}" checkout_root)
endif()
if(NOT checkout_result EQUAL 0 OR NOT checkout_root STREQUAL expected_root)
	message(STATUS "SKIPPED: x265 source is not a Git checkout (release archive)")
	return()
endif()
file(MAKE_DIRECTORY "${WORK}/origin/source/cmake")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${X265}/src"
	show HEAD:source/cmake/Version.cmake
	OUTPUT_FILE "${WORK}/origin/source/cmake/Version.cmake" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${X265}/src" show HEAD:x265Version.txt
	OUTPUT_FILE "${WORK}/origin/x265Version.txt" COMMAND_ERROR_IS_FATAL ANY)

function(run_git)
	execute_process(COMMAND "${GIT_EXECUTABLE}" ${ARGN}
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "Git failed: ${ARGN}\n${output}\n${error}")
	endif()
endfunction()

run_git(-C "${WORK}/origin" init)
run_git(-C "${WORK}/origin" add -- source/cmake/Version.cmake x265Version.txt)
run_git(-C "${WORK}/origin" -c user.name=ConfigurationTest
	-c user.email=configuration@example.invalid -c commit.gpgsign=false
	commit -m fixture)
run_git(clone --depth 1 "file://${WORK}/origin" "${WORK}/shallow")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK}/shallow"
	rev-parse --is-shallow-repository
	RESULT_VARIABLE result OUTPUT_VARIABLE shallow OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0 OR NOT shallow STREQUAL "true")
	message(FATAL_ERROR "Fixture must be a shallow repository")
endif()

file(WRITE "${WORK}/probe.cmake" [=[
cmake_minimum_required(VERSION 3.21)
set(GIT_ARCHETYPE 1)
include("${VERSION_SCRIPT}")
string(REGEX MATCHALL "([0-9]+)" VERSION_LIST "${X265_LATEST_TAG}")
list(GET VERSION_LIST 0 X265_VERSION_MAJOR)
list(GET VERSION_LIST 1 X265_VERSION_MINOR)
if(NOT "${X265_VERSION_MAJOR}.${X265_VERSION_MINOR}" STREQUAL "4.2"
	OR NOT X265_VERSION STREQUAL "4.2"
	OR NOT X265_TAG_DISTANCE STREQUAL "0"
	OR NOT X265_REVISION_ID STREQUAL "e444744c0")
	message(FATAL_ERROR "Expected the pinned x265 4.2 release metadata")
endif()
if(NOT GIT_ARCHETYPE STREQUAL "1")
	message(FATAL_ERROR "Explicit archive selection was overwritten")
endif()
]=])
set(VERSION_SCRIPT "${WORK}/shallow/source/cmake/Version.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DVERSION_SCRIPT=${VERSION_SCRIPT}"
	-P "${WORK}/probe.cmake" WORKING_DIRECTORY "${WORK}/shallow/source"
	RESULT_VARIABLE original_result OUTPUT_VARIABLE original_output
	ERROR_VARIABLE original_error)
if(original_result EQUAL 0 OR NOT original_error MATCHES "list GET given empty list")
	message(FATAL_ERROR "Original script did not reproduce the empty version list\n${original_output}\n${original_error}")
endif()
run_git(-C "${WORK}/shallow" apply "${X265}/0002-respect-archive-version.patch")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DVERSION_SCRIPT=${VERSION_SCRIPT}"
	-P "${WORK}/probe.cmake" WORKING_DIRECTORY "${WORK}/shallow/source"
	RESULT_VARIABLE patched_result OUTPUT_VARIABLE patched_output
	ERROR_VARIABLE patched_error)
if(NOT patched_result EQUAL 0)
	message(FATAL_ERROR "Patched version script failed\n${patched_output}\n${patched_error}")
endif()
file(REMOVE_RECURSE "${WORK}/origin" "${WORK}/shallow")
file(REMOVE "${WORK}/probe.cmake")
message(STATUS "x265 shallow checkout: original fails, explicit archive version succeeds after patch")
