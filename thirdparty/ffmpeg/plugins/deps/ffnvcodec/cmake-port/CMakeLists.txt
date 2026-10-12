cmake_minimum_required(VERSION 3.21)
project(ffnvcodec_headers NONE)

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
	set(CMAKE_INSTALL_LIBDIR lib)
endif()
include(GNUInstallDirs)

install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../src/include/ffnvcodec"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

configure_file(ffnvcodec.pc.in ffnvcodec.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ffnvcodec.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)