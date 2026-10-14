#pragma once
#include "native_time_input.h"
#include "shared_time.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::time_input_diagnostics {
using json=nlohmann::json;
bool initialize(uintptr_t image,uint32_t thread,const std::string& run_id,
                const std::string& loaded_dll_sha256,
                bool(*context)(void*,native_time_input::Context&,std::string&),std::string&);
bool ready();
bool unresolved();
void poll(void* event,uint64_t next_frame);
void leaf(uintptr_t caller,const shared_time::HookResult&,uint64_t next_frame);
void frame(uint64_t completed_frame);
void tick();
void suspend(const std::string& reason);
json read(uint64_t completed_frames,uint64_t ui_updates);
bool queue(const json& args,uint64_t completed_frames,json& result,std::string& error);
bool receipt(const json& args,json& result,std::string& error);
}
