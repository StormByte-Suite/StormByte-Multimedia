set(VPX_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(VPX_COMPONENT "VPX codec")
set(VPX_PATCH_FILES
	"${CMAKE_CURRENT_LIST_DIR}/0001-Only-add-arm-asm-files-when-target-is-arm.patch"
	"${CMAKE_CURRENT_LIST_DIR}/0002-fix-enable-VP9-high-bit-depth-in-the-libvpx-Meson-po.patch"
)

set(VPX_OPTIONS
	"-Dlibs=enabled"
	"-Dexamples=disabled"
	"-Dtools=disabled"
	"-Ddocs=disabled"
	"-Dinstall_docs=disabled"
	"-Ddecode_perf_tests=disabled"
	"-Dencode_perf_tests=disabled"
	"-Dunit_tests=disabled"
	"-Dvp8_encoder=enabled"
	"-Dvp8_decoder=enabled"
	"-Dvp9_encoder=enabled"
	"-Dvp9_decoder=enabled"
	"-Dvp9_highbitdepth=enabled"
	"-Dmultithread=enabled"
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
	list(APPEND VPX_OPTIONS "-Dcpu=aarch64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|armv7)")
	list(APPEND VPX_OPTIONS "-Dcpu=arm")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
	list(APPEND VPX_OPTIONS
		"-Dcpu=x86_64"
		"-Ddisable_features=neon,neon_asm,sve,sve2,msa"
		"-Dforce_disable_features=neon,neon_asm,arm,arm64,sve,sve2,msa"
	)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
	list(APPEND VPX_OPTIONS
		"-Dcpu=x86"
		"-Ddisable_features=neon,neon_asm,sve,sve2,msa"
		"-Dforce_disable_features=neon,neon_asm,arm,arm64,sve,sve2,msa"
	)
endif()

buildmaster_component(
	vpx
	"${VPX_COMPONENT}"
	"${VPX_SRC_DIR}"
	"${VPX_OPTIONS}"
	"${FFMPEG_BUNDLED_TYPE}"
	"vpx"
	"GIT={RESET;PATCH={${VPX_PATCH_FILES}};TITLE=VPX patches}"
)
buildmaster_group_add(ff-video vpx)
register_plugin(vpx "libvpx=enabled libvpx_vp8_encoder=enabled libvpx_vp8_decoder=enabled libvpx_vp9_encoder=enabled libvpx_vp9_decoder=enabled")
