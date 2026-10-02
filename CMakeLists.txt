cmake_minimum_required(VERSION 3.12)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("StormByte C++ Library: Multimedia module"
	VERSION 0.0.9999
	DESCRIPTION "StormByte-Multimedia is a StormByte library module for parsing configuration files"
	HOMEPAGE_URL "https://suite.stormbyte.org/StormByte-Multimedia"
)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

include(cmake/optimizations.cmake)
include(cmake/asan.cmake)
cmake_policy(SET CMP0079 NEW)

include(cmake/load_buildmaster.cmake)
add_subdirectory(doc)
add_subdirectory(lib)
add_subdirectory(thirdparty)
add_subdirectory(test)

include(cmake/install.cmake)