#pragma once
#include "vendor/nlohmann/json.hpp"
#include "move_kiss/move_kiss.h"
namespace mt2mp::game::move_ui_probe {
struct Input {std::string token,snapshot,key;uint64_t epoch{},generation{};std::array<float,3> position{};};
enum class Phase {Rejected,Attempted,Armed,Captured,Confirmed,Unknown};
struct Evidence {
    Phase phase=Phase::Rejected;std::string token,action_uuid,cause;
    uint32_t attempted_mask{},returned_mask{},exception{},cleanup_exception{};
    bool command_accepted{};uint64_t captures{};
};
struct Bound {Input input;move_kiss::Intent armed;Evidence receipt;bool submitted=false;uint64_t armed_at{};};
bool parse(const nlohmann::json&,Input&,std::string&);
bool parse_token(const nlohmann::json&,std::string&,std::string&);
bool bind_capture(Bound&,const move_kiss::Intent&,const std::string&,std::string&);
bool confirmation(Bound&,const nlohmann::json& controller,const nlohmann::json& local_native,bool host,std::string&);
bool probe(const nlohmann::json&,nlohmann::json&,std::string&);
bool read(const nlohmann::json&,nlohmann::json&,std::string&);
bool unresolved() noexcept;
bool captured(const move_kiss::Intent&,const std::string& action_uuid,std::string&) noexcept;
void submitted(const std::string& action_uuid) noexcept;
void failed(uintptr_t target,const std::string& cause) noexcept;
}
