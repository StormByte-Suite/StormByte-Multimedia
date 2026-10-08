add_executable(MultimediaRegistryTests registry_test.cxx)
target_link_libraries(MultimediaRegistryTests PRIVATE multimedia_test_support)

function(add_registry_test test_name)
	add_test(NAME "registry.${test_name}" COMMAND MultimediaRegistryTests "${test_name}")
	set_tests_properties("registry.${test_name}" PROPERTIES TIMEOUT 30 LABELS "multimedia;registry")
endfunction()

add_registry_test(test_matroska_extensions_resolve_to_same_container)
add_registry_test(test_mov_extensions_resolve_to_same_container)
add_registry_test(test_mp4_extensions_resolve_to_same_container)
add_registry_test(test_mp4_registry_exposes_read_and_write_access)
add_registry_test(test_codec_names_resolve_by_public_identity)
