#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace mt2mp::game::initial_release {
struct Version {int32_t major=0,minor=0,patch=0;bool operator==(const Version&)const=default;};
struct State {
    Version version,planned,last_changelog;
    int32_t release_type=0,changes=0,changelogs=0,last_changelog_type=0;
    int32_t server_current=0,server_target=0,map_state=0;
    bool operator==(const State&)const=default;
};
// Native addresses stay local; the existing Controller carries snapshot and action UUIDs.
struct Binding {
    uintptr_t image=0,window=0,manager=0,release=0,software=0,server=0,map=0;
    uint64_t epoch=0,generation=0;
    uint32_t thread=0;
    std::string snapshot_uuid;
    bool operator==(const Binding&)const=default;
};
struct Request {std::string snapshot_uuid,action_uuid;State before;};
class Memory {
public:
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
bool read_state(Memory&,const Binding&,State&,std::string&);
bool initial_before(const State&,std::string&);
bool functional_starting_region(Memory&,const Binding&,std::string&);
bool release_effect(const State& before,const State& after,std::string&);
bool settled_open(const State&,std::string&);
struct alignas(8) Record {std::array<uint8_t,0x78> bytes{};};
using RecordCtor=void(*)(void*,const char*);
using RecordDtor=void(*)(void*);
using RecordEquals=bool(*)(const void*,const char*);
using UiCommand=bool(*)(void*,void*);
struct Api {RecordCtor construct=nullptr;RecordDtor destroy=nullptr;RecordEquals equals=nullptr;UiCommand command=nullptr;};
// Before a hook is installed this checks all called entry bodies. A hooked UI entry
// needs the actual hook owner's physical patch and trampoline check at each invocation.
bool resolve_unpatched(Memory&,uintptr_t,std::string_view,Api&,std::string&);
struct CallReceipt {
    bool constructed=false,label_confirmed=false,attempted=false,returned=false,accepted=false;
    bool cleanup_attempted=false,cleanup_confirmed=false;
    uint32_t seh_code=0,cleanup_exception=0;
};
class Calls {
public:
    virtual ~Calls()=default;
    virtual bool command(const Api&,uintptr_t,CallReceipt&,std::string&)=0;
};
Calls& process_calls();
class Root {
public:
    virtual ~Root()=default;
    // Authorize either an uninitialized-Controller Host fixture or an ordered action.
    // A fixture binds actual native origin/Clock0/GameUpdate and does not claim a peer
    // ACK. Ordered gameplay requires actual Subscriber membership replication.
    // Check current game thread/world and functional starter without RNG calls.
    virtual bool authorize(const Request&,Binding&,Api&,std::string&)=0;
    virtual bool current(const Binding&,std::string&)=0;
    virtual void failed(std::string_view,const std::string&)noexcept=0;
};
struct Receipt {CallReceipt native;State after;bool effect_observed=false;std::string cause;};
// Caller owns UUID dedupe and full-ledger/state replication. This is a synchronous
// original UI effect receipt, not a Controller completion or subscriber acknowledgement.
bool execute(const Request&,Root&,Memory&,Calls&,Receipt&,std::string&);
enum class CommandClass:uint8_t {Other,DoCelebration,Unreadable};
CommandClass classify_record(const Api&,const void* native_record,uint32_t& seh_code)noexcept;
enum class Dispatch:uint8_t {Forward,Suppress};
class IntentRoot {
public:
    virtual ~IntentRoot()=default;
    virtual bool read_current(Binding&,State&,std::string&)=0;
    virtual bool submit(const Binding&,const State&,std::string&)=0;
    virtual void rejected(const std::string&)noexcept=0;
};
// Call at the original ReleaseHUD.UICommand entry before CanReleaseNow consumes RNG.
// Worker/foreign commands retain their original route. Both peers submit to the same
// Controller; original native application uses the owner's trampoline once.
Dispatch capture(uintptr_t window,uint32_t current_thread,uint32_t game_thread,
                 bool cooperative,CommandClass,IntentRoot&,std::string&,uint32_t command_fault=0)noexcept;
}
