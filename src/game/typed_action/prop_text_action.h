#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::action_session {
enum class PropTextKind:uint8_t {Name=1,Description=2};
struct PropTextIntent {
    PropTextKind kind=PropTextKind::Name;
    std::string text;
    bool operator==(const PropTextIntent&)const=default;
};
}
