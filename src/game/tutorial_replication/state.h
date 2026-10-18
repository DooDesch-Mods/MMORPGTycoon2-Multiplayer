#pragma once
#include "vendor/nlohmann/json.hpp"
#include "startingpoint.h"
#include "research.h"
#include "staff_display.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <array>

namespace mt2mp::game::tutorial_replication {
struct MessageProgress {
    bool shown=false;
    float value=0,minimum=0,maximum=0;
    bool operator==(const MessageProgress&)const=default;
};
bool valid(const MessageProgress&,std::string&);
struct Cursor {
    std::string scenario,last;
    int32_t count=0,next=0;
    bool complete=false;
    std::vector<std::string> history;
    std::optional<MessageProgress> message_progress;
    bool operator==(const Cursor&)const=default;
};
struct State {
    std::vector<Cursor> active;
    bool initial_complete=false,leveltwo_complete=false;
    std::optional<startingpoint::Row> startingpoint;
    std::optional<research::Row> research;
    std::optional<staff_display::State> staff_display;
    // Terminal cursors survive the native ScenarioLog's immediate destruction.
    std::vector<Cursor> terminal;
    std::optional<startingpoint::Row> terminal_startingpoint;
    std::optional<std::array<int32_t,3>> version,terminal_version;
    // Fixed Initial/LevelTwo birth and end serials. Zero birth means loaded.
    std::array<std::array<uint32_t,2>,2> lifecycle_order{};
    int32_t leveltwo_initial_next=-1;
    bool operator==(const State&)const=default;
};
bool valid(const State&,std::string&);
nlohmann::json encode(const State&);
bool decode(const nlohmann::json&,State&,std::string&);
enum class Start {Native,Skip,Reject};
Start command_policy(const std::string&,bool restoring);
}
