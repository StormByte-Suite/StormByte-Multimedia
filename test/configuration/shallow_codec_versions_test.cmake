cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED REPO OR NOT DEFINED WORK)
	message(FATAL_ERROR "REPO and WORK are required")
endif()
find_package(Git REQUIRED)
find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
set(CODECS "${REPO}/thirdparty/ffmpeg/plugins/codecs")
file(MAKE_DIRECTORY "${WORK}")

function(run_git)
	execute_process(COMMAND "${GIT_EXECUTABLE}" ${ARGN}
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "Git failed: ${ARGN}\n${output}\n${error}")
	endif()
endfunction()

function(make_shallow name)
	run_git(-C "${WORK}/${name}-origin" init)
	run_git(-C "${WORK}/${name}-origin" add -- .)
	run_git(-C "${WORK}/${name}-origin" -c user.name=ConfigurationTest
		-c user.email=configuration@example.invalid -c commit.gpgsign=false commit -m fixture)
	run_git(clone --depth 1 --no-tags "file://${WORK}/${name}-origin" "${WORK}/${name}")
endfunction()

file(MAKE_DIRECTORY "${WORK}/opus-origin/cmake" "${WORK}/opus-origin/meson")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CODECS}/audio/opus/src"
	show HEAD:cmake/OpusPackageVersion.cmake
	OUTPUT_FILE "${WORK}/opus-origin/cmake/OpusPackageVersion.cmake" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CODECS}/audio/opus/src"
	show HEAD:meson/get-version.py
	OUTPUT_FILE "${WORK}/opus-origin/meson/get-version.py" COMMAND_ERROR_IS_FATAL ANY)
file(COPY "${CODECS}/audio/opus/src/.gitignore" DESTINATION "${WORK}/opus-origin")
make_shallow(opus)
run_git(-C "${WORK}/opus" apply "${CODECS}/audio/opus/0002-shallow-package-version.patch")
run_git(-C "${WORK}/opus" reset --hard)
run_git(-C "${WORK}/opus" clean -fd)
if(EXISTS "${WORK}/opus/stormbyte_package_version")
	message(FATAL_ERROR "Opus fallback must be removed by the BuildMaster reset/clean cycle")
endif()
run_git(-C "${WORK}/opus" apply "${CODECS}/audio/opus/0002-shallow-package-version.patch")
file(WRITE "${WORK}/opus/probe.cmake" [=[
cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/OpusPackageVersion.cmake")
get_package_version(PACKAGE_VERSION PROJECT_VERSION)
if(NOT PACKAGE_VERSION STREQUAL "1.6.1" OR NOT PROJECT_VERSION STREQUAL "1.6.1")
	message(FATAL_ERROR "Opus fallback must preserve package and project version 1.6.1")
endif()
]=])
execute_process(COMMAND "${CMAKE_COMMAND}" -P "${WORK}/opus/probe.cmake"
	WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "Opus CMake shallow version failed")
endif()
execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${WORK}/opus/meson/get-version.py"
	--package-version RESULT_VARIABLE result OUTPUT_VARIABLE version OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0 OR NOT version STREQUAL "1.6.1")
	message(FATAL_ERROR "Opus Meson shallow version failed: ${version}")
endif()

file(MAKE_DIRECTORY "${WORK}/svt-origin/Source/Lib/Codec")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CODECS}/video/svt-av1/src"
	show HEAD:Source/Lib/Codec/ConfigureGitVersion.cmake
	OUTPUT_FILE "${WORK}/svt-origin/Source/Lib/Codec/ConfigureGitVersion.cmake" COMMAND_ERROR_IS_FATAL ANY)
make_shallow(svt)
run_git(-C "${WORK}/svt" apply "${CODECS}/video/svt-av1/0001-shallow-version.patch")
file(WRITE "${WORK}/svt.in" "@PACKAGE_VERSION_STRING@\n")
execute_process(COMMAND "${CMAKE_COMMAND}"
	"-DINPUT_FILE=${WORK}/svt.in" "-DOUTPUT_FILE=${WORK}/svt.out"
	"-DGIT_ROOT_DIR=${WORK}/svt" -DPACKAGE_VERSION_STRING=4.2.0
	-P "${WORK}/svt/Source/Lib/Codec/ConfigureGitVersion.cmake" RESULT_VARIABLE result)
file(READ "${WORK}/svt.out" version)
string(STRIP "${version}" version)
if(NOT result EQUAL 0 OR NOT version MATCHES "^v4\\.2\\.0\\+g[0-9a-f]+$")
	message(FATAL_ERROR "SVT shallow version must preserve semver without dirty: ${version}")
endif()
run_git(-C "${WORK}/svt" tag v4.2.0)
execute_process(COMMAND "${CMAKE_COMMAND}"
	"-DINPUT_FILE=${WORK}/svt.in" "-DOUTPUT_FILE=${WORK}/svt.out"
	"-DGIT_ROOT_DIR=${WORK}/svt" -DPACKAGE_VERSION_STRING=4.2.0
	-P "${WORK}/svt/Source/Lib/Codec/ConfigureGitVersion.cmake" RESULT_VARIABLE result)
file(READ "${WORK}/svt.out" version)
string(STRIP "${version}" version)
if(NOT result EQUAL 0 OR NOT version STREQUAL "v4.2.0")
	message(FATAL_ERROR "SVT must preserve the HEAD release tag: ${version}")
endif()

run_git(clone --depth 1 --no-tags "file://${CODECS}/video/x264/src" "${WORK}/x264")
run_git(-C "${WORK}/x264" apply "${CODECS}/video/x264/0001-shallow-revision.patch")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${WORK}/x264/version.py" --package-version
	RESULT_VARIABLE result OUTPUT_VARIABLE version OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0 OR NOT version STREQUAL "0.165.3223+93")
	message(FATAL_ERROR "x264 must preserve the pinned revision: ${version}")
endif()
file(WRITE "${WORK}/x264/shallow_version.json" "{\"commit\":\"invalid\",\"revision\":\"0\"}\n")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${WORK}/x264/version.py" --package-version
	RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Update shallow_version.json")
	message(FATAL_ERROR "x264 must reject mismatched pinned revision metadata")
endif()

file(REMOVE_RECURSE "${WORK}/opus-origin" "${WORK}/opus" "${WORK}/svt-origin"
	"${WORK}/svt" "${WORK}/x264")
file(REMOVE "${WORK}/svt.in" "${WORK}/svt.out")
message(STATUS "Shallow codec versions: Opus CMake/Meson, SVT semver/tags/no-dirty, x264 revision/pin checks passed")
