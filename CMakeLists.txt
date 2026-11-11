cmake_minimum_required(VERSION 3.20)
project(MT2_Multiplayer LANGUAGES CXX)
# mod.json holds the only Multiplayer version.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mod.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" mod_json)
string(JSON MT2MP_BUILD_VERSION GET "${mod_json}" version)
message(STATUS "Multiplayer version ${MT2MP_BUILD_VERSION}")
set_property(SOURCE src/core/config.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    "MT2MP_BUILD_VERSION=\"${MT2MP_BUILD_VERSION}\"")
# Keep version changes out of every other compile command and the shared PCH.
set_source_files_properties(src/core/config.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # The combined native adapters exceed the standard COFF section limit.
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/game/npc_probe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/management_runtime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/ui_move_runtime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/ui_region_runtime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/typed_action/action_session.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/typed_action_runtime.cpp"
        PROPERTIES COMPILE_OPTIONS /bigobj)
endif()

# Force x64
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64-bit builds are supported")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
include(cmake/framework_pin.cmake)
include(cmake/framework_plugin.cmake)
