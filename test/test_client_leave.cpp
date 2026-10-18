#include <atomic>
#include <memory>
#include "action_session.h"
#include <iostream>
#include <stdexcept>
#include <map>
#include <chrono>
#define LOG_NET_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)

using namespace mt2mp::action_session;
namespace {
const std::string snapshot="11111111-1111-1111-1111-111111111111";
const std::string action_id="22222222-2222-2222-2222-222222222222";
const std::string record_key=std::string(64,'a')+":world:building:44";
void require(bool okay,const std::string& cause) {if(!okay)throw std::runtime_error(cause);}
struct FakeSession:Session {
    Context value;bool available=true;std::function<void()> on_current;
    explicit FakeSession(uint64_t peer):value{42,peer,1,3,peer==1?Role::Host:Role::Client,snapshot} {}
    bool current(Context& out,std::string& cause) override {if(on_current)on_current();if(!available){cause="fixture disconnected";return false;}out=value;cause.clear();return true;}
};
struct FakeEngine:Engine {
    Binding world{5,8};Ledger state;Evidence receipt;uint64_t executions=0,applies=0,retirements=0;
    bool current_ok=true,retirement_ok=true,throw_execute=false,throw_observe=false;Outcome effect=Outcome::Confirmed;
    std::function<void()> on_observe;
    FakeEngine(){state.cash=1000;state.flat_day=13;state.record_present=1;state.record_day=13;state.record_cash=1000;state.line_counts[0]=3;}
    bool bind(const std::string& id,Binding& out,std::string& cause) override {if(id!=snapshot){cause="wrong snapshot";return false;}out=world;cause.clear();return true;}
    bool current(const Binding& b,const std::string&,std::string& cause) override {if(!current_ok||b!=world){cause="fixture native world replaced";return false;}cause.clear();return true;}
    bool ledger(const Binding&,Ledger& out,std::string& cause) override {out=state;cause.clear();return true;}
    Outcome remove(std::string& cause) {
        if(throw_execute)throw std::runtime_error("fixture native exception");
        if(effect!=Outcome::Confirmed){cause="fixture native rejected/unknown original cause";return effect;}
        receipt={};receipt.before=state;state.cash+=400;state.record_cash+=400;state.record_income+=400;
        state.line_counts[0]--;state.line_income[0]+=400;receipt.after=state;
        receipt.target_retired=receipt.undo_confirmed=receipt.selection_confirmed=true;cause.clear();return effect;
    }
    Outcome execute_remove(const Binding&,const Request&,std::string& cause) override {++executions;return remove(cause);}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& cause) override {++applies;return remove(cause);}
    bool observe_remove(const Binding&,const Request&,Evidence& out,std::string& cause) override {
        if(on_observe)on_observe();
        if(throw_observe)throw std::runtime_error("fixture receipt observation exception");
        out=receipt;cause.clear();return true;
    }
    bool retire(const Binding&,const Request&,std::string& cause) override {
        ++retirements;if(!retirement_ok){cause="fixture holder retirement failed";return false;}receipt.retirement_confirmed=true;cause.clear();return true;
    }
};
struct Packet {uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;Handler handler;std::deque<Packet>& packets;bool refused=false;uint64_t claims=0;
    Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q) {}
    bool claim(Handler h,std::string& cause) override {++claims;if(handler){cause="channel already claimed";return false;}handler=std::move(h);cause.clear();return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string& cause) override {
        if(refused){cause="fixture send refused";return false;}packets.push_back({peer,target,bytes});cause.clear();return true;
    }
};
struct Pair {
    std::deque<Packet> packets;FakeSession hs{1},cs{2};FakeEngine he,ce;Wire ht{1,packets},ct{2,packets};
    std::vector<std::string> logs;Controller host,client;std::string error;
    Pair():host(hs,he,ht,[this](const char* level,const std::string& op,const std::string& cause){logs.push_back(std::string(level)+" "+op+" "+cause);}),
        client(cs,ce,ct,[this](const char* level,const std::string& op,const std::string& cause){logs.push_back(std::string(level)+" "+op+" "+cause);}) {
        require(host.start(0,error),error);require(client.start(0,error),error);
    }
    Packet pop() {require(!packets.empty(),"expected serialized packet");auto p=packets.front();packets.pop_front();return p;}
    bool deliver(const Packet& p,uint64_t now=1) {return (p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),now,error);}
    Message message(const Packet& p){Message m;require(decode(p.bytes.data(),p.bytes.size(),m,error),error);return m;}
    void submit() {require(client.submit_remove(action_id,record_key,0,error),error);require(he.executions==0&&ce.applies==0,"submit must not execute client");}
};
}

namespace fixture {
using Json=nlohmann::json;
Json captures=Json::object(),snapshot_status=Json::object();
uint32_t thread=7;
unsigned suspended=0,invalidated=0,deleted=0,wire_retired=0,membership_left=0,native_retired=0;
bool software_armed=true,software_ok=true,time_active=false;
bool tutorial_fenced=true,tutorial_fenced_at_delete=true;unsigned tutorial_retired=0;std::string tutorial_cause;
void reset_software_stream();
bool title=false,transition=false,saving=false,in_lobby=true,time_initialized=false;
bool fence_ok=true,wire_ok=true,time_ok=true,live_ok=true,staff_ok=true;unsigned staff_retired=0;
bool bug_ok=true;unsigned bug_retired=0;
unsigned vip_retired=0,chat_closes=0;bool chat_admitted=true;
void reset(){
    captures=Json::object();snapshot_status=Json::object();thread=7;
    suspended=invalidated=deleted=wire_retired=membership_left=native_retired=0;
    software_armed=software_ok=true;time_active=false;reset_software_stream();
    tutorial_fenced=tutorial_fenced_at_delete=true;tutorial_retired=0;tutorial_cause.clear();
    title=transition=saving=time_initialized=false;in_lobby=true;
    fence_ok=wire_ok=time_ok=live_ok=staff_ok=true;staff_retired=0;
    bug_ok=true;bug_retired=0;vip_retired=0;chat_closes=0;chat_admitted=true;
}
}
uint32_t GetCurrentThreadId(){return fixture::thread;}
namespace mt2mp {
namespace net {
namespace chat {struct Runtime {
    static Runtime& instance(){static Runtime value;return value;}
    void set_lobby_admitted(bool value){
        require(!value&&!fixture::wire_retired&&!fixture::invalidated,"chat retirement must precede wire retirement and world invalidation");
        fixture::chat_admitted=value;++fixture::chat_closes;
    }
};}
uint64_t epoch=0,revision=0,applied_updates=0;
std::array<float,2> initial{},published{};
std::map<uint64_t,int> peers;
std::string failure;
std::chrono::steady_clock::time_point last_send{};
#include "software_sync_retire_body.inc"
namespace session_wire {
void suspend_actions(){++fixture::suspended;}
bool retire_lobby(std::string& e){++fixture::wire_retired;if(!fixture::wire_ok)e="wire retirement failed";return fixture::wire_ok;}
}
void invalidate_snapshot_world(const char*){require(!fixture::chat_admitted&&fixture::chat_closes==1,"successful Leave closes pending chat before invalidating world");++fixture::invalidated;}
struct SteamMatchmaking {
    static SteamMatchmaking& instance(){static SteamMatchmaking value;return value;}
    void leave_lobby(){
        require(fixture::native_retired==1&&!fixture::software_armed,"Steam membership released before native software retirement");
        require(!epoch&&!revision&&!applied_updates&&initial==std::array<float,2>{}&&published==std::array<float,2>{}&&peers.empty()&&failure.empty()&&last_send==std::chrono::steady_clock::time_point{},"Steam membership released before actual software stream retirement");
        ++fixture::membership_left;fixture::in_lobby=false;
    }
    bool in_lobby()const{return fixture::in_lobby;}
};
}
namespace game {
namespace native {
struct WorldStatus{int active_mode_index=3;bool title_transition_pending=false;int saves_in_progress=0;};
bool read_world_status(WorldStatus& out,std::string&){out={fixture::title?2:3,fixture::transition,fixture::saving?1:0};return true;}
bool retire_software_replica(std::string& e){
    ++fixture::native_retired;
    if(!fixture::software_ok){e="original software retirement failure";return false;}
    fixture::software_armed=false;e.clear();return true;
}
}
namespace ui_build_runtime {fixture::Json status(){return fixture::captures;}}
namespace ui_remove_runtime {fixture::Json status(){return fixture::captures;}}
namespace ui_move_runtime {fixture::Json status(){return fixture::captures;}}
namespace ui_region_runtime {fixture::Json status(){return fixture::captures;}}
namespace ui_price_runtime {fixture::Json status(){return fixture::captures;}}
namespace npc_probe::action_runtime {bool pending_ui(){return fixture::captures.value("npc_pending",false);}}
namespace npc_probe::zone_action_runtime {bool pending_ui(){return fixture::captures.value("zone_pending",false);}}
namespace npc_probe::monster_action_runtime {bool pending_ui(){return fixture::captures.value("monster_pending",false);}}
namespace bug_authority::manual_action {
bool pending_ui(){return fixture::captures.value("bug_pending",false);}
bool retire_lobby(std::string& e){
    ++fixture::bug_retired;
    require(fixture::title&&!fixture::transition&&!fixture::saving&&fixture::staff_retired==1,
        "Bug owner retired before idle Title and Developer owner");
    if(!fixture::bug_ok){e="original Bug owner retirement failure";return false;}
    return true;
}
}
namespace developer_staff::action_runtime {
bool pending_ui(){return fixture::captures.value("staff_pending",false);}
bool retire_lobby(std::string& e){
    ++fixture::staff_retired;
    require(fixture::title&&!fixture::transition&&!fixture::saving,"Developer owner retired before idle native Title");
    require(fixture::native_retired==1&&fixture::wire_retired==1&&fixture::membership_left==1&&fixture::tutorial_retired==1,
        "Developer owner retired before software/membership/wire/tutorial retirement");
    if(!fixture::staff_ok){e="original Developer retirement readback failure";return false;}
    return true;
}
}
namespace time_runtime {enum class Phase{Waiting,Active};}
namespace time_integration {
struct Status{struct {time_runtime::Phase phase=time_runtime::Phase::Waiting;uint64_t epoch=0;} runtime;};
struct Adapter {
    bool status(Status& out,std::string& e){if(!fixture::time_initialized){e="time_integration_not_initialized";return false;}if(fixture::time_active){out.runtime.phase=time_runtime::Phase::Active;out.runtime.epoch=42;}return true;}
    bool suspend(const char*,std::string& e){if(!fixture::time_ok)e="time retirement failed";return fixture::time_ok;}
};
Adapter& instance(){static Adapter value;return value;}
}
namespace live_host_state {
struct Fence{bool suspend_lobby(std::string& e){if(!fixture::fence_ok)e="fence Unknown";return fixture::fence_ok;}};
Fence& economic_fence(){static Fence value;return value;}
bool retire_lobby_fence(std::string& e){if(!fixture::fence_ok)e="fence Unknown";return fixture::fence_ok;}
namespace day {void bind_host_journal(void*){}}
}
namespace tutorial_replication {
void retire(const std::string& cause){
    require(fixture::title&&!fixture::transition&&!fixture::saving,"tutorial fence retired before actual idle Title");
    require(fixture::native_retired==1&&!fixture::software_armed&&fixture::membership_left==1&&fixture::wire_retired==1&&fixture::wire_ok,"tutorial fence retired before software/Steam/wire completion");
    ++fixture::tutorial_retired;fixture::tutorial_fenced=false;fixture::tutorial_cause=cause;
}
}
namespace typed_action_runtime {
struct Live{bool suspend_lobby(std::string& e){if(!fixture::live_ok)e="live Unknown";return fixture::live_ok;}};
struct Vip {
    void retire_lobby(){
        require(fixture::title&&!fixture::transition&&!fixture::saving&&fixture::bug_retired==1,
                "VIP owner retired before settled Title and other world owners");
        ++fixture::vip_retired;
    }
};
struct Runtime {
    action_session::Controller& controller;
    uint32_t thread=7;bool lobby_retiring=false;
    std::unique_ptr<Live> live_state;bool live_port=false;
    std::unique_ptr<Vip> vip_presentation;
    std::string live_error;
    explicit Runtime(action_session::Controller& value):controller(value){}
    ~Runtime(){fixture::tutorial_fenced_at_delete=fixture::tutorial_fenced;++fixture::deleted;}
};
std::atomic<Runtime*> published_runtime{nullptr};
fixture::Json status(){return fixture::snapshot_status;}
#include "client_leave_runtime_body.inc"
}
}
}
namespace runtime=mt2mp::game::typed_action_runtime;
namespace fixture {
void reset_software_stream(){
    mt2mp::net::epoch=42;mt2mp::net::revision=9;mt2mp::net::applied_updates=8;
    mt2mp::net::initial={100,2};mt2mp::net::published={120,3};mt2mp::net::peers={{2,9}};
    mt2mp::net::failure="retained software cache name failure";
    mt2mp::net::last_send=std::chrono::steady_clock::now();
}
}
void software_retirement_delta(){
    Pair pair;std::string error;bool ready=true;
    fixture::reset();auto* owner=new runtime::Runtime(pair.client);owner->lobby_retiring=true;
    runtime::published_runtime.store(owner);
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::software_armed&&fixture::native_retired==0,"active World cleared replica");
    fixture::title=true;fixture::transition=true;
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::software_armed&&fixture::native_retired==0,"pending native Leave cleared replica");
    fixture::transition=false;fixture::saving=true;
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::software_armed&&fixture::native_retired==0,"native save cleared replica");
    fixture::saving=false;fixture::time_initialized=fixture::time_active=true;
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::software_armed&&fixture::native_retired==0,"active Time owner cleared replica");
    fixture::time_active=false;
    require(runtime::finish_lobby_leave(ready,error)&&ready,error);
    require(!fixture::software_armed&&fixture::native_retired==1&&fixture::deleted==1&&fixture::membership_left==1&&fixture::wire_retired==1,"settled Leave did not retire exactly once");

    fixture::reset();fixture::title=true;fixture::software_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&error=="original software retirement failure"&&fixture::software_armed&&mt2mp::net::epoch==42&&fixture::membership_left==0&&fixture::wire_retired==0,"native failure was hidden or old stream/membership released");
    fixture::reset();fixture::title=true;fixture::fence_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&error=="fence Unknown"&&fixture::native_retired==0&&fixture::software_armed&&mt2mp::net::epoch==42,"fence failure bypassed before native retirement");

    fixture::reset();fixture::title=true;pair.submit();
    owner=new runtime::Runtime(pair.client);owner->lobby_retiring=true;runtime::published_runtime.store(owner);
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::native_retired==0&&fixture::software_armed&&mt2mp::net::epoch==42,"pending Controller released software owner");
    runtime::published_runtime.store(nullptr);delete owner;
    std::cout<<"PASS actual Leave ordering: active/pending/save/Time/Controller retained; native failure preserved; native and actual stream cleared before Steam release\n";
}

void tutorial_lifetime_delta(){
    Pair pair;std::string error;bool ready=true;
    fixture::reset();auto* owner=new runtime::Runtime(pair.client);runtime::published_runtime.store(owner);
    require(runtime::begin_lobby_leave(error),error);
    require(fixture::tutorial_fenced&&fixture::tutorial_retired==0,"begin Leave released tutorial fence with old World active");
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"active World released tutorial fence");
    fixture::title=true;fixture::transition=true;
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"pending Title released tutorial fence");
    fixture::transition=false;fixture::saving=true;
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"native save released tutorial fence");
    fixture::saving=false;fixture::time_initialized=fixture::time_active=true;
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"active Time owner released tutorial fence");
    fixture::time_active=false;
    require(runtime::finish_lobby_leave(ready,error)&&ready,error);
    require(fixture::tutorial_retired==1&&!fixture::tutorial_fenced&&!fixture::tutorial_fenced_at_delete&&fixture::deleted==1&&!runtime::published_runtime.load()&&fixture::tutorial_cause=="confirmed native Title after lobby Leave","settled Leave did not retire tutorial once before owner storage ended");

    fixture::reset();fixture::title=true;fixture::wire_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&error=="wire retirement failed"&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"wire retirement failure released tutorial fence or lost original cause");
    fixture::reset();fixture::title=true;fixture::software_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&error=="original software retirement failure"&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"software retirement failure released tutorial fence or lost original cause");
    fixture::reset();fixture::title=true;fixture::fence_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&error=="fence Unknown"&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"economic fence failure released tutorial fence or lost original cause");
    fixture::reset();fixture::title=true;pair.submit();
    owner=new runtime::Runtime(pair.client);owner->lobby_retiring=true;runtime::published_runtime.store(owner);
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&fixture::tutorial_fenced&&fixture::tutorial_retired==0,"pending Controller released tutorial fence");
    runtime::published_runtime.store(nullptr);delete owner;
    std::cout<<"PASS tutorial Title lifetime: World/transition/save/Time/Controller/failures retain fence; settled software/Steam/wire retire once before owner deletion\n";
}

void idle_departure(){
    Pair p;require(p.client.can_retire_lobby(),"idle connected Client cannot retire");
    p.cs.available=false;require(!p.client.tick(2,p.error),"missing Host session accepted");
    require(p.client.status().locked&&p.client.can_retire_lobby(),"idle host departure prevents retirement");
    const auto cause=p.client.status().cause;
    require(!p.client.submit_remove(action_id,record_key,3,p.error),"retirement eligibility resumed action admission");
    require(p.client.status().cause==cause&&p.packets.empty()&&p.ce.applies==0,"idle retirement changed cause or sent/replayed action");
    bool other_thread_allowed=true;
    std::thread other([&]{other_thread_allowed=p.client.can_retire_lobby();});other.join();
    require(!other_thread_allowed,"foreign thread gained retirement eligibility");

    fixture::reset();auto* owner=new runtime::Runtime(p.client);owner->live_state=std::make_unique<runtime::Live>();owner->live_port=true;owner->vip_presentation=std::make_unique<runtime::Vip>();
    runtime::published_runtime.store(owner);
    require(runtime::begin_lobby_leave(p.error),p.error);
    require(owner->lobby_retiring&&fixture::suspended==1&&fixture::invalidated==1,"Leave did not suspend before world transition");
    bool ready=true;require(runtime::finish_lobby_leave(ready,p.error)&&!ready&&fixture::deleted==0,"owner deleted before actual Title");
    require(fixture::vip_retired==0,"VIP owner retired with old World active");
    fixture::title=true;fixture::transition=true;
    require(runtime::finish_lobby_leave(ready,p.error)&&!ready&&fixture::deleted==0,"owner deleted during Title transition");
    fixture::transition=false;fixture::saving=true;
    require(runtime::finish_lobby_leave(ready,p.error)&&!ready,"owner deleted during native save");
    fixture::saving=false;
    require(runtime::finish_lobby_leave(ready,p.error)&&ready,p.error);
    require(fixture::deleted==1&&fixture::membership_left==1&&fixture::wire_retired==1&&!runtime::published_runtime.load(),"Leave did not finish membership/handler/storage retirement");
    require(fixture::vip_retired==1,"settled Leave did not retire VIP owner exactly once");
    require(p.client.status().locked&&p.client.status().cause==cause,"retirement cleared original Controller lock");
}
void completed_actions_and_reentry(){
    Pair completed;completed.submit();
    for(unsigned i=0;i<4;++i)require(completed.deliver(completed.pop()),completed.error);
    require(completed.client.status().confirmed==1,"completed action fixture failed");
    completed.cs.available=false;
    require(!completed.client.tick(3,completed.error)&&completed.client.can_retire_lobby(),
        "confirmed historical action prevented idle retirement");
    Pair changed;changed.cs.value.peer_mask=7;
    require(!changed.client.tick(3,changed.error)&&changed.client.can_retire_lobby(),"idle membership change prevented retirement");
    Pair reentrant;bool entered=false;
    reentrant.cs.on_current=[&]{entered=true;std::string cause;require(!reentrant.client.tick(3,cause),"reentrant guard admitted");};
    require(!reentrant.client.tick(3,reentrant.error)&&entered&&!reentrant.client.can_retire_lobby(),
        "nested Unknown was relabeled as harmless session observation loss");
}
void incomplete_preparation(){
    fixture::reset();std::string error;bool ready=false;
    require(runtime::begin_lobby_leave(error),"uninitialized Controller cannot Leave preparation");
    fixture::title=true;
    require(runtime::finish_lobby_leave(ready,error)&&ready,"uninitialized Controller cannot finish Leave");
    require(fixture::deleted==0&&fixture::wire_retired==1,"uninitialized retirement fabricated an owner");
}
void pending_and_unknown(){
    Pair pending;pending.submit();pending.cs.available=false;
    require(!pending.client.tick(2,pending.error)&&!pending.client.can_retire_lobby(),"sent Client request retired without completion");
    Pair unknown;unknown.he.throw_execute=true;unknown.submit();require(unknown.deliver(unknown.pop()),unknown.error);
    require(unknown.host.status().locked&&!unknown.host.can_retire_lobby(),"native Unknown admitted retirement");
    unknown.hs.available=false;require(!unknown.host.tick(3,unknown.error)&&!unknown.host.can_retire_lobby(),"disconnect erased native Unknown");
    Pair native_loss;native_loss.ce.current_ok=false;
    require(!native_loss.client.tick(2,native_loss.error)&&!native_loss.client.can_retire_lobby(),"native world mismatch admitted session-only retirement");
    Pair send_failure;send_failure.ct.refused=true;
    require(!send_failure.client.submit_remove(action_id,record_key,2,send_failure.error)&&!send_failure.client.can_retire_lobby(),"uncertain send admitted retirement");
    fixture::reset();auto* owner=new runtime::Runtime(unknown.host);runtime::published_runtime.store(owner);
    require(!runtime::begin_lobby_leave(unknown.error)&&fixture::suspended==0&&fixture::invalidated==0,"runtime ignored Unknown guard");
    runtime::published_runtime.store(nullptr);delete owner;
}
void other_guards(){
    Pair p;p.cs.available=false;require(!p.client.tick(2,p.error),"setup failed");
    for(const char* key:{"deferred_build_capture","replay_active","pending_ui","npc_pending","monster_pending"}){
        fixture::reset();fixture::captures[key]=true;
        auto* owner=new runtime::Runtime(p.client);runtime::published_runtime.store(owner);
        require(!runtime::begin_lobby_leave(p.error)&&fixture::suspended==0,"pending UI capture admitted retirement");
        runtime::published_runtime.store(nullptr);delete owner;
    }
    fixture::reset();fixture::live_ok=false;
    auto* owner=new runtime::Runtime(p.client);owner->live_state=std::make_unique<runtime::Live>();runtime::published_runtime.store(owner);
    require(!runtime::begin_lobby_leave(p.error)&&p.error=="live Unknown"&&fixture::invalidated==0,"live Unknown bypassed");
    require(!runtime::begin_lobby_leave(p.error),"partial retirement replayed");
    runtime::published_runtime.store(nullptr);delete owner;
}
void locked_live_departure(){
    Pair pair;fixture::reset();std::string error;bool ready=false;
    auto* owner=new runtime::Runtime(pair.client);owner->live_state=std::make_unique<runtime::Live>();owner->live_port=true;
    owner->live_error="retained actual Economy Unknown original NPC pose mismatch";const auto original=owner->live_error;
    runtime::published_runtime.store(owner);
    require(runtime::begin_lobby_leave(error)&&owner->lobby_retiring&&owner->live_error==original,"idle live Unknown could not start Leave or its cause was cleared");
    require(pair.ce.applies==0&&pair.he.executions==0&&pair.packets.empty(),"Leave repeated a failed native/transport action");
    require(!runtime::begin_lobby_leave(error)&&owner->live_error==original&&fixture::suspended==1,"repeated Leave re-entered suspension");
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&!fixture::deleted&&owner->live_error==original,"active World deleted Unknown evidence");
    fixture::title=true;require(runtime::finish_lobby_leave(ready,error)&&ready&&fixture::deleted==1&&fixture::native_retired==1&&fixture::wire_retired==1,"actual idle Title did not complete explicit Leave");
    fixture::reset();owner=new runtime::Runtime(pair.client);owner->live_state=std::make_unique<runtime::Live>();owner->live_error=original;fixture::live_ok=false;
    runtime::published_runtime.store(owner);
    require(!runtime::begin_lobby_leave(error)&&error=="live Unknown"&&owner->live_error==original&&!fixture::invalidated,"new suspension error replaced original Unknown or admitted native Leave");
    runtime::published_runtime.store(nullptr);delete owner;
    std::cout<<"PASS locked live Leave: original cause retained, no action replay, exact Title retirement, distinct suspension failure\n";
}
void developer_lifetime_delta(){
    Pair pair;fixture::reset();std::string error;bool ready=true;
    auto* owner=new runtime::Runtime(pair.client);runtime::published_runtime.store(owner);
    fixture::captures["zone_pending"]=true;
    require(!runtime::begin_lobby_leave(error)&&error=="native lobby Leave blocked by retained active/pending UI capture intent"&&
        !fixture::suspended&&!owner->lobby_retiring&&!fixture::invalidated,"pending MonsterZone intent crossed Leave boundary");
    fixture::captures["zone_pending"]=false;
    fixture::captures["staff_pending"]=true;
    require(!runtime::begin_lobby_leave(error)&&error=="native lobby Leave blocked by retained active/pending UI capture intent"&&
        !fixture::suspended&&!owner->lobby_retiring&&!fixture::invalidated,"pending Developer intent crossed Leave boundary");
    fixture::captures["staff_pending"]=false;
    require(runtime::begin_lobby_leave(error),error);
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&!fixture::staff_retired,"active World retired Developer owner");
    fixture::title=true;fixture::staff_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&!ready&&error=="original Developer retirement readback failure"&&
        fixture::staff_retired==1&&!fixture::deleted&&runtime::published_runtime.load()==owner,
        "Developer retirement failure was hidden or callback storage deleted");
    runtime::published_runtime.store(nullptr);delete owner;
    fixture::reset();owner=new runtime::Runtime(pair.client);runtime::published_runtime.store(owner);
    require(runtime::begin_lobby_leave(error),error);fixture::title=true;
    require(runtime::finish_lobby_leave(ready,error)&&ready&&fixture::staff_retired==1&&fixture::deleted==1&&
        !runtime::published_runtime.load(),"confirmed Developer Title retirement did not release owner once");
    std::cout<<"PASS Developer Leave lifetime: pending intent, active World, exact ordering, original failure, confirmed retirement\n";
}
void bug_lifetime_delta(){
    Pair pair;fixture::reset();std::string error;bool ready=true;
    auto* owner=new runtime::Runtime(pair.client);runtime::published_runtime.store(owner);
    fixture::captures["bug_pending"]=true;
    require(!runtime::begin_lobby_leave(error)&&!fixture::suspended&&!fixture::invalidated&&!owner->lobby_retiring,
        "pending Bug intent crossed Leave boundary");
    fixture::captures["bug_pending"]=false;
    require(runtime::begin_lobby_leave(error),error);
    require(runtime::finish_lobby_leave(ready,error)&&!ready&&!fixture::bug_retired,"active World retired Bug owner");
    fixture::title=true;fixture::bug_ok=false;
    require(!runtime::finish_lobby_leave(ready,error)&&error=="original Bug owner retirement failure"&&
        !ready&&fixture::bug_retired==1&&!fixture::deleted&&runtime::published_runtime.load()==owner,
        "Bug retirement failure was hidden or callback storage deleted");
    runtime::published_runtime.store(nullptr);delete owner;
    fixture::reset();owner=new runtime::Runtime(pair.client);runtime::published_runtime.store(owner);
    require(runtime::begin_lobby_leave(error),error);fixture::title=true;
    require(runtime::finish_lobby_leave(ready,error)&&ready&&fixture::bug_retired==1&&fixture::deleted==1&&
        !runtime::published_runtime.load(),"confirmed Bug Title retirement did not release owner once");
    std::cout<<"PASS Bug Leave lifetime: pending intent, active World, retained failure and confirmed retirement\n";
}
void chat_retirement_delta(){
    Pair pair;fixture::reset();std::string error;
    auto* owner=new runtime::Runtime(pair.client);runtime::published_runtime.store(owner);
    fixture::captures["pending_ui"]=true;
    require(!runtime::begin_lobby_leave(error)&&fixture::chat_admitted&&!fixture::chat_closes,"rejected Leave must not close chat admission");
    fixture::captures["pending_ui"]=false;
    require(runtime::begin_lobby_leave(error)&&!fixture::chat_admitted&&fixture::chat_closes==1&&!fixture::wire_retired,"actual Leave closes chat before transport retirement");
    require(!runtime::begin_lobby_leave(error)&&fixture::chat_closes==1,"rejected Leave replay does not close chat twice");
    runtime::published_runtime.store(nullptr);delete owner;
}
int main(){
    try{
        chat_retirement_delta();
        bug_lifetime_delta();
        developer_lifetime_delta();
        locked_live_departure();
#ifndef TUTORIAL_LEAVE_DELTA_ONLY
#ifndef NAMED_WORLD_LEAVE_DELTA_ONLY
        idle_departure();completed_actions_and_reentry();incomplete_preparation();pending_and_unknown();other_guards();
#endif
        software_retirement_delta();
#endif
        tutorial_lifetime_delta();
        std::cout<<"client Leave regressions passed: actual Controller and actual begin/finish bodies\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
