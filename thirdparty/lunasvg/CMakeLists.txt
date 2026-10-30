target_include_directories(StormByte-Multimedia BEFORE PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/include"
)
target_compile_definitions(StormByte-Multimedia PRIVATE LUNASVG_BUILD_STATIC)

buildmaster_component(
	lunasvg
	"LunaSVG"
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"-DBUILD_SHARED_LIBS=OFF;-DUSE_SYSTEM_PLUTOVG=OFF;-DLUNASVG_BUILD_EXAMPLES=OFF;-DPLUTOVG_BUILD_EXAMPLES=OFF;-DLUNASVG_DISABLE_LOAD_SYSTEM_FONTS=ON;-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD=ON"
	"static"
	"lunasvg;plutovg"
	"BACKEND=cmake"
)

buildmaster_link(StormByte-Multimedia lunasvg)
