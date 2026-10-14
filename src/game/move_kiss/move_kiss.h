#pragma once
#include "../typed_action/action_session.h"
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::move_kiss {
using State = action_session::BuildingState;
struct Memory { virtual ~Memory() = default; virtual bool read(uintptr_t, void*, size_t, std::string&) = 0; };
struct Origin {
    std::string snapshot_uuid; uint64_t world_epoch=0, generation=0; uintptr_t world=0;
    bool operator==(const Origin&) const = default;
};
struct Intent {
    Origin origin; uintptr_t behaviour=0, record=0, ghost=0, target=0;
    uint64_t local_uid=0; std::string object_key; State before, after;
    bool operator==(const Intent&) const = default;
};
struct Services {
    virtual ~Services() = default;
    virtual bool main_thread() const noexcept = 0;
    virtual bool active(Origin&, std::string&) = 0;
    virtual bool bind_target(const Origin&, uintptr_t ghost, uintptr_t target, uint64_t uid, State&, std::string& key, std::string&) = 0;
    virtual bool submit_move(const Intent&, std::string&) = 0;
    virtual void reject(const std::string&) noexcept = 0;
};
enum class Route { Forward, Suppress };
class Capture {
    Memory& memory_; Services& services_; uintptr_t image_; bool busy_=false;
    bool read_intent(uintptr_t, const Origin&, Intent&, std::string&);
public:
    Capture(Memory& m, Services& s, uintptr_t image):memory_(m),services_(s),image_(image){}
    Route purchase(uintptr_t behaviour, uintptr_t caller_rva) noexcept;
};
using Ctor = void(*)(void*, const char*);
using Begin = void(*)(const void*);
using Open = void*(*)(const void*);
using MoveCtor = void(*)(void*, void*, const void*);
using MoveDo = void(*)(void*, void*);
using Dtor = void(*)(void*);
using Finish = void(*)();
struct Calls {
    Ctor loc_ctor=nullptr; Begin begin=nullptr; Open open=nullptr;
    MoveCtor move_ctor=nullptr; MoveDo move_do=nullptr;
    Dtor move_dtor=nullptr; Finish close=nullptr, end=nullptr;
    Dtor vector_dtor=nullptr, string_dtor=nullptr;
};
struct Receipt {
    bool started=false, loc_attempted=false,loc_returned=false,begin_attempted=false,begin_returned=false;
    bool open_attempted=false,open_returned=false,ctor_attempted=false,ctor_returned=false;
    bool move_attempted=false,move_returned=false,dtor_attempted=false,dtor_returned=false;
    bool close_attempted=false,close_returned=false,end_attempted=false,end_returned=false;
    bool vector_attempted=false,vector_returned=false,string_attempted=false,string_returned=false;
    uint32_t exception=0,cleanup_exception=0;
    uintptr_t graph=0;
};
// Local transform cache bytes never enter a transport envelope.
bool transform(const State&, std::array<unsigned char,0x6c>&, std::string&);
// The caller reserves its action UUID before invoking this one-shot native call.
bool execute_once(const Calls&, uintptr_t target, const std::array<unsigned char,0x6c>&, Receipt&, std::string&);
bool resolve_calls(Memory&, uintptr_t image, const std::string& exe_sha256, Calls&, std::string&);
struct LocalBinding {
    Origin origin; uintptr_t target=0; uint64_t local_uid=0;
    State actual; action_session::Ledger ledger;
    std::array<unsigned char,0x6c> transform{};
};
struct Readback {
    LocalBinding current;
    bool manager_registered=false, inverse_registered=false, selection_present=false, edit_closed=false;
};
struct ExecutionServices {
    virtual ~ExecutionServices()=default;
    // Root resolves the existing shared ID and performs native placement checks.
    virtual bool prepare(const action_session::Request&, LocalBinding&, std::string&)=0;
    virtual bool readback(const LocalBinding&, uintptr_t inverse_graph, Readback&, std::string&)=0;
};
action_session::Outcome execute(ExecutionServices&, const Calls&, const action_session::Request&, Receipt&, action_session::Evidence&, std::string&);
}
