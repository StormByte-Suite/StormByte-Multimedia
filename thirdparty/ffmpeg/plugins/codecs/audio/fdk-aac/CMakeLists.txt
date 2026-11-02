plugin_require(WITH_NONFREE "libfdk_aac=disabled")

set(FDK_AAC_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(FDK_AAC_COMPONENT "FDK_AAC codec")
set(FDK_AAC_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-widen-fractional-samples-without-negative-shift.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-log2-avoid-negative-exponent-shift.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0003-pow-avoid-negative-mantissa-shift.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0004-pow2-avoid-negative-integral-part-shift.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0005-scale-avoid-negative-saturating-shifts.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0006-fixmul-avoid-negative-product-shifts.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0007-block-switch-avoid-negative-sample-shifts.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0008-scale-avoid-negative-value-shifts.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0009-band-energy-avoid-negative-spectrum-shifts.patch"
)

set(FDK_AAC_OPTIONS
	"-DBUILD_PROGRAMS=OFF"
	"-DFDK_AAC_INSTALL_CMAKE_CONFIG_MODULE=OFF"
	"-DFDK_AAC_INSTALL_PKGCONFIG_MODULE=ON"
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
	list(APPEND FDK_AAC_OPTIONS "-DCMAKE_ASM_NASM_FLAGS=-DPIC -w-pp-macro-params-legacy")
endif()

buildmaster_component(
	fdk-aac
	"${FDK_AAC_COMPONENT}"
	"${FDK_AAC_SRC_DIR}"
	"${FDK_AAC_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"fdk-aac"
	"GIT={RESET;PATCH={${FDK_AAC_PATCHES}};TITLE=FDK-AAC fixed-point conversion}"
)
buildmaster_group_add(ff-audio fdk-aac)
register_plugin(fdk-aac "libfdk_aac=enabled")
