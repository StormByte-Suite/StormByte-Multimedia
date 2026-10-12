install(TARGETS StormByte-Multimedia
	ARCHIVE 		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	LIBRARY 		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	RUNTIME 		DESTINATION ${CMAKE_INSTALL_BINDIR}
)
set(_filter_header_exclusions "")
foreach(_filter IN LISTS STORMBYTE_MULTIMEDIA_DISABLED_FILTERS)
	list(APPEND _filter_header_exclusions
		PATTERN "pipeline/filters/${_filter}.h" EXCLUDE
		PATTERN "pipeline/filters/${_filter}.hxx" EXCLUDE
	)
endforeach()
install(DIRECTORY "${CMAKE_SOURCE_DIR}/lib/public/StormByte/"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/StormByte"
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hxx"
	PATTERN "pipeline/route.hxx" EXCLUDE
	${_filter_header_exclusions}
)
if(ENABLE_ADVANCED_INSTALL)
	install(DIRECTORY "${CMAKE_SOURCE_DIR}/lib/advanced/StormByte/"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/StormByte"
		FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hxx"
	)
endif()