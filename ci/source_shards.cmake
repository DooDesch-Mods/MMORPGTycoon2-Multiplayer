# Export binary source-check targets, including compile-only libraries.
# Extraction-only utilities remain build dependencies of their consumers.
get_property(source_check_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
set(source_check_target_map "")
foreach(target IN LISTS source_check_targets)
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "INTERFACE_LIBRARY" OR kind STREQUAL "UTILITY")
        continue()
    endif()
    if(kind STREQUAL "EXECUTABLE")
        string(APPEND source_check_target_map "${target}\t${kind}\t$<TARGET_FILE:${target}>\n")
    elseif(kind MATCHES "^(STATIC|OBJECT|SHARED|MODULE)_LIBRARY$")
        string(APPEND source_check_target_map "${target}\t${kind}\t-\n")
    else()
        message(FATAL_ERROR "Source-check target ${target} has unmapped build type ${kind}")
    endif()
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/source-check-targets-$<CONFIG>.tsv"
    CONTENT "${source_check_target_map}")
