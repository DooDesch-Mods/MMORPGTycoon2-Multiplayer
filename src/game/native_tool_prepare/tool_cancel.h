#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>
namespace mt2mp::game::native_tool_prepare {
struct Context {std::string token,snapshot;uint64_t epoch{},generation{};};
struct Readback {bool actual_default=false;uint8_t placing=1,pending=1,cancel=1;uintptr_t owned_a=1,owned_b=1;int32_t records=-1;uint8_t manager_progress=1,manager_queue=1;int32_t edits=-1,saves=-1;};
bool parse(const nlohmann::json&,Context&,std::string&);
bool clean(bool build,const Readback&,std::string&,bool prior_tool=true);
bool unresolved_effect(uint32_t attempted,bool confirmed)noexcept;
bool invoke(const nlohmann::json&,nlohmann::json&,std::string&);
bool read(const nlohmann::json&,nlohmann::json&,std::string&);
bool unresolved() noexcept;
}
