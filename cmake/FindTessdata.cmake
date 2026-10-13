# vim: ts=2 sw=2
# Find a system tessdata directory (*.traineddata).
#
# Once done this will define
#  TESSDATA_FOUND            - At least one language model is on disk
#  TESSDATA_DIR              - Directory that contains the models
#  TESSDATA_FILES            - List of *.traineddata paths
#  TESSDATA_LANGUAGE_COUNT   - Number of models
#
# Hints:
#  TESSDATA_PREFIX  - Environment or CMake variable (dir or parent of tessdata/)
#  Tessdata_ROOT    - CMake prefix (share/tessdata under it)

include(FindPackageHandleStandardArgs)

function(_tessdata_models dir out_var)
	set(_files "")
	if(IS_DIRECTORY "${dir}")
		file(GLOB _files "${dir}/*.traineddata")
	endif()
	set(${out_var} "${_files}" PARENT_SCOPE)
endfunction()

set(_tessdata_candidates "")
if(DEFINED TESSDATA_DIR AND NOT TESSDATA_DIR STREQUAL "")
	list(APPEND _tessdata_candidates "${TESSDATA_DIR}")
endif()

if(DEFINED TESSDATA_PREFIX AND NOT TESSDATA_PREFIX STREQUAL "")
	list(APPEND _tessdata_candidates
		"${TESSDATA_PREFIX}"
		"${TESSDATA_PREFIX}/tessdata"
	)
endif()
if(DEFINED ENV{TESSDATA_PREFIX} AND NOT "$ENV{TESSDATA_PREFIX}" STREQUAL "")
	list(APPEND _tessdata_candidates
		"$ENV{TESSDATA_PREFIX}"
		"$ENV{TESSDATA_PREFIX}/tessdata"
	)
endif()
if(DEFINED Tessdata_ROOT AND NOT Tessdata_ROOT STREQUAL "")
	list(APPEND _tessdata_candidates
		"${Tessdata_ROOT}/share/tessdata"
		"${Tessdata_ROOT}/share/tesseract/tessdata"
		"${Tessdata_ROOT}/share/tesseract-ocr/tessdata"
		"${Tessdata_ROOT}/tessdata"
	)
endif()

if(NOT WIN32)
	find_package(PkgConfig)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(PC_TESSERACT QUIET tesseract)
		if(PC_TESSERACT_PREFIX)
			list(APPEND _tessdata_candidates
				"${PC_TESSERACT_PREFIX}/share/tessdata"
				"${PC_TESSERACT_PREFIX}/share/tesseract/tessdata"
				"${PC_TESSERACT_PREFIX}/share/tesseract-ocr/tessdata"
			)
			file(GLOB _pc_ver "${PC_TESSERACT_PREFIX}/share/tesseract-ocr/*/tessdata")
			list(APPEND _tessdata_candidates ${_pc_ver})
		endif()
	endif()
endif()

list(APPEND _tessdata_candidates
	"/usr/share/tessdata"
	"/usr/local/share/tessdata"
	"/usr/share/tesseract/tessdata"
	"/usr/local/share/tesseract/tessdata"
	"/usr/share/tesseract-ocr/tessdata"
	"/usr/local/share/tesseract-ocr/tessdata"
	"/opt/homebrew/share/tessdata"
	"/opt/homebrew/share/tesseract/tessdata"
)
file(GLOB _sys_ver
	"/usr/share/tesseract-ocr/*/tessdata"
	"/usr/local/share/tesseract-ocr/*/tessdata"
	"/opt/homebrew/share/tesseract-ocr/*/tessdata"
)
list(APPEND _tessdata_candidates ${_sys_ver})
list(REMOVE_DUPLICATES _tessdata_candidates)

set(TESSDATA_DIR "")
set(TESSDATA_FILES "")
foreach(_dir IN LISTS _tessdata_candidates)
	_tessdata_models("${_dir}" _models)
	if(_models)
		set(TESSDATA_DIR "${_dir}")
		set(TESSDATA_FILES "${_models}")
		break()
	endif()
endforeach()

set(TESSDATA_LANGUAGE_COUNT 0)
if(TESSDATA_FILES)
	list(LENGTH TESSDATA_FILES TESSDATA_LANGUAGE_COUNT)
endif()

find_package_handle_standard_args(Tessdata
	REQUIRED_VARS TESSDATA_DIR
	FAIL_MESSAGE "No system *.traineddata found. Install language packs or set TESSDATA_PREFIX."
)

mark_as_advanced(TESSDATA_DIR TESSDATA_FILES TESSDATA_LANGUAGE_COUNT)
