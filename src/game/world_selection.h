#pragma once
#include <string_view>
namespace mt2mp::game::world_selection {
inline bool valid_name(std::string_view value) {
    return !value.empty() && value.size()<=200 && value!="." && value!=".." &&
        value.find_first_of("/\\\"\r\n\t")==std::string_view::npos;
}
inline bool save_file(std::string_view value) {
    return value.size()>4 && value.ends_with(".vrz") && !value.ends_with(".meta.vrz") && valid_name(value);
}
}
