# The Framework owns hooks, input, rendering and the PhysFS profile.
# This target links the Multiplayer logic without a second loader or MinHook runtime.
set(MT2_FRAMEWORK_SDK "" CACHE PATH "Framework SDK source tree with native ownership services")
foreach(header include/mt2fw/plugin.h adapters/mt2/sdk/native_services.h adapters/mt2/sdk/native_routes.h adapters/mt2/sdk/native_ownership.h)
    if(NOT EXISTS "${MT2_FRAMEWORK_SDK}/${header}")
        message(FATAL_ERROR "Framework SDK header is required: ${MT2_FRAMEWORK_SDK}/${header}")
    endif()
endforeach()
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The Multiplayer Framework plugin requires Windows x64")
endif()
enable_language(ASM_MASM)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/vendor/imgui")
add_library(mp_imgui STATIC
    "${IMGUI_DIR}/imgui.cpp" "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp" "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(mp_imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/vendor/SDL3/include")
target_link_libraries(mp_imgui PUBLIC opengl32
    "${CMAKE_CURRENT_SOURCE_DIR}/include/vendor/SDL3/lib/x64/SDL3.lib")
file(GLOB_RECURSE MP_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp src/hooks/*.cpp src/net/*.cpp src/sync/*.cpp src/overlay/*.cpp src/game/*.cpp)
add_library(MT2MultiplayerPlugin SHARED ${MP_SOURCES}
    src/game/object_identity/native_lease/native_free/native_free_bridges.asm)
target_include_directories(MT2MultiplayerPlugin PRIVATE include src/game
    include/vendor/steam include/vendor/minhook/include
    "${MT2_FRAMEWORK_SDK}/include" "${MT2_FRAMEWORK_SDK}/adapters/mt2/sdk")
target_compile_definitions(MT2MultiplayerPlugin PRIVATE MT2MP_FRAMEWORK_PLUGIN
    MT2MP_EXPORTS MT2MP_BUILD_VERSION="${MT2MP_BUILD_VERSION}" NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MT2MultiplayerPlugin PRIVATE mp_imgui opengl32 ws2_32 bcrypt version)
if(MSVC)
    target_compile_options(MT2MultiplayerPlugin PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:/MP2>" "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    target_precompile_headers(MT2MultiplayerPlugin PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/vendor/nlohmann/json.hpp>")
endif()
option(MT2MP_BUILD_TESTS "Build tests" ON)
if(MT2MP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(ci)
endif()
