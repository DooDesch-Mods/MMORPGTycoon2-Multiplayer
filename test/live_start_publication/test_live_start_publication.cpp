#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

struct Binding {};
struct Fence {};
struct Transport {};
struct Scenario {int fail_at=-1; int bridge_created=0; int bridge_destroyed=0; int binds=0;};
Scenario scenario;

bool stage(int id, std::string& error) {
    if (scenario.fail_at == id) {error="original stage " + std::to_string(id); return false;}
    ++scenario.binds;
    return true;
}

namespace action {
enum class Role {Host, Client};
struct Context {Role role=Role::Host;};
}

namespace live_host_state {
struct NativePort {NativePort(action::Context, Binding, Fence*) {}};
struct PartyPort {};
namespace day {
struct Target {};
struct NativePort {NativePort(action::Context, Binding, Fence&) {}};
struct Sync {
    Sync(NativePort&, live_host_state::NativePort&, action::Context, Binding) {}
    bool bootstrap(std::string& error) {return stage(0,error);}
};
Sync* host_journal=nullptr;
void bind_host_journal(Sync* sync) {host_journal=sync;}
namespace economy {
struct NativePort {NativePort(day::NativePort&, Fence&, Target) {}};
struct Coupling {Coupling(day::Sync&, NativePort&) {}};
}
}
bool read_time_authority(action::Context, uint64_t&, std::string&) {return true;}
struct Bridge {
    NativePort* port;
    day::Sync* day_provider=nullptr;
    day::economy::Coupling* economy_provider=nullptr;
    template<class Clock>
    Bridge(action::Context, Binding, int, NativePort& native, Transport&, Clock) : port(&native) {++scenario.bridge_created;}
    ~Bridge() {++scenario.bridge_destroyed;}
    void bind_day(day::Sync& provider) {day_provider=&provider;}
    void bind_economy(day::economy::Coupling& provider) {economy_provider=&provider;}
    bool bind_parties(PartyPort&, std::string& error) {return stage(1,error);}
    template<class Provider> bool bind_vip(Provider&, std::string& error) {return stage(2,error);}
    template<class Provider> bool bind_release(Provider&, std::string& error) {return stage(3,error);}
    template<class Provider> bool bind_management(Provider&, std::string& error) {return stage(4,error);}
    template<class Provider> bool bind_marketing(Provider&, std::string& error) {return stage(5,error);}
    template<class Provider> bool bind_monsters(Provider&, std::string& error) {return stage(6,error);}
};
}

namespace party_frame {
std::unique_ptr<live_host_state::PartyPort> make_native(live_host_state::NativePort&) {
    return std::make_unique<live_host_state::PartyPort>();
}
}
void vip_alert_authorize() {}
namespace scenario_vip_authority {struct FrameAdapter {explicit FrameAdapter(void(*)()) {}};}
namespace release_runtime {struct LivePort {};}
namespace management_runtime {struct LivePort {};}
namespace marketing_authority {struct NativePort {};}
namespace npc_probe::monster_action_runtime {
struct MonsterPort {};
MonsterPort& live_port() {static MonsterPort port; return port;}
}

struct Engine {
    live_host_state::Bridge* bridge=nullptr;
    live_host_state::NativePort* port=nullptr;
    void bind_live_state(live_host_state::Bridge& state, live_host_state::NativePort& native) {
        bridge=&state; port=&native;
    }
};
struct Runtime {
    Transport transport;
    Engine engine;
    std::unique_ptr<live_host_state::NativePort> live_port;
    std::unique_ptr<live_host_state::Bridge> live_state;
    std::unique_ptr<live_host_state::day::NativePort> day_port;
    std::unique_ptr<live_host_state::day::Sync> day_state;
    std::unique_ptr<live_host_state::day::economy::NativePort> economy_port;
    std::unique_ptr<live_host_state::day::economy::Coupling> economy_state;
    std::unique_ptr<live_host_state::PartyPort> parties;
    std::unique_ptr<scenario_vip_authority::FrameAdapter> vip_presentation;
    std::unique_ptr<release_runtime::LivePort> release;
    std::unique_ptr<management_runtime::LivePort> management;
    std::unique_ptr<marketing_authority::NativePort> marketing;
    std::string live_error;
};

#include "actual_live_start_tail.h"

void require(bool condition, const char* message) {if(!condition)throw std::runtime_error(message);}

int main() {
    try {
        for(int failed=0; failed<7; ++failed) {
            scenario={}; scenario.fail_at=failed;
            live_host_state::day::host_journal=nullptr;
            Runtime runtime; std::string error; Fence fence; bool result=false;
            {
                struct KeepFailure {Runtime& runtime; std::string& error;
                    ~KeepFailure() {if(!error.empty())runtime.live_error=error;}} keep{runtime,error};
                result=actual_live_start_tail(&runtime,{action::Role::Host},{},7,fence,error);
            }
            require(!result,"fallible stage unexpectedly succeeded");
            require(!runtime.live_state,"failed setup published tickable Bridge");
            require(!runtime.engine.bridge && !runtime.engine.port,"failed setup bound Engine");
            require(!live_host_state::day::host_journal,"failed setup bound Host journal");
            require(error=="original stage "+std::to_string(failed),"original provider error lost");
            require(runtime.live_error==error,"retained live failure differs from original cause");
            require(scenario.bridge_created==1 && scenario.bridge_destroyed==1,"failed local Bridge not released");
        }
        for(const auto role:{action::Role::Host,action::Role::Client}) {
            scenario={}; live_host_state::day::host_journal=nullptr;
            Runtime runtime; std::string error; Fence fence;
            const bool result=actual_live_start_tail(&runtime,{role},{},7,fence,error);
            require(result && error.empty(),"fully bound setup failed");
            require(runtime.live_state && scenario.bridge_created==1 && scenario.bridge_destroyed==0,"Bridge not published once");
            require(runtime.live_state->port==runtime.live_port.get(),"published Bridge lost NativePort reference");
            require(runtime.live_state->day_provider==runtime.day_state.get(),"published Bridge lost Day reference");
            require(runtime.live_state->economy_provider==runtime.economy_state.get(),"published Bridge lost Economy reference");
            require(runtime.engine.bridge==runtime.live_state.get() && runtime.engine.port==runtime.live_port.get(),"Engine bound different references");
            require(live_host_state::day::host_journal==(role==action::Role::Host?runtime.day_state.get():nullptr),"Host journal role binding differs");
        }
        std::cout << "live start publication PASS 9 cases\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "live start publication FAIL: " << error.what() << '\n';
        return 1;
    }
}
