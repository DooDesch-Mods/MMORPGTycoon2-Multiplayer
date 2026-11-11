# The tag in .github/framework-sdk.json is the only Framework version of this repository. It selects the
# SDK checkout (headers and the SDL3, MinHook and nlohmann/json copies under third_party/vendor) and sets
# the minimum Framework runtime the plugin accepts at load time.
include_guard(GLOBAL)
set(MT2_FRAMEWORK_PIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../.github/framework-sdk.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2_FRAMEWORK_PIN_FILE}")
file(READ "${MT2_FRAMEWORK_PIN_FILE}" pin_json)
string(JSON MT2_FRAMEWORK_TAG GET "${pin_json}" tag)
if(NOT MT2_FRAMEWORK_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "Framework pin must be a tag vMAJOR.MINOR.PATCH: ${MT2_FRAMEWORK_TAG}")
endif()
set(MT2_FRAMEWORK_VERSION_DEFINITIONS
    MT2MP_FRAMEWORK_MAJOR=${CMAKE_MATCH_1} MT2MP_FRAMEWORK_MINOR=${CMAKE_MATCH_2} MT2MP_FRAMEWORK_PATCH=${CMAKE_MATCH_3})

set(MT2_FRAMEWORK_SDK "" CACHE PATH "Framework repository checkout at the tag in .github/framework-sdk.json")
foreach(file include/mt2fw/plugin.h adapters/mt2/sdk/native_services.h adapters/mt2/sdk/native_routes.h
        adapters/mt2/sdk/native_ownership.h third_party/vendor/nlohmann/json.hpp
        third_party/vendor/minhook/include/MinHook.h third_party/vendor/SDL3/include/SDL3/SDL.h
        third_party/vendor/SDL3/lib/x64/SDL3.lib)
    if(NOT EXISTS "${MT2_FRAMEWORK_SDK}/${file}")
        message(FATAL_ERROR "Framework SDK file missing: ${MT2_FRAMEWORK_SDK}/${file}; set MT2_FRAMEWORK_SDK to a checkout of ${MT2_FRAMEWORK_TAG}")
    endif()
endforeach()

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${MT2_FRAMEWORK_SDK}" rev-parse HEAD "${MT2_FRAMEWORK_TAG}^{commit}"
        RESULT_VARIABLE sdk_result OUTPUT_VARIABLE sdk_revisions ERROR_VARIABLE sdk_error OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(NOT GIT_FOUND OR sdk_result)
    message(WARNING "Framework SDK revision not verified against ${MT2_FRAMEWORK_TAG} (no git checkout or tag): ${sdk_error}")
else()
    string(REPLACE "\n" ";" sdk_revisions "${sdk_revisions}")
    list(GET sdk_revisions 0 sdk_head)
    list(GET sdk_revisions 1 sdk_tag_commit)
    if(NOT sdk_head STREQUAL sdk_tag_commit)
        message(FATAL_ERROR "Framework SDK checkout is at ${sdk_head}, the pin ${MT2_FRAMEWORK_TAG} is ${sdk_tag_commit}")
    endif()
    message(STATUS "Framework SDK ${MT2_FRAMEWORK_TAG} at ${sdk_head}: ${MT2_FRAMEWORK_SDK}")
endif()
set(MT2_FRAMEWORK_VENDOR "${MT2_FRAMEWORK_SDK}/third_party/vendor")
