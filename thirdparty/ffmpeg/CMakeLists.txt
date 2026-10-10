option(WITH_NONFREE "Enable nonfree codecs in FFmpeg build" OFF)
option(WITH_GPL "Enable GPL components in FFmpeg build" OFF)
if(NOT DEFINED WITH_FFMPEG)
	if(WIN32)
		set(WITH_FFMPEG "BUNDLED" CACHE STRING "Use FFmpeg libraries (SYSTEM or BUNDLED)" FORCE)
	else()
		set(WITH_FFMPEG "BUNDLED" CACHE STRING "Use FFmpeg libraries (SYSTEM or BUNDLED)")
	endif()
endif()

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
		ffnvcodec=disabled
		nvenc=disabled
		nvdec=disabled
		cuda=disabled
		cuvid=disabled
		ff_cuda_nvcc=disabled
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

	set(FFMPEG_BM_OPTS "REQUIRE_TOOL=pkgconfig")
	list(APPEND FFMPEG_BM_OPTS
		"GIT={RESET;PATCH=${CMAKE_CURRENT_LIST_DIR}/0001-mov-read-track-name-as-raw-utf8.patch;PATCH=${CMAKE_CURRENT_LIST_DIR}/0002-avcodec-pthread-frame-sync-decoded-side-data.patch;PATCH=${CMAKE_CURRENT_LIST_DIR}/0003-avcodec-hevc-drop-remaining-slices-of-skipped-picture.patch;TITLE=FFmpeg compatibility patches}"
	)
	if(WIN32)
		list(APPEND FFMPEG_BM_OPTS "TOOLCHAIN=msvc")
	endif()
	# Do not WHOLE the six archives. FFmpeg 9 ships framepool.c in both
	# libswscale and libavfilter; --whole-archive pulls both. Static
	# builds merge them below so bfd rescans one archive.
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

	# Static: one rescanned archive plus WHOLE plugins. Shared: the
	# component INTERFACE already carries the plugins. LINK= stays on
	# the pack because REPACK does not copy it from ffmpeg.
	if(FFMPEG_BUNDLED_TYPE STREQUAL "static")
		set(_FF_PACK_OPTS "REPACK")
		if(WIN32)
			list(APPEND _FF_PACK_OPTS "LINK={ws2_32;secur32;bcrypt;ncrypt;crypt32}")
		elseif(APPLE)
			list(APPEND _FF_PACK_OPTS "LINK=iconv")
		endif()
		list(JOIN _FF_PACK_OPTS ";" _FF_PACK_OPTSTR)
		buildmaster_meta(ffmpeg-pack "FFmpeg static pack" "${_FF_PACK_OPTSTR}")
		buildmaster_meta_add(ffmpeg-pack ffmpeg)
		buildmaster_link(ffmpeg-pack ffmpeg-plugins)
		set(_FF_LINK ffmpeg-pack)
	else()
		set(_FF_LINK ffmpeg)
	endif()

	# Shared Multimedia absorbs the static pack (or NEEDs the shared
	# FFmpeg). buildmaster_link is PUBLIC on a host target, so tests
	# would inherit it. Depend orders the install; the hook attaches
	# the already-filled INTERFACE as PRIVATE.
	if(BUILD_SHARED_LIBS)
		buildmaster_depend(StormByte-Multimedia "${_FF_LINK}")
		function(_sb_mm_link_ffmpeg_private)
			target_link_libraries(StormByte-Multimedia PRIVATE "${_FF_LINK}")
		endfunction()
		buildmaster_hook_graph(_sb_mm_link_ffmpeg_private zz-ffmpeg-private-link
			CAPTURE _FF_LINK
		)
	else()
		buildmaster_link(StormByte-Multimedia "${_FF_LINK}")
	endif()

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

	# Static media dependencies are absorbed into the host DSO. Hide symbols
	# contributed by static archives so their PC-relative relocations resolve
	# locally without binding Multimedia's own public RTTI or host symbols.
	# No gc-sections / -dead_strip / /OPT:REF — they can drop live objects.
	# /FORCE:MULTIPLE: duplicate helpers across the static archives.
	# Frameworks: find_library, already PRIVATE, so tests do not inherit them.
	if(FFMPEG_BUNDLED_TYPE STREQUAL "static")
		if(WIN32)
			target_link_options(StormByte-Multimedia PRIVATE
				"/FORCE:MULTIPLE"
			)
		elseif(APPLE)
			foreach(_fw IN ITEMS
				VideoToolbox
				AudioToolbox
				CoreMedia
				CoreVideo
				CoreFoundation
			)
				find_library(_SB_MM_FW_${_fw} "${_fw}" REQUIRED)
				target_link_libraries(StormByte-Multimedia PRIVATE
					"${_SB_MM_FW_${_fw}}"
				)
			endforeach()
		elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			target_link_options(StormByte-Multimedia PRIVATE
				"-Wl,--exclude-libs,ALL"
				"-Wl,--no-undefined"
			)
		endif()
	endif()
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
