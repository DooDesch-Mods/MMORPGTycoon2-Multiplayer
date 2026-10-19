#include "typed_action/action_session.h"
#include <atomic>
#include <iostream>
#include <stdexcept>

namespace action=mt2mp::action_session;
namespace fixture {
unsigned assertions=0;uint32_t actual_thread=7;
const std::string snapshot="11111111-1111-4111-8111-111111111111";
const std::string action_id="22222222-2222-4222-8222-222222222222";
const std::string other_id="33333333-3333-4333-8333-333333333333";
void need(bool okay,const std::string& cause){++assertions;if(!okay)throw std::runtime_error(cause);}
}
uint32_t GetCurrentThreadId(){return fixture::actual_thread;}
namespace building_mixed_identity::opened_file {struct CurrentWorld {uint64_t world_epoch=1,generation=2;};}
namespace native_open_runtime {
building_mixed_identity::opened_file::CurrentWorld world;
bool available=true;unsigned reads=0;
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string& error){
    ++reads;if(!available){error="original world unavailable";return false;}out=world;return true;
}
}
namespace fixture {
struct Session final:action::Session {
    action::Context value;bool available=true;
    explicit Session(uint64_t peer):value{91,peer,1,3,peer==1?action::Role::Host:action::Role::Client,snapshot}{}
    bool current(action::Context& out,std::string& error)override{if(!available){error="original session unavailable";return false;}out=value;return true;}
};
struct Packet {uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Transport final:action::Transport {
    uint64_t peer;std::deque<Packet>& packets;
    Transport(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,to,bytes});return true;}
};
struct Live {
    bool legacy=false,drift=false;unsigned captures=0;
    bool capture_before(const action::Request&,const action::Ledger&,uint64_t,std::string&);
};
struct Engine final:action::Engine {
    uint32_t thread_=7;Live* live_state_=nullptr;unsigned attempts=0;bool unknown=false;
    bool bind(const std::string&,action::Binding& out,std::string&)override{out={1,2};return true;}
#include "binding_native_current.inc"
#include "binding_authority_capture.inc"
    bool ledger(const action::Binding&,action::Ledger& out,std::string&)override{out={};out.cash=out.record_cash=10000;out.record_present=1;return true;}
    action::Outcome execute_remove(const action::Binding&,const action::Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    action::Outcome apply_remove(const action::Binding&,const action::Request&,const action::Evidence&,std::string&)override{throw std::runtime_error("unexpected Client apply");}
    bool observe_remove(const action::Binding&,const action::Request&,action::Evidence&,std::string&)override{throw std::runtime_error("unexpected observation");}
    bool retire(const action::Binding&,const action::Request&,std::string&)override{throw std::runtime_error("unexpected retirement");}
    action::Outcome execute_action(const action::Binding&,const action::Request&,std::string& error)override{
        ++attempts;error=unknown?"fixture original effect uncertain":"fixture stops before native effect";
        return unknown?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
    }
};
}
namespace mt2mp::game::typed_action_runtime {
struct Runtime {
    fixture::Session session;fixture::Engine engine;fixture::Transport transport;action::Controller controller;
    fixture::Live live;uint32_t thread=7;bool lobby_retiring=false;
    Runtime(uint64_t peer,std::deque<fixture::Packet>& packets):session(peer),transport(peer,packets),controller(session,engine,transport,{}){engine.live_state_=&live;}
};
std::atomic<Runtime*> published_runtime{nullptr};
#include "binding_queries.inc"
}
namespace runtime=mt2mp::game::typed_action_runtime;
bool fixture::Live::capture_before(const action::Request& request,const action::Ledger&,uint64_t previous,std::string& error){
    ++captures;const auto* owner=runtime::published_runtime.load();const auto state=owner->controller.status();
    need(state.pending&&!state.locked&&state.action_uuid==request.action_uuid&&previous==0,"actual Controller did not enter pending before Engine authority capture");
    action::Binding out{99,99};std::string denied;
    const auto reads=native_open_runtime::reads;
    need(!runtime::network_binding(snapshot,out,denied)&&out==action::Binding{}&&denied=="network admission requires idle unlocked Controller","new admission must remain blocked during pending capture");
    need(native_open_runtime::reads==reads,"pending new admission reached native query");
    if(drift)++native_open_runtime::world.generation;
    return legacy?runtime::network_binding(snapshot,out,error):runtime::read_binding(snapshot,out,error);
}
namespace {
void query_guards(){using namespace fixture;
    std::deque<Packet> packets;runtime::Runtime host(1,packets);action::Binding out{8,9};std::string error;
    runtime::published_runtime=nullptr;
    need(!runtime::read_binding(snapshot,out,error)&&out==action::Binding{},"absent Runtime leaked a binding");
    runtime::published_runtime=&host;
    need(!runtime::read_binding(snapshot,out,error)&&error=="binding read requires started unlocked Controller","unstarted query admitted");
    need(host.controller.start(0,error),error);
    error="stale failure";need(runtime::read_binding(snapshot,out,error)&&out==action::Binding{1,2}&&error.empty(),"idle read failed or retained stale error");
    need(runtime::network_binding(snapshot,out,error),error);
    actual_thread=8;need(!runtime::read_binding(snapshot,out,error)&&out==action::Binding{},"wrong thread read admitted");actual_thread=7;
    host.lobby_retiring=true;need(!runtime::read_binding(snapshot,out,error),"retiring read admitted");host.lobby_retiring=false;
    host.session.available=false;need(!runtime::read_binding(snapshot,out,error)&&error=="original session unavailable","Session failure lost");host.session.available=true;
    need(!runtime::read_binding(other_id,out,error)&&out==action::Binding{},"foreign shared snapshot read admitted");
    const auto saved=host.session.value;host.session.value.session++;
    need(!runtime::read_binding(snapshot,out,error),"foreign session read admitted");host.session.value=saved;
    host.session.value.role=action::Role::Client;need(!runtime::read_binding(snapshot,out,error),"foreign role read admitted");host.session.value=saved;
    native_open_runtime::available=false;need(!runtime::read_binding(snapshot,out,error)&&error=="original world unavailable","native original cause lost");native_open_runtime::available=true;
    native_open_runtime::world.world_epoch=2;need(!runtime::read_binding(snapshot,out,error)&&out==action::Binding{},"foreign native epoch admitted");native_open_runtime::world.world_epoch=1;
    native_open_runtime::world.generation=3;need(!runtime::read_binding(snapshot,out,error)&&out==action::Binding{},"foreign native generation admitted");native_open_runtime::world.generation=2;
    need(runtime::read_binding(snapshot,out,error)&&error.empty(),"read did not recover after corrected provenance");
    runtime::published_runtime=nullptr;
}
void pending_capture(bool legacy,bool drift,bool unknown){using namespace fixture;
    native_open_runtime::world={1,2};std::deque<Packet> packets;runtime::Runtime host(1,packets),client(2,packets);std::string error;
    runtime::published_runtime=&host;host.live.legacy=legacy;host.live.drift=drift;host.engine.unknown=unknown;
    need(host.controller.start(0,error)&&client.controller.start(0,error),error);
    action::BuildingState building;building.type="SmallInn";building.variant="base";building.alive=true;
    need(client.controller.submit_place(action_id,building,0,error),error);
    need(packets.size()==1&&host.engine.attempts==0,"Client intent executed before Host order");
    const auto request=packets.front();packets.pop_front();
    host.controller.receive(request.sender,request.bytes.data(),request.bytes.size(),1,error);
    const auto state=host.controller.status();
    need(host.live.captures==1,"actual Host authority callback absent or repeated");
    if(legacy||drift){
        need(host.engine.attempts==0&&state.attempted==0&&state.outcome==action::Outcome::RejectedNoEffect,"failed before-capture became an effect");
        need(state.cause.find(legacy?"network admission requires idle unlocked Controller":"typed native action bound world changed")!=std::string::npos,"before-capture original cause lost");
    }else{
        need(host.engine.attempts==1&&state.attempted==1,"pending capture did not reach ordered execution once");
        need(state.cause.find(unknown?"fixture original effect uncertain":"fixture stops before native effect")!=std::string::npos,"ordered outcome lost after read");
        action::Binding checked;
        need(unknown?!runtime::read_binding(snapshot,checked,error):runtime::read_binding(snapshot,checked,error),"Unknown lock or unlocked recovery changed");
        need(state.locked==unknown,"Unknown outcome lock changed");
    }
    native_open_runtime::world={1,2};host.controller.receive(request.sender,request.bytes.data(),request.bytes.size(),2,error);
    need(host.live.captures==1&&host.engine.attempts==((legacy||drift)?0U:1U),"duplicate replayed pending authority capture or native attempt");
    runtime::published_runtime=nullptr;
}
}
int main()try{
    query_guards();pending_capture(true,false,false);pending_capture(false,false,false);pending_capture(false,true,false);pending_capture(false,false,true);
    std::cout<<fixture::assertions<<" pending binding assertions passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
