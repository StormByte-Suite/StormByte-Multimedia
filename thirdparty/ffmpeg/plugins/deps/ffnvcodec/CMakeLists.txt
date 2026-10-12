if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(FFNVCODEC_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
	set(FFNVCODEC_CMAKE_PORT_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake-port")

	target_include_directories(StormByte-Multimedia BEFORE PRIVATE
		"${FFNVCODEC_SRC_DIR}/include"
	)

	buildmaster_component(
		ffnvcodec-headers
		"FFmpeg NVIDIA codec headers"
		"${FFNVCODEC_CMAKE_PORT_DIR}"
		""
		headers
		""
	)
	buildmaster_group_add(ff-deps ffnvcodec-headers)
	register_dependency(ffnvcodec-headers)

	register_builtin_plugin(
		"ffnvcodec=enabled cuda=enabled nvenc=enabled nvdec=disabled cuvid=disabled ff_cuda_nvcc=disabled ff_cuda_llvm=disabled ff_cuda_sdk=disabled av1_nvenc_encoder=enabled h264_nvenc_encoder=enabled hevc_nvenc_encoder=enabled"
	)
else()
	register_builtin_plugin("cuda=disabled cuvid=disabled ffnvcodec=disabled ff_cuda_nvcc=disabled ff_cuda_llvm=disabled ff_cuda_sdk=disabled nvdec=disabled nvenc=disabled av1_nvenc_encoder=disabled h264_nvenc_encoder=disabled hevc_nvenc_encoder=disabled av1_cuvid_decoder=disabled h264_cuvid_decoder=disabled hevc_cuvid_decoder=disabled mjpeg_cuvid_decoder=disabled mpeg1_cuvid_decoder=disabled mpeg2_cuvid_decoder=disabled mpeg4_cuvid_decoder=disabled vc1_cuvid_decoder=disabled vp8_cuvid_decoder=disabled vp9_cuvid_decoder=disabled av1_nvdec_hwaccel=disabled h264_nvdec_hwaccel=disabled hevc_nvdec_hwaccel=disabled mjpeg_nvdec_hwaccel=disabled mpeg1_nvdec_hwaccel=disabled mpeg2_nvdec_hwaccel=disabled mpeg4_nvdec_hwaccel=disabled vc1_nvdec_hwaccel=disabled vp8_nvdec_hwaccel=disabled vp9_nvdec_hwaccel=disabled wmv3_nvdec_hwaccel=disabled bilateral_cuda_filter=disabled bwdif_cuda_filter=disabled chromakey_cuda_filter=disabled colorspace_cuda_filter=disabled hwupload_cuda_filter=disabled libvmaf_cuda_filter=disabled overlay_cuda_filter=disabled pad_cuda_filter=disabled scale_cuda_filter=disabled thumbnail_cuda_filter=disabled transpose_cuda_filter=disabled yadif_cuda_filter=disabled")
endif()
