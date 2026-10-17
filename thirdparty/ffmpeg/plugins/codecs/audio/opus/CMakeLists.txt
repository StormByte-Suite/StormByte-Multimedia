set(OPUS_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

set(OPUS_DATA_HASH "a5177ec6fb7d15058e99e57029746100121f68e4890b1467d4094aa336b6013e")
if(EXISTS "${OPUS_SRC_DIR}/autogen.sh")
	file(READ "${OPUS_SRC_DIR}/autogen.sh" _autogen_contents)
	string(REGEX MATCH "[0-9a-f]{64}" _match "${_autogen_contents}")
	if(_match)
		set(OPUS_DATA_HASH "${_match}")
	endif()
endif()

if(FFMPEG_BUNDLED_TYPE STREQUAL "static")
	set(OPUS_SHARED_MODE OFF)
else()
	set(OPUS_SHARED_MODE ON)
endif()

set(OPUS_COMPONENT "Opus codec")
set(OPUS_DNN_UNPACK "${BUILDMASTER_BINDIR}/files/opus-dnn")
# Headers included by in-tree sources live in this tree's unpack
# (files/opus-dnn/dnn). ENABLE_PGO configures a second tree and does not
# inherit include paths that point at the other one, so the system include
# is written here. The model .c files are compiled from that unpack by
# 0001-dnn-data-from-unpack.patch. They must not be copied into the git
# checkout: stamp git-cleans it, and both trees share that checkout.
file(TO_CMAKE_PATH "${OPUS_DNN_UNPACK}" _opus_dnn_unpack)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/opus_system_includes.cmake" _opus_inc_cmake)
file(WRITE "${_opus_inc_cmake}"
	"include_directories(BEFORE SYSTEM\n"
	"\t\"${_opus_dnn_unpack}/dnn\"\n"
	"\t\"${_opus_dnn_unpack}\"\n"
	")\n")
set(OPUS_OPTIONS
	"-DCMAKE_PROJECT_INCLUDE=${_opus_inc_cmake}"
	"-DOPUS_BUILD_SHARED_LIBRARY=${OPUS_SHARED_MODE}"
	"-DOPUS_BUILD_TESTING=OFF"
	"-DOPUS_CUSTOM_MODES=ON"
	"-DOPUS_BUILD_PROGRAMS=OFF"
	"-DOPUS_FUZZING=OFF"
	"-DOPUS_CHECK_ASM=OFF"
	"-DOPUS_DISABLE_INTRINSICS=OFF"
	"-DOPUS_INSTALL_PKG_CONFIG_MODULE=ON"
	"-DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF"
	"-DOPUS_DRED=ON"
	"-DOPUS_OSCE=ON"
	"-DOPUS_DNN_DATA_DIR=${OPUS_DNN_UNPACK}"
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	list(APPEND OPUS_OPTIONS "-DOPUS_X86_MAY_HAVE_SSE=ON" "-DOPUS_X86_MAY_HAVE_SSE2=ON"
		"-DOPUS_X86_MAY_HAVE_SSE4_1=ON" "-DOPUS_X86_MAY_HAVE_AVX2=ON"
		"-DOPUS_X86_PRESUME_SSE4_1=OFF" "-DOPUS_X86_PRESUME_AVX2=OFF")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64|arm|armv[0-9].*)$")
	list(APPEND OPUS_OPTIONS "-DOPUS_USE_NEON=ON" "-DOPUS_MAY_HAVE_NEON=ON")
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	list(APPEND OPUS_OPTIONS "-DCMAKE_ASM_NASM_FLAGS=-DPIC -w-pp-macro-params-legacy")
endif()

set(OPUS_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-dnn-data-from-unpack.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-shallow-package-version.patch"
)

buildmaster_component(
	opus
	"${OPUS_COMPONENT}"
	"${OPUS_SRC_DIR}"
	"${OPUS_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"opus"
	"BACKEND=cmake;GIT={RESET;PATCH={${OPUS_PATCHES}};TITLE=Opus compatibility};FILES={URL=https://media.xiph.org/opus/models/opus_data-${OPUS_DATA_HASH}.tar.gz;NAME=opus-dnn;SHA256=${OPUS_DATA_HASH};UNPACK;TITLE=Opus DNN data}"
)
buildmaster_group_add(ff-audio opus)
register_plugin(opus "libopus=enabled")
