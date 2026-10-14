cmake_minimum_required(VERSION 3.20)
project(MT2_Multiplayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Force x64
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64-bit builds are supported")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── Vendor: MinHook ──────────────────────────────────────────────
add_subdirectory(include/vendor/minhook EXCLUDE_FROM_ALL)

# ── Vendor: Dear ImGui ───────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/include/vendor/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
# SDL3 headers for ImGui SDL3 backend
target_include_directories(imgui PUBLIC ${CMAKE_SOURCE_DIR}/include/vendor/SDL3/include)
# OpenGL
target_link_libraries(imgui PUBLIC opengl32)
# SDL3 - link against import lib generated from game's SDL3.dll
target_link_libraries(imgui PUBLIC ${CMAKE_SOURCE_DIR}/include/vendor/SDL3/lib/x64/SDL3.lib)

# ── Steamworks SDK headers ───────────────────────────────────────
add_library(steamworks_headers INTERFACE)
target_include_directories(steamworks_headers INTERFACE ${CMAKE_SOURCE_DIR}/include/vendor/steam)

# ── Main mod DLL ─────────────────────────────────────────────────
file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp
    src/hooks/*.cpp
    src/net/*.cpp
    src/sync/*.cpp
    src/overlay/*.cpp
    src/game/*.cpp
)

add_library(MT2_Multiplayer SHARED ${MOD_SOURCES})
enable_language(ASM_MASM)
target_sources(MT2_Multiplayer PRIVATE src/game/object_identity/native_lease/native_free/native_free_bridges.asm)
target_include_directories(MT2_Multiplayer PUBLIC include)
target_include_directories(MT2_Multiplayer PRIVATE src/game)
target_link_libraries(MT2_Multiplayer PRIVATE
    minhook
    imgui
    steamworks_headers
    opengl32
    ws2_32
    bcrypt
    version
    ${CMAKE_SOURCE_DIR}/include/vendor/SDL3/lib/x64/SDL3.lib
)
target_include_directories(MT2_Multiplayer PRIVATE ${CMAKE_SOURCE_DIR}/include/vendor/SDL3/include)
target_compile_definitions(MT2_Multiplayer PRIVATE
    MT2MP_EXPORTS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── Steam API Proxy DLL ─────────────────────────────────────────
# Classic DLL proxy with ASM JMP stubs for all 1096 exports
enable_language(ASM_MASM)
add_library(steam_api64 SHARED
    proxy/proxy_main.cpp
    proxy/local_bootstrap.cpp
    proxy/steam_stubs.asm
    proxy/steam_stubs_init.cpp
    proxy/steam_api64_proxy.def
)
target_compile_definitions(steam_api64 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── Sidecar Relay Process ────────────────────────────────────
add_executable(mt2mp_sidecar
    tools/sidecar/mt2mp_sidecar_main.cpp
)
target_compile_definitions(mt2mp_sidecar PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(mt2mp_sidecar PRIVATE include)
target_link_libraries(mt2mp_sidecar PRIVATE ws2_32)
set_target_properties(mt2mp_sidecar PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ── Tests ────────────────────────────────────────────────────────
option(MT2MP_BUILD_TESTS "Build tests" ON)
if(MT2MP_BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SOURCES test/test_*.cpp)
    foreach(TEST_FILE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_FILE})
        target_include_directories(${TEST_NAME} PRIVATE include)
        target_link_libraries(${TEST_NAME} PRIVATE MT2_Multiplayer)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()
