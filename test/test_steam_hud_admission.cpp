#include "hud_time_writer.h"
#include "hud_calendar_transport.h"
#include "time_runtime.h"
#include "session_provider.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fixture {
using namespace mt2mp;
using namespace mt2mp::game;
action_session::Context session{};
time_runtime::Status runtime{};
hud_time::CalendarStatus calendar{};
bool connected=true,status_ok=true,session_ok=true,send_ok=true,suspend_ok=true;
uint64_t physical_session=0,physical_slot=0,epoch=0;
uint8_t mask=0;
std::string snapshot,pending,wire_error;
unsigned reads=0,change_on_read=0,claims=0,sends=0,applies=0,suspends=0;
std::function<void()> change;
net::SidecarHandler captured_handler;
std::vector<uint64_t> targets;
hud_time::CalendarSample applied{};
int checks=0;
void check(bool okay,const char* expression,int line){
    ++checks;if(!okay){std::fprintf(stderr,"line %d: %s\n",line,expression);std::exit(1);}
}
void reset(bool client=true){
    session={0x109000000000123ULL,client?2ULL:1ULL,1,3,
        client?action_session::Role::Client:action_session::Role::Host,"snapshot-A"};
    physical_session=session.session;physical_slot=session.local_peer;mask=session.peer_mask;
    snapshot=session.snapshot_uuid;epoch=41;pending.clear();wire_error.clear();
    runtime={};runtime.phase=time_runtime::Phase::Active;runtime.epoch=epoch;runtime.hook_mask=3;
    runtime.controller.applied={true,false,7,{3,1}};runtime.controller.observed={true,false,9,{1,0}};
    calendar={};calendar.bound=true;calendar.epoch=epoch;
    connected=status_ok=session_ok=send_ok=suspend_ok=true;
    reads=change_on_read=claims=sends=applies=suspends=0;change={};captured_handler={};targets.clear();applied={};
}
struct Session final:action_session::Session {
    bool current(action_session::Context& out,std::string& error) override {
        ++reads;if(change_on_read&&reads==change_on_read&&change)change();
        if(!session_ok){error="selected Session unavailable";return false;}
        out=session;error.clear();return true;
    }
} provider;
}
#define CHECK(...) fixture::check((__VA_ARGS__),#__VA_ARGS__,__LINE__)
namespace mt2mp::action_session {Session& selected_session(){return fixture::provider;}}
namespace mt2mp::net {
std::string current_snapshot_id(){return fixture::snapshot;}
uint64_t current_world_epoch(){return fixture::epoch;}
std::string pending_snapshot_load_id(){return fixture::pending;}
namespace session_wire {
bool connected(){return fixture::connected;}
uint64_t session_id(){return fixture::physical_session;}
uint64_t local_peer(){return fixture::physical_slot;}
uint8_t peer_mask(){return fixture::mask;}
std::string failure_reason(){return fixture::wire_error;}
bool send(uint8_t channel,const uint8_t* bytes,uint32_t size,uint64_t target){
    CHECK(channel==10&&bytes&&size==game::hud_time::transport::packet_size);
    ++fixture::sends;fixture::targets.push_back(target);return fixture::send_ok;
}
bool claim_handler(uint8_t channel,SidecarHandler handler,std::string& error){
    CHECK(channel==10);++fixture::claims;
    if(fixture::captured_handler){error="exclusive Calendar channel already claimed";return false;}
    fixture::captured_handler=std::move(handler);error.clear();return true;
}
}
}
namespace mt2mp::game::time_integration {
struct Status {time_runtime::Status runtime;};
struct Provider {
    bool status(Status& out,std::string& error){
        if(!fixture::status_ok){error="Time status failed";return false;}
        out.runtime=fixture::runtime;error.clear();return true;
    }
};
Provider& instance(){static Provider value;return value;}
}
namespace mt2mp::game::hud_time_runtime {
bool initialized=true;
#include "steam_hud_time.inc"
}
namespace mt2mp::game::hud_calendar_runtime {
struct CalendarFixture {hud_time::CalendarStatus status(){return fixture::calendar;}} calendar;
#include "steam_hud_calendar.inc"
}
// Only logging is replaced; the extracted send and exclusive claim bodies are unchanged.
#define LOG_INFO(...) ((void)0)
namespace fixture {
hud_time::transport::Services services(){
    hud_time::transport::Services services;
    services.thread_id=[](){return uint32_t{42};};
    services.context=hud_calendar_runtime::transport_context;
    services.probe=[](hud_time::CalendarSample& out,std::string& e){
        out={epoch,1,runtime.controller.observed.revision,{2026,1,2,2,123,true,true}};e.clear();return true;
    };
    services.apply=[](uint64_t sender,const hud_time::CalendarSample& sample,std::string& e){
        CHECK(sender==1);++applies;applied=sample;e.clear();return true;
    };
    services.suspend_calendar=[](const std::string&,std::string& e){
        ++suspends;calendar.bound=false;
        if(!suspend_ok){e="injected Calendar retirement failure";return false;}e.clear();return true;
    };
#include "steam_hud_wire.inc"
    return services;
}
}
#undef LOG_INFO

int main(){
    using namespace fixture;
    namespace clock=mt2mp::game::hud_time_runtime;
    namespace cal=mt2mp::game::hud_calendar_runtime;
    namespace wire=mt2mp::game::hud_time::transport;
    std::string error;
    hud_time::Authority authority;
    hud_time::CalendarContext context;
    wire::Context transport;
    clock::initialized=false;
    CHECK(!clock::authority(nullptr,authority,error)&&error=="hud_time_runtime_not_initialized");
    clock::initialized=true;
    reset();
    CHECK(clock::authority(nullptr,authority,error)&&authority.available&&authority.client);
    CHECK(authority.epoch==41&&authority.revision==7&&authority.selected_scale==3&&authority.pause_scale==1);
    CHECK(cal::context(nullptr,context,error)&&context.active&&context.client&&context.time_revision==7);
    CHECK(cal::transport_context(transport,error)&&transport.local_peer==2&&transport.peer_mask==3&&transport.calendar_bound);
    CHECK(transport.epoch==41&&transport.calendar_epoch==41&&transport.time_revision==7);
    reset(false);
    CHECK(clock::authority(nullptr,authority,error)&&!authority.available&&!authority.client);
    CHECK(cal::context(nullptr,context,error)&&context.active&&!context.client&&context.time_revision==9);
    CHECK(cal::transport_context(transport,error)&&transport.local_peer==1&&!transport.client);
    // Active admission failures keep their causes instead of impersonating a disconnected success.
    reset();session_ok=false;
    CHECK(!clock::authority(nullptr,authority,error)&&error=="selected Session unavailable");
    CHECK(!cal::context(nullptr,context,error)&&error=="selected Session unavailable");
    reset();status_ok=false;
    CHECK(!clock::authority(nullptr,authority,error)&&error=="Time status failed");
    CHECK(!cal::context(nullptr,context,error)&&error=="Time status failed");
    const std::vector<std::function<void()>> bad_session={
        [](){++session.session;},[](){session.session=physical_session=0;},
        [](){session.snapshot_uuid="snapshot-B";},[](){session.snapshot_uuid=snapshot="";},[](){session.host_peer=2;},
        [](){session.local_peer=physical_slot=76561198000000001ULL;},
        [](){session.local_peer=physical_slot=0;},[](){session.local_peer=3;},
        [](){session.role=action_session::Role::Host;},
        [](){session.role=static_cast<action_session::Role>(255);},
        [](){session.peer_mask=7;},[](){session.peer_mask=mask=0x83;},
        [](){session.peer_mask=mask=2;},[](){session.peer_mask=mask=1;}
    };
    for(const auto& mutate:bad_session){
        reset();mutate();
        CHECK(!clock::authority(nullptr,authority,error)&&!error.empty());
        CHECK(!cal::context(nullptr,context,error)&&!error.empty());
        CHECK(!cal::transport_context(transport,error)&&!error.empty());
    }
    const std::vector<std::function<void()>> unavailable={
        [](){runtime.phase=time_runtime::Phase::Waiting;},[](){runtime.hook_mask=1;},
        [](){runtime.presentation_uncertain=true;},[](){runtime.controller.write_locked=true;},
        [](){runtime.epoch=0;},[](){++epoch;},[](){connected=false;},[](){pending="loading";}
    };
    for(const auto& mutate:unavailable){
        reset();mutate();
        CHECK(clock::authority(nullptr,authority,error)&&!authority.available&&error.empty());
        CHECK(cal::context(nullptr,context,error)&&!context.active&&error.empty());
        CHECK(cal::transport_context(transport,error)&&!transport.active&&error.empty());
        CHECK(reads==0);
    }
    reset();runtime.controller.applied.uncertain=true;
    CHECK(clock::authority(nullptr,authority,error)&&!authority.available);
    CHECK(cal::context(nullptr,context,error)&&!context.active);
    reset(false);runtime.controller.observed.present=false;
    CHECK(cal::context(nullptr,context,error)&&!context.active);
    reset();calendar.locked=true;
    CHECK(cal::transport_context(transport,error)&&!transport.calendar_bound);
    // A changed selected Session is rejected even if transport metadata changes with it.
    reset();change_on_read=2;change=[](){++session.session;++physical_session;};
    CHECK(!cal::transport_context(transport,error)&&error=="Calendar transport current Session changed");
    reset();change_on_read=2;change=[](){++epoch;};
    CHECK(!cal::transport_context(transport,error)&&error=="Calendar transport current Session changed");

    // Run the unchanged production Adapter against the extracted session-wire callbacks.
    reset(false);
    {
        wire::Adapter adapter;
        CHECK(adapter.initialize(42,services(),error)&&claims==1);
        CHECK(!adapter.initialize(42,services(),error)&&claims==1);
        CHECK(adapter.bind(error));
        CHECK(adapter.tick(1000,error)&&sends==1&&targets==std::vector<uint64_t>{2});
        CHECK(adapter.status().send_accepted==1);
        CHECK(adapter.suspend("lobby left",error)&&!adapter.status().bound&&suspends==1);
        calendar.bound=true;
        CHECK(!adapter.bind(error)&&error=="calendar_transport_epoch_retired");
        ++epoch;runtime.epoch=calendar.epoch=epoch;
        CHECK(adapter.bind(error)&&claims==1);
        send_ok=false;wire_error="Steam channel refused packet";
        CHECK(!adapter.tick(1100,error)&&error.find(wire_error)!=std::string::npos);
        CHECK(!adapter.status().bound&&adapter.status().send_attempted==2&&adapter.status().send_accepted==1);
        wire::Adapter duplicate;
        CHECK(!duplicate.initialize(42,services(),error)&&error.find("already claimed")!=std::string::npos);
    }
    reset();
    {
        wire::Adapter adapter;
        CHECK(adapter.initialize(42,services(),error)&&claims==1&&adapter.bind(error));
        hud_time::CalendarSample sample{epoch,1,8,{2026,1,2,2,123,true,true}};
        wire::Packet packet{};CHECK(wire::encode(sample,packet,error));
        captured_handler(1,packet.data(),static_cast<uint32_t>(packet.size()));
        CHECK(adapter.status().pending&&adapter.tick(1000,error)&&applies==0&&adapter.status().future_waits==1);
        runtime.controller.applied.revision=8;
        CHECK(adapter.tick(1100,error)&&applies==1&&applied==sample&&adapter.status().apply_confirmed==1);
        CHECK(!adapter.enqueue(76561198000000001ULL,packet.data(),packet.size(),error)&&error=="calendar_transport_untrusted_sender");
        CHECK(adapter.suspend("leave",error)&&!adapter.status().pending&&!adapter.status().bound);
        calendar.bound=true;
        CHECK(!adapter.bind(error)&&error=="calendar_transport_epoch_retired");
        ++epoch;runtime.epoch=calendar.epoch=epoch;
        CHECK(adapter.bind(error)&&claims==1);
        CHECK(!adapter.enqueue(1,packet.data(),packet.size(),error)&&error=="calendar_transport_sample_epoch_mismatch");
        suspend_ok=false;
        CHECK(!adapter.suspend("retire failure",error)&&adapter.status().locked&&adapter.status().suspend_pending);
        suspend_ok=true;CHECK(adapter.suspend("cleanup",error)&&adapter.status().locked);
        calendar.bound=true;++epoch;runtime.epoch=calendar.epoch=epoch;
        CHECK(!adapter.bind(error)&&error=="calendar_suspend_failed: injected Calendar retirement failure; subsequent: calendar_transport_locked");
    }
    bool expired=false;
    try{wire::Packet packet{};captured_handler(1,packet.data(),static_cast<uint32_t>(packet.size()));}
    catch(const std::runtime_error& cause){expired=std::string(cause.what())=="calendar_transport_handler_owner_expired";}
    CHECK(expired);
    reset();send_ok=false;auto io=services();wire::Packet packet{};
    CHECK(!io.send(10,packet.data(),static_cast<uint32_t>(packet.size()),1,error)&&error=="session transport refused Calendar packet");
    std::printf("Steam HUD admission: %d checks passed\n",checks);
}
