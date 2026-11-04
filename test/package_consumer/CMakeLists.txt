cmake_minimum_required(VERSION 3.28)
project(StormByteMultimediaPackageConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(PREPEND CMAKE_MODULE_PATH
	"${MULTIMEDIA_PREFIX}/share/cmake/Modules"
	"${DEPENDENCY_PREFIX}/share/cmake/Modules"
)
list(PREPEND CMAKE_PREFIX_PATH "${MULTIMEDIA_PREFIX}" "${DEPENDENCY_PREFIX}")
set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)

find_package(StormByte REQUIRED COMPONENTS Multimedia)
find_package(StormByte REQUIRED COMPONENTS Base Buffer Logger Multimedia System)

foreach(path IN ITEMS "${STORMBYTE_Multimedia_INCLUDE_DIR}" "${STORMBYTE_Multimedia_LIBRARY}")
	cmake_path(IS_PREFIX MULTIMEDIA_PREFIX "${path}" NORMALIZE _in_multimedia_prefix)
	if(NOT _in_multimedia_prefix)
		message(FATAL_ERROR "Multimedia was not found in ${MULTIMEDIA_PREFIX}: ${path}")
	endif()
endforeach()
foreach(path IN ITEMS
		"${STORMBYTE_LIBRARY}"
		"${STORMBYTE_Buffer_LIBRARY}"
		"${STORMBYTE_Logger_LIBRARY}"
		"${STORMBYTE_System_LIBRARY}")
	cmake_path(IS_PREFIX DEPENDENCY_PREFIX "${path}" NORMALIZE _in_dependency_prefix)
	if(NOT _in_dependency_prefix)
		message(FATAL_ERROR "StormByte dependency was not found in ${DEPENDENCY_PREFIX}: ${path}")
	endif()
endforeach()

add_executable(multimedia_package_consumer consumer.cxx)
target_link_libraries(multimedia_package_consumer PRIVATE StormByte::Multimedia)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(multimedia_package_consumer PRIVATE -Wno-changes-meaning)
endif()
set_target_properties(multimedia_package_consumer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/bin>"
)
