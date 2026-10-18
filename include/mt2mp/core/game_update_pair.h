#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game_update_pair {
enum class Section : uint32_t { Before, Lobby, Typed, Transport, Time, Hud, Diagnostics, Prepare, Count };
inline constexpr const char* section_names[] = {"game_update.before", "game_update.lobby", "game_update.typed", "game_update.transport", "game_update.time", "game_update.hud", "game_update.diagnostics", "game_update.prepare"};
struct TraceToken { uint64_t epoch=0,token=0; };
struct Trace {
    void* context=nullptr;
    TraceToken (*begin)(void*,Section)=nullptr;
    void (*end)(void*,Section,TraceToken)=nullptr;
};
struct Frame {
    uint64_t id=0;
    void* game=nullptr;
    float dt=0;
    uint32_t original_returned=0;
};
// Call only after the Framework-specific runtime initialization has completed.
// The exact owner and game thread remain bound until all entered frames end.
bool activate(const void* owner,const Trace&,std::string& error);
bool begin(const void* owner,const Frame&,std::string& error);
// Closed distinguishes a consumed failed-original/post frame from an invalid end.
bool end(const void* owner,const Frame&,std::string& error,bool* closed=nullptr);
bool retire(const void* owner,std::string& error);
}
