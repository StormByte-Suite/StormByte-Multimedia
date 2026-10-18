option(WITH_NONFREE "Enable nonfree codecs in FFmpeg build" OFF)
option(WITH_GPL "Enable GPL components in FFmpeg build" OFF)
if(NOT DEFINED WITH_FFMPEG)
	if(WIN32)
		set(WITH_FFMPEG "BUNDLED" CACHE STRING "Use FFmpeg libraries (SYSTEM or BUNDLED)" FORCE)
	else()
		set(WITH_FFMPEG "BUNDLED" CACHE STRING "Use FFmpeg libraries (SYSTEM or BUNDLED)")
	endif()
endif()

# Private link used by the shared host and by StormByte-Multimedia-Test.
# Static FFmpeg is six archives, not one merged pack. ld.bfd scans each
# archive once, so Linux wraps those six in $<LINK_GROUP:RESCAN>
# (--start-group). ld64 and lld-link already rescan, and CMake's RESCAN
# feature is not available there (on Apple it is not --start-group).
# Plugins stay after the group, the same place they sat after the pack.
# Do not WHOLE the six: framepool.c is in both libswscale and libavfilter.
# The imported archives (ffmpeg__bm__*) exist only after graph finalize,
# which is after test/CMakeLists.txt runs, so early callers are recorded
# and attached from the graph hook.
function(stormbyte_ffmpeg_absorb target)
	# Hide symbols contributed by static archives so their PC-relative
	# relocations resolve locally. No gc-sections / -dead_strip / /OPT:REF —
	# they can drop live objects. Frameworks stay PRIVATE.
	if(NOT DEFINED FFMPEG_BUNDLED_TYPE OR NOT FFMPEG_BUNDLED_TYPE STREQUAL "static")
		return()
	endif()
	if(APPLE)
		foreach(_fw IN ITEMS
			VideoToolbox
			AudioToolbox
			CoreMedia
			CoreVideo
			CoreFoundation
		)
			find_library(_SB_MM_FW_${_fw} "${_fw}" REQUIRED)
			target_link_libraries(${target} PRIVATE "${_SB_MM_FW_${_fw}}")
		endforeach()
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_link_options(${target} PRIVATE
			"-Wl,--exclude-libs,ALL"
			"-Wl,--no-undefined"
		)
	endif()
endfunction()

function(_sb_mm_attach_ffmpeg target visibility)
	if(NOT FFMPEG_BUNDLED_TYPE STREQUAL "static")
		target_link_libraries(${target} ${visibility} ffmpeg)
		return()
	endif()
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		# Not the ffmpeg INTERFACE target: CMake ignores LINK_GROUP on
		# INTERFACE items. These are the STATIC IMPORTED archives.
		set(_libs
			ffmpeg__bm__avutil
			ffmpeg__bm__avcodec
			ffmpeg__bm__avformat
			ffmpeg__bm__swscale
			ffmpeg__bm__swresample
			ffmpeg__bm__avfilter
		)
		foreach(_lib IN LISTS _libs)
			if(NOT TARGET "${_lib}")
				message(FATAL_ERROR
					"stormbyte_link_ffmpeg: '${_lib}' does not exist yet")
			endif()
		endforeach()
		list(JOIN _libs "," _csv)
		# Plugins are a second item, outside the group, as they were
		# outside the pack. Linking ffmpeg itself would also pull the
		# six archives ungrouped and CMake would fold that back into
		# the group.
		target_link_libraries(${target} ${visibility}
			"$<LINK_GROUP:RESCAN,${_csv}>"
			ffmpeg-plugins
		)
		if(TARGET ffmpeg_install)
			add_dependencies(${target} ffmpeg_install)
		endif()
		return()
	endif()
	# Windows / macOS: the ffmpeg INTERFACE already carries the six
	# archives, the plugins and the platform LINK= libraries.
	target_link_libraries(${target} ${visibility} ffmpeg)
endfunction()

function(stormbyte_link_ffmpeg target)
	set(_vis PRIVATE)
	if(ARGC GREATER 1)
		set(_vis "${ARGV1}")
	endif()
	if(WITH_FFMPEG STREQUAL "SYSTEM" OR NOT FFMPEG_BUNDLED_TYPE STREQUAL "static")
		target_link_libraries(${target} ${_vis} ffmpeg)
		stormbyte_ffmpeg_absorb("${target}")
	elseif(TARGET ffmpeg__bm__avutil)
		_sb_mm_attach_ffmpeg("${target}" "${_vis}")
		stormbyte_ffmpeg_absorb("${target}")
	else()
		set_property(GLOBAL APPEND PROPERTY _SB_MM_FFMPEG_PENDING "${target}")
		set_property(GLOBAL APPEND PROPERTY _SB_MM_FFMPEG_PENDING_VIS "${_vis}")
	endif()
endfunction()

if(WITH_FFMPEG STREQUAL "SYSTEM")
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
	find_package(FFmpeg REQUIRED)
	add_library(ffmpeg INTERFACE)
	target_link_libraries(ffmpeg INTERFACE
		FFmpeg::avutil
		FFmpeg::avcodec
		FFmpeg::avformat
		FFmpeg::swscale
		FFmpeg::swresample
		FFmpeg::avfilter
	)
	buildmaster_link(StormByte-Multimedia ffmpeg)
else()
	buildmaster_message(STATUS "Setting up bundled FFmpeg" 1)

	set(FFMPEG_BUNDLED_TYPE "static" CACHE STRING "Build FFmpeg as static or shared library (static or shared)")
	if(MSVC)
		set(FFMPEG_BUNDLED_TYPE "static" CACHE STRING "Build FFmpeg as static or shared library (static or shared)" FORCE)
	endif()

	if(NOT FFMPEG_BUNDLED_TYPE STREQUAL "static" AND NOT FFMPEG_BUNDLED_TYPE STREQUAL "shared")
		buildmaster_message(FATAL "FFMPEG_BUNDLED_TYPE must be either 'static' or 'shared'")
	endif()

	set(FFMPEG_MESON_OPTIONS
		asm=enabled
		inline_asm=enabled
		checkasm=disabled # This is only the check_asm test binary
		programs=disabled
		tests=disabled
		avdevice=disabled
		devices=disabled
		iconv=disabled
		alsa=disabled
		libpulse=disabled
		libjack=disabled
		libv4l2=disabled
		xlib=disabled
		libxcb=disabled
		libdrm=disabled
		vaapi=disabled
		vaapi_drm=disabled
		vaapi_x11=disabled
		deinterlace_vaapi_filter=disabled
		denoise_vaapi_filter=disabled
		overlay_vaapi_filter=disabled
		procamp_vaapi_filter=disabled
		scale_vaapi_filter=disabled
		sharpness_vaapi_filter=disabled
		tonemap_vaapi_filter=disabled
		transpose_vaapi_filter=disabled
		hstack_vaapi_filter=disabled
		vstack_vaapi_filter=disabled
		xstack_vaapi_filter=disabled
		pad_vaapi_filter=disabled
		drawbox_vaapi_filter=disabled
		vdpau=disabled
		vdpau_x11=disabled
		cuvid=disabled
		ff_cuda_nvcc=disabled
		ff_cuda_sdk=disabled
		libmfx=disabled
		libvpl=disabled
		rkmpp=disabled
		mmal=disabled
		omx=disabled
		omx_rpi=disabled
		mediandk=disabled
		mediafoundation=disabled
		opencl=disabled
		opengl=disabled
		sdl2=disabled
		vulkan=disabled
		libsrt=disabled
		librtmp=disabled
		libssh=disabled
		libtensorflow=disabled
		nanosleep=disabled
	)

	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
		list(APPEND FFMPEG_MESON_OPTIONS x86asm=enabled)
	endif()

	if(APPLE)
		list(APPEND FFMPEG_MESON_OPTIONS
			videotoolbox=enabled
			audiotoolbox=enabled
			avfoundation=disabled
			appkit=disabled
			metal=disabled
			securetransport=disabled
		)
	else()
		list(APPEND FFMPEG_MESON_OPTIONS
			appkit=disabled
			avfoundation=disabled
			videotoolbox=disabled
			audiotoolbox=disabled
			metal=disabled
			securetransport=disabled
		)
	endif()

	if(WITH_GPL)
		list(APPEND FFMPEG_MESON_OPTIONS gpl=enabled version3=enabled)
	else()
		list(APPEND FFMPEG_MESON_OPTIONS gpl=disabled version3=disabled)
	endif()

	if(WITH_NONFREE)
		list(APPEND FFMPEG_MESON_OPTIONS nonfree=enabled)
	else()
		list(APPEND FFMPEG_MESON_OPTIONS nonfree=disabled)
	endif()

	add_subdirectory(plugins)

	list(APPEND FFMPEG_MESON_OPTIONS ${FFMPEG_PLUGIN_OPTIONS})

	set(FFMPEG_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${FFMPEG_SRC_DIR}"
	)

	set(FFMPEG_BM_OPTS "REQUIRE_TOOL=pkgconfig")
	set(FFMPEG_PATCHES
		"${CMAKE_CURRENT_LIST_DIR}/0001-mov-read-track-name-as-raw-utf8.patch"
	)
	list(APPEND FFMPEG_BM_OPTS
		"GIT={RESET;PATCH={${FFMPEG_PATCHES}};TITLE=FFmpeg compatibility patches}"
	)
	if(WIN32)
		list(APPEND FFMPEG_BM_OPTS "TOOLCHAIN=${STORMBYTE_WINDOWS_TOOLCHAIN}")
		list(APPEND FFMPEG_BM_OPTS "LINK={ws2_32;secur32;bcrypt;ncrypt;crypt32}")
	elseif(APPLE)
		list(APPEND FFMPEG_BM_OPTS "LINK=iconv")
	endif()
	# Do not WHOLE the six archives. FFmpeg 9 ships framepool.c in both
	# libswscale and libavfilter; --whole-archive pulls both. Linux
	# rescans them as a link group; other linkers rescan on their own.
	if(CMAKE_INTERPROCEDURAL_OPTIMIZATION
			OR (CMAKE_BUILD_TYPE STREQUAL "Release"
				AND CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE))
		list(APPEND FFMPEG_BM_OPTS "IPO=fat")
	endif()

	list(JOIN FFMPEG_BM_OPTS ";" FFMPEG_BM_OPTSTR)

	buildmaster_component(
		ffmpeg
		"FFmpeg"
		"${FFMPEG_SRC_DIR}"
		"${FFMPEG_MESON_OPTIONS}"
		"${FFMPEG_BUNDLED_TYPE}"
		"avutil;avcodec;avformat;swscale;swresample;avfilter"
		"${FFMPEG_BM_OPTSTR}"
	)
	buildmaster_group_add(ff-plugins ffmpeg)
	buildmaster_link(ffmpeg ffmpeg-plugins)

	# Shared Multimedia absorbs the static archives (or NEEDs the shared
	# FFmpeg) as PRIVATE, so tests do not inherit them. The test library
	# calls stormbyte_link_ffmpeg itself. Depend only orders the install;
	# the hook attaches the link once the imported archives exist.
	# Static Multimedia publishes the same link as PUBLIC so a consumer
	# of the .a still sees the six archives.
	buildmaster_depend(StormByte-Multimedia ffmpeg)
	if(NOT BUILD_SHARED_LIBS)
		stormbyte_link_ffmpeg(StormByte-Multimedia PUBLIC)
	endif()
	function(_sb_mm_link_ffmpeg_private)
		if(BUILD_SHARED_LIBS)
			stormbyte_link_ffmpeg(StormByte-Multimedia)
		endif()
		get_property(_pending GLOBAL PROPERTY _SB_MM_FFMPEG_PENDING)
		get_property(_pending_vis GLOBAL PROPERTY _SB_MM_FFMPEG_PENDING_VIS)
		set(_i 0)
		foreach(_t IN LISTS _pending)
			list(GET _pending_vis ${_i} _v)
			math(EXPR _i "${_i} + 1")
			_sb_mm_attach_ffmpeg("${_t}" "${_v}")
			stormbyte_ffmpeg_absorb("${_t}")
		endforeach()
	endfunction()
	buildmaster_hook_graph(_sb_mm_link_ffmpeg_private zz-ffmpeg-private-link)

	function(_stormbyte_ffmpeg_announce_build)
		buildmaster_message(STATUS "Setting up FFmpeg build" 1)
		if(NOT FFMPEG_PLUGIN_OPTIONS)
			return()
		endif()
		buildmaster_message(STATUS "Enabled codec options:" 2)
		list_to_columns(_ff_opts_fmt "\t\t\t" 36 ${FFMPEG_PLUGIN_OPTIONS})
		if(_ff_opts_fmt)
			string(REGEX REPLACE "\n$" "" _ff_opts_fmt "${_ff_opts_fmt}")
			string(REPLACE "\n" ";" _ff_opts_lines "${_ff_opts_fmt}")
			foreach(_ff_opts_line IN LISTS _ff_opts_lines)
				if(NOT _ff_opts_line STREQUAL "")
					message(STATUS "${_ff_opts_line}")
				endif()
			endforeach()
		endif()
	endfunction()
	buildmaster_hook_graph(_stormbyte_ffmpeg_announce_build zz-ffmpeg-announce
		CAPTURE FFMPEG_PLUGIN_OPTIONS
	)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/ffmpeg.cmake)
export_ffmpeg_version()
if(WITH_FFMPEG STREQUAL "SYSTEM")
	if(STORMBYTE_FFMPEG_VERSION VERSION_LESS "9.0.0")
		message(FATAL_ERROR
			"WITH_FFMPEG=SYSTEM requires FFmpeg >= 9.0.0 (found ${STORMBYTE_FFMPEG_VERSION})")
	endif()
endif()
prepare_ffmpeg_filter_header()
