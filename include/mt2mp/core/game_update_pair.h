#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game_update_pair {
enum class Section : uint32_t { Before, Lobby, Typed, Transport, Time, Hud, Diagnostics, Prepare, OrderedService, OrderedTransport, OrderedMaintain, OrderedPeriodic, OrderedSnapshot, OrderedController, OrderedBoundary, OrderedSeal, OrderedConsume, OrderedNativeStep, OrderedDiagnostics, Count };
inline constexpr const char* section_names[] = {"game_update.before", "game_update.lobby", "game_update.typed", "game_update.transport", "game_update.time", "game_update.hud", "game_update.diagnostics", "game_update.prepare", "ordered.service", "ordered.transport", "ordered.maintain", "ordered.periodic", "ordered.snapshot", "ordered.controller", "ordered.boundary", "ordered.seal", "ordered.consume", "ordered.native_step", "ordered.diagnostics"};
struct TraceToken { uint64_t epoch=0,token=0; };
struct Trace {
    void* context=nullptr;
    TraceToken (*begin)(void*,Section)=nullptr;
    void (*end)(void*,Section,TraceToken)=nullptr;
};
// The Framework callback owner binds this bridge; inactive captures return zero tokens.
#ifdef MT2MP_FRAMEWORK_PLUGIN
TraceToken begin_scope(Section) noexcept;
void end_scope(Section,TraceToken) noexcept;
#endif
class Scope {
    Section section_;
    TraceToken token_{};
public:
    explicit Scope(Section section) noexcept:section_(section) {
#ifdef MT2MP_FRAMEWORK_PLUGIN
        token_=begin_scope(section_);
#endif
    }
    ~Scope() noexcept {
#ifdef MT2MP_FRAMEWORK_PLUGIN
        if(token_.token)end_scope(section_,token_);
#endif
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
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
