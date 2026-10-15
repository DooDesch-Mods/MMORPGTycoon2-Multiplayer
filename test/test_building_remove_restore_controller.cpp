#include "typed_action/action_session.h"
#include "multiplayer_world/world_state.h"
#include "building_history_slice/cash_effect.h"
#include <iostream>
#include <stdexcept>
namespace action=mt2mp::action_session;namespace world=mt2mp::world_state;
using namespace action;
#include "history_effect.inc"
namespace {
int checks=0;void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
const std::string placed="11111111-1111-4111-8111-111111111111",removed="22222222-2222-4222-8222-222222222222",undo="33333333-3333-4333-8333-333333333333",redo="44444444-4444-4444-8444-444444444444",snapshot="55555555-5555-4555-8555-555555555555";
BuildingState building(){BuildingState b;b.type="SmallInn";b.variant="base";b.alive=true;b.x=8;b.y=9;b.z=10;return b;}
Ledger initial(){Ledger l;l.cash=l.record_cash=94000;l.record_present=1;l.flat_day=l.record_day=2;return l;}
struct LocalSession:Session {Context value;explicit LocalSession(uint64_t peer):value{71,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=value;return true;}};
struct Native:Engine {
    BuildingState state=building();Ledger money=initial();Evidence receipt;unsigned effects=0;bool partial=false,reject=false;
    Native(){state.alive=false;}
    bool bind(const std::string&,Binding& b,std::string&)override{b={1,2};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& l,std::string&)override{l=money;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{return false;}
    Outcome execute_action(const Binding&,const Request& r,std::string& e)override{
        if(reject){e="actual retained graph not stack top";return Outcome::RejectedNoEffect;}
        ++effects;receipt={};receipt.before=money;receipt.object_before=state;state.alive=r.action==Action::UndoBuilding;
        Ledger after;need(mt2mp::game::building_history_slice::cash_effect(money,state.alive?-400:400,after,e),e);money=after;
        receipt.after=money;receipt.object_after=state;receipt.target_retired=!state.alive;receipt.undo_confirmed=true;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& e)override{
        if(host.before!=money||host.object_before!=state){e="Client full before differs";return Outcome::RejectedNoEffect;}
        ++effects;receipt=host;receipt.retirement_confirmed=false;money=host.after;state=host.object_after;
        if(partial){e="actual fresh native receiver readback failed";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;return true;}
};
struct Packet{uint64_t peer,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {uint64_t peer;std::deque<Packet>& queue;Wire(uint64_t p,std::deque<Packet>& q):peer(p),queue(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{queue.push_back({peer,to,bytes});return true;}};
void flow(bool baseline,int failure){
    std::deque<Packet> packets;LocalSession hs(1),cs(2);Native he,ce;Wire hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    const auto key=baseline?std::string(64,'a')+":world:building:44":"action:"+placed;
    he.reject=failure==1;ce.partial=failure==2;
    need(host.start(0,e)&&client.start(0,e),e);
    need(client.submit_building_history(Action::UndoBuilding,undo,removed,key,0,e),e);
    need(he.effects==0&&ce.effects==0,"intent executed before Host order");
    Packet request=packets.front();
    for(unsigned i=0;!packets.empty()&&i<16;++i){auto p=packets.front();packets.pop_front();(p.target==1?host:client).receive(p.peer,p.bytes.data(),p.bytes.size(),1,e);}
    if(failure==1){need(he.effects==0&&ce.effects==0&&client.status().outcome==Outcome::RejectedNoEffect,"Host rejection became effect");need(client.status().cause.find("not stack top")!=std::string::npos,"Host cause lost");return;}
    if(failure==2){need(he.effects==1&&ce.effects==1&&host.status().locked&&host.status().confirmed==0,"partial Client falsely acknowledged");host.receive(request.peer,request.bytes.data(),request.bytes.size(),2,e);need(he.effects==1&&ce.effects==1,"partial request replayed");return;}
    need(he.effects==1&&ce.effects==1&&host.status().confirmed==1&&client.status().confirmed==1&&host.status().ack_mask==3,"Restore lacked both real apply/retire ACKs host="+host.status().cause+" client="+client.status().cause+" effects="+std::to_string(he.effects)+"/"+std::to_string(ce.effects)+" confirmed="+std::to_string(host.status().confirmed)+"/"+std::to_string(client.status().confirmed)+" ack="+std::to_string(host.status().ack_mask));
    need(he.money==ce.money&&he.state==ce.state&&he.state.alive,"Restore peers differ");
    host.receive(request.peer,request.bytes.data(),request.bytes.size(),2,e);
    for(unsigned i=0;!packets.empty()&&i<16;++i){auto p=packets.front();packets.pop_front();(p.target==1?host:client).receive(p.peer,p.bytes.data(),p.bytes.size(),2,e);}
    need(he.effects==1&&ce.effects==1,"duplicate executed original again");
    need(host.submit_building_history(Action::RedoBuilding,redo,removed,key,3,e),e);
    for(unsigned i=0;!packets.empty()&&i<16;++i){auto p=packets.front();packets.pop_front();(p.target==1?host:client).receive(p.peer,p.bytes.data(),p.bytes.size(),4,e);}
    need(he.effects==2&&ce.effects==2&&host.status().confirmed==2&&client.status().confirmed==2&&host.status().ack_mask==3&&!he.state.alive&&he.money==ce.money,"Redo Remove order/evidence/ACK differs");
}
}
int main(){try{std::string e;for(bool baseline:{false,true})for(int fail=0;fail<3;++fail)flow(baseline,fail);
    Request r;r.action=Action::UndoBuilding;Evidence effect;effect.object_after=building();effect.object_before=effect.object_after;effect.object_before.alive=false;
    need(object_effect(r,effect),"Undo Remove restoration rejected");r.action=Action::RedoBuilding;std::swap(effect.object_before,effect.object_after);need(object_effect(r,effect),"Redo Remove destruction rejected");effect.object_after.x+=1;need(!object_effect(r,effect),"history changed full pose accepted");
    Message m;m.kind=Kind::Request;m.session=7;m.request={};m.request.action=Action::UndoBuilding;m.request.peer=2;m.request.sequence=1;m.request.action_uuid=undo;m.request.snapshot_uuid=snapshot;m.request.target_action_uuid=removed;m.request.canonical_record_key="action:"+placed;
    std::vector<uint8_t> bytes;need(encode(m,bytes,e),e);Message out;need(decode(bytes.data(),bytes.size(),out,e)&&out==m,"request wire did not retain mutation UUID and shared object key separately");m.request.canonical_record_key="bad";need(!encode(m,bytes,e),"malformed shared key encoded");
    const world::Origin origin{snapshot,std::string(64,'a'),1,2};const auto key=origin.sha256+":world:building:44";world::Table table;need(table.bind_snapshot(origin,{{key,key,building()}},0,e),e);need(table.retire_native_effects(origin,1,removed,{key},e),e);world::LocalBinding binding{origin,key,999,building()};need(table.restore_native_building_key(origin,2,undo,key,binding,e),e);need(table.rows().size()==1&&table.rows().at(key).state.alive,"baseline key identity changed");need(!table.restore_native_building_key(origin,3,redo,key,binding,e)&&table.revision()==2,"duplicate restore advanced revision");
    std::cout<<"PASS "<<checks<<" serialized Controller/Table assertions; native effects are synthetic\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
