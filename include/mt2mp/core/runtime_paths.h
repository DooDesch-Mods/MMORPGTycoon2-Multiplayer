#pragma once
#include "mt2mp/core/config.h"
#include <filesystem>
#include <stdexcept>

namespace mt2mp::runtime_paths {
inline std::filesystem::path diagnostics(const std::filesystem::path& runtime) {
    const auto& config=Config::instance();
    if(config.framework_profile_verified){
        const auto& value=config.framework_profile_directory;
        const auto profile=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));
        if(!profile.is_absolute())throw std::runtime_error("Framework diagnostic profile path unavailable");
        return profile/L"diagnostics/multiplayer";
    }
    return runtime/L"diagnostics";
}
}
