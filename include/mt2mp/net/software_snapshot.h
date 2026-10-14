#pragma once
#include "mt2mp/game/native_engine.h"
#include "vendor/nlohmann/json.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mt2mp::net {
inline game::native::SoftwareCache parse_initial_software_cache(const nlohmann::json& value) {
    if(!value.is_object()||value.size()!=3||!value.contains("revision")||!value.contains("rating")||!value.contains("fun"))
        throw std::runtime_error("initial software cache requires revision, rating and fun");
    if(!value.at("revision").is_number_integer()||value.at("revision")!=1)
        throw std::runtime_error("initial software revision must be integer 1");
    auto number=[&](const char* name) {
        const auto& item=value.at(name);
        if(!item.is_number()) throw std::runtime_error(std::string("software cache requires numeric ")+name);
        const double raw=item.get<double>();
        if(!std::isfinite(raw)||std::abs(raw)>std::numeric_limits<float>::max())
            throw std::runtime_error(std::string("software cache requires finite float32 ")+name);
        return static_cast<float>(raw);
    };
    game::native::SoftwareCache result{number("rating"),number("fun")};
    if(result.rating<0||result.rating>1000) throw std::runtime_error("software rating requires native range 0..1000");
    return result;
}
}
