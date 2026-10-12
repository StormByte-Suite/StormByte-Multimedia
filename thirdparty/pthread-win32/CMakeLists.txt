set(PTHREAD_WIN32_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PTHREAD_WIN32_COMPONENT "pthread Win32 library")
set(PTHREAD_WIN32_PATCHES
	"${CMAKE_CURRENT_LIST_DIR}/0001-cmake-output-name-pthread.patch"
)

target_include_directories(StormByte-Multimedia BEFORE PRIVATE
	"${PTHREAD_WIN32_SRC_DIR}"
)

buildmaster_component(
	pthread-win32
	"${PTHREAD_WIN32_COMPONENT}"
	"${PTHREAD_WIN32_SRC_DIR}"
	""
	"${FFMPEG_BUNDLED_TYPE}"
	"pthread"
	"GIT={RESET;PATCH={${PTHREAD_WIN32_PATCHES}};TITLE=pthread unversioned name}"
)

