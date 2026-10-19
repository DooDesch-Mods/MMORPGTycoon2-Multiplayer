#pragma once
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace mt2mp::game::region_history_key {
inline std::optional<int32_t> parse(std::string_view key){
    if(!key.starts_with("region:"))return {};
    int32_t id=-1;const auto first=key.data()+7,last=key.data()+key.size();
    const auto result=std::from_chars(first,last,id);
    if(result.ec!=std::errc{}||result.ptr!=last||id<0||key!="region:"+std::to_string(id))return {};
    return id;
}
}
