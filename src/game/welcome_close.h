#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>

namespace mt2mp::game::tutorial_welcome {
struct Request {
    std::string token,snapshot;
    uint64_t epoch=0,generation=0;
    int32_t index=4;
    bool leveltwo=false;
};
// Call only from the existing guarded native mutation mailbox on Mainthread.
// This hides only Initial welcome/allDone or one of the seven fixed LevelTwo messages. Normal Update owns progression.
const char* leveltwo_message(int32_t index);
bool close(const Request&,nlohmann::json& receipt,std::string& error);
bool network_tab(const Request&,nlohmann::json& receipt,std::string& error);
}
