plugin_require(WITH_GPL "libx265=disabled libx265_encoder=disabled")

set(X265_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

set(X265_NASM "")
set(X265_NASM_PIC "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	set(X265_NASM "CMAKE_ASM_NASM_FLAGS=-w-pp-macro-params-legacy")
	set(X265_NASM_PIC "CMAKE_ASM_NASM_FLAGS=-DPIC -w-pp-macro-params-legacy")
endif()

set(X265_CET ON)
if(APPLE)
	set(X265_CET OFF)
endif()

set(X265_COMMON
	ENABLE_CET=${X265_CET}
	ENABLE_PIC=ON
	ENABLE_SVT_HEVC=OFF
	ENABLE_LIBVMAF=OFF
	ENABLE_LIBNUMA=OFF
	GIT_ARCHETYPE=1
	ENABLE_CLI=OFF
	"LIB_INSTALL_DIR=${CMAKE_INSTALL_LIBDIR}"
)

set(X265_LIB12 "${CMAKE_CURRENT_BINARY_DIR}/bm/x265-12bit/libx265.a")
if(WIN32)
	set(X265_LIB12 "${CMAKE_CURRENT_BINARY_DIR}/bm/x265-12bit/x265.lib")
endif()

set(X265_OPT_12
	${X265_COMMON}
	ENABLE_SHARED=OFF
	HIGH_BIT_DEPTH=ON
	MAIN12=ON
	EXPORT_C_API=OFF
)
if(X265_NASM)
	list(APPEND X265_OPT_12 "${X265_NASM}")
endif()

set(X265_OPT_10
	${X265_COMMON}
	ENABLE_SHARED=OFF
	ENABLE_HDR10_PLUS=ON
	HIGH_BIT_DEPTH=ON
	MAIN12=OFF
	EXPORT_C_API=OFF
)
if(X265_NASM)
	list(APPEND X265_OPT_10 "${X265_NASM}")
endif()

if(FFMPEG_BUNDLED_TYPE STREQUAL "shared")
	set(X265_8_SHARED ON)
else()
	set(X265_8_SHARED OFF)
endif()

set(X265_OPT_8
	${X265_COMMON}
	"ENABLE_SHARED=${X265_8_SHARED}"
	HIGH_BIT_DEPTH=OFF
	EXPORT_C_API=ON
	LINKED_10BIT=ON
	LINKED_12BIT=ON
	"EXTRA_LIB=${X265_LIB12}"
	"EXTRA_LINK_FLAGS=-L${CMAKE_CURRENT_BINARY_DIR}/bm/x265-12bit -L${CMAKE_CURRENT_BINARY_DIR}/bm/x265-10bit"
)
if(X265_NASM_PIC)
	list(APPEND X265_OPT_8 "${X265_NASM_PIC}")
endif()

set(X265_TC "")
if(MSVC)
	set(X265_TC "TOOLCHAIN=msvc")
endif()

# The multilib variants define x265_analysis_distortion_data differently
# (notably sse_t* ctuDistortion). Combining their IR violates the ODR, as
# diagnosed by GCC LTO. Keep all bit depths outside IPO on every compiler;
# native optimization and assembly remain enabled.
set(X265_12_OPTSTR "SOURCE=source;NOINSTALL;IPO=off;GIT={RESET;PATCH=${CMAKE_CURRENT_LIST_DIR}/0001-json11-cstdint.patch;TITLE=json11 cstdint}")
set(X265_10_OPTSTR "SOURCE=source;NOINSTALL;IPO=off")
set(X265_8_OPTSTR "SOURCE=source;IPO=off")
if(FFMPEG_BUNDLED_TYPE STREQUAL "static")
	set(X265_8_OPTSTR "${X265_8_OPTSTR};REPACK")
endif()
if(NOT X265_TC STREQUAL "")
	set(X265_12_OPTSTR "${X265_12_OPTSTR};${X265_TC}")
	set(X265_10_OPTSTR "${X265_10_OPTSTR};${X265_TC}")
	set(X265_8_OPTSTR "${X265_8_OPTSTR};${X265_TC}")
endif()

buildmaster_group(ff-x265 "X265")

buildmaster_component(
	x265-12bit
	"X265 codec (12-bit)"
	"${X265_SRC_DIR}"
	"${X265_OPT_12}"
	static
	"x265"
	"${X265_12_OPTSTR}"
)
buildmaster_component(
	x265-10bit
	"X265 codec (10-bit)"
	"${X265_SRC_DIR}"
	"${X265_OPT_10}"
	static
	"x265"
	"${X265_10_OPTSTR}"
)
buildmaster_component(
	x265
	"X265 codec"
	"${X265_SRC_DIR}"
	"${X265_OPT_8}"
	"${FFMPEG_BUNDLED_TYPE}"
	"x265"
	"${X265_8_OPTSTR}"
)

buildmaster_depend(x265 x265-12bit)
buildmaster_depend(x265 x265-10bit)

buildmaster_group_add(ff-x265 x265-12bit x265-10bit x265)
buildmaster_group_add(ff-video ff-x265)
register_plugin(x265 "libx265=enabled libx265_encoder=enabled")
