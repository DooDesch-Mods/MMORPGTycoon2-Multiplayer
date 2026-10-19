#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
#include <deque>
using namespace mt2mp::action_session;
using json=nlohmann::json;
namespace {
void need(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
Message sample(Action direction,bool objects){
    Message m;m.kind=Kind::HostResult;m.session=1;m.revision=5;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=direction;r.peer=2;r.sequence=1;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.target_action_uuid="22222222-2222-4222-8222-222222222222";r.snapshot_uuid="33333333-3333-4333-8333-333333333333";
    r.canonical_record_key="terrain:"+r.target_action_uuid;
    auto& e=m.evidence;e.undo_confirmed=e.retirement_confirmed=true;e.region_before.id=56;e.region_before.preset="Forest";
    e.region_before.styles={"Forest","Forest","Forest","Forest"};e.region_before.scenery_count=10;
    e.region_before.scenery_sha256=std::string(64,'a');e.region_before.grid_sha256=std::string(64,'b');
    e.region_after=e.region_before;e.region_after.grid_sha256=std::string(64,'c');
    if(objects){e.region_after.scenery_count=direction==Action::UndoBuilding?12:8;e.region_after.scenery_sha256=std::string(64,'d');}
    e.terrain_before.contours_sha256=std::string(64,'a');e.terrain_before.bugs_sha256=std::string(64,'e');
    e.terrain_after=e.terrain_before;e.terrain_after.contours_sha256=std::string(64,'f');
    if(objects){e.region_after.random={1,2,3,4};e.terrain_after.global_random={5,6,7,8};e.terrain_after.bug_count=2;e.terrain_after.bugs_sha256=std::string(64,'d');}
    e.region_retired_sha256=std::string(64,'c');return m;
}
struct SessionPort:Session {
    Context value;
    SessionPort(uint64_t peer,const std::string& snapshot):value{1,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string&)override{out=value;return true;}
};
struct EnginePort:Engine {
    Message expected;Evidence receipt;RegionState row;unsigned executed=0,applied=0;int failure=0;
    explicit EnginePort(Message value):expected(std::move(value)),row(expected.evidence.region_before){}
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=expected.evidence.before;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong Terrain family");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong Terrain family");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong Terrain family");}
    Outcome execute_action(const Binding&,const Request& r,std::string&)override{
        need(r.action==expected.request.action&&r.canonical_record_key==expected.request.canonical_record_key&&r.target_action_uuid==expected.request.target_action_uuid,"wrong Terrain history request");
        ++executed;receipt=expected.evidence;receipt.retirement_confirmed=false;row=receipt.region_after;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& e)override{
        need(row==host.region_before,"Terrain Client before differs");++applied;row=host.region_after;receipt=host;receipt.retirement_confirmed=false;
        if(failure==2){e="partial Terrain original failure";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{
        receipt.retirement_confirmed=true;if(failure==1)receipt.terrain_after.contours_sha256=std::string(64,'b');return true;
    }
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;std::deque<Packet>& packets;
    Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}
};
void controller_flow(Action direction,bool objects,bool host_origin,int failure){
    const auto value=sample(direction,objects);SessionPort hs(1,value.request.snapshot_uuid),cs(2,value.request.snapshot_uuid);
    EnginePort he(value),ce(value);he.failure=failure==1?1:0;ce.failure=failure==2?2:0;
    std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    need(host.start(0,error)&&client.start(0,error),error);auto& origin=host_origin?host:client;
    need(origin.submit_building_history(direction,value.request.action_uuid,value.request.target_action_uuid,value.request.canonical_record_key,0,error),error);
    need(!ce.applied&&(!host_origin?!he.executed:he.executed==1),"Terrain input bypassed Host ordering");
    auto deliver=[&]{need(!packets.empty(),"missing Terrain packet");auto p=packets.front();packets.pop_front();return(p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,error);};
    Packet request_packet{};
    if(!host_origin){request_packet=packets.front();const auto ok=deliver();
        if(failure==1){need(host.status().locked&&!host.status().confirmed&&!ce.applied,"changed Terrain retirement accepted");return;}need(ok,error);}
    need(he.executed==1&&!ce.applied,"Terrain Host original ordering differs");const auto result_packet=packets.front();const auto applied=deliver();
    if(failure==2){need(!applied&&client.status().locked&&ce.applied==1,"partial Terrain operation escaped Unknown");deliver();need(host.status().locked&&!host.status().confirmed,"failed Terrain peer confirmed");return;}
    need(applied&&ce.applied==1&&he.row==ce.row&&!client.status().confirmed,error);
    if(failure==3){Message ack;need(decode(packets.front().bytes.data(),packets.front().bytes.size(),ack,error),error);
        ack.evidence.terrain_after.contours_sha256=std::string(64,'b');need(encode(ack,packets.front().bytes,error),error);
        deliver();need(host.status().locked&&!host.status().confirmed,"changed Terrain contour ACK accepted");return;}
    need(deliver()&&deliver(),error);need(host.status().confirmed==1&&client.status().confirmed==1,"Terrain full peer ACK missing");
    if(!host_origin)need(host.receive(request_packet.sender,request_packet.bytes.data(),request_packet.bytes.size(),3,error),error);
    need(client.receive(result_packet.sender,result_packet.bytes.data(),result_packet.bytes.size(),3,error),error);
    need(he.executed==1&&ce.applied==1&&!origin.submit_building_history(direction,value.request.action_uuid,value.request.target_action_uuid,value.request.canonical_record_key,4,error),"Terrain duplicate replayed original");
}
}
int main()try{
    unsigned cases=0;std::vector<uint8_t> bytes;std::string error;Message decoded;
    for(auto direction:{Action::UndoBuilding,Action::RedoBuilding})for(bool objects:{false,true}){
        const auto m=sample(direction,objects);
        need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==m,error);++cases;
        auto pending=m;pending.kind=Kind::Request;pending.revision=0;pending.required_mask=0;pending.outcome=Outcome::Pending;pending.evidence={};
        need(encode(pending,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==pending,error);++cases;
        auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,error),"invalid Terrain history accepted");++cases;};
        reject([](auto& x){x.request.target_action_uuid=x.request.action_uuid;});
        reject([](auto& x){x.request.canonical_record_key+="x";});
        reject([](auto& x){x.evidence.region_after.id++;});
        reject([](auto& x){x.evidence.region_after.preset="Desert";});
        reject([](auto& x){x.evidence.region_after.styles[2]="Desert";});
        reject([](auto& x){x.evidence.region_after.scenery_count=1000001;});
        reject([](auto& x){x.evidence.region_after.grid_sha256="bad";});
        reject([](auto& x){x.evidence.terrain_after.release_edits[4]++;});
        reject([](auto& x){x.evidence.terrain_after.height_cell=12;});
        reject([](auto& x){x.evidence.terrain_after.contours_sha256="bad";});
        reject([](auto& x){x.evidence.region_retired_sha256="bad";});
        reject([](auto& x){x.evidence.after.cash++;});
        reject([](auto& x){x.evidence.after.line_income[6]++;});
        reject([](auto& x){x.evidence.undo_confirmed=false;});
        reject([](auto& x){x.evidence.selection_confirmed=true;});
        reject([](auto& x){x.evidence.target_retired=true;});
        reject([](auto& x){x.evidence.property_before.content.id=56;});
        reject([](auto& x){x.evidence.path_before.paths=1;});
        reject([&](auto& x){x.evidence.region_after.scenery_count=direction==Action::UndoBuilding?9:11;});
        if(!objects){
            reject([](auto& x){x.evidence.region_after.scenery_sha256=std::string(64,'f');});
            reject([](auto& x){x.evidence.region_after.random[0]++;});
            reject([](auto& x){x.evidence.terrain_after.global_random[0]++;});
            reject([](auto& x){x.evidence.terrain_after.bug_count++;});
            reject([](auto& x){x.evidence.terrain_after.bugs_sha256=std::string(64,'f');});
        }
        need(encode(m,bytes,error),error);auto raw=json::parse(bytes);need(raw["schema"]==35,"Terrain history requires shared schema35");
        raw["schema"]=10;auto payload=raw.dump();need(!decode(reinterpret_cast<const uint8_t*>(payload.data()),payload.size(),decoded,error),"single-object history schema accepted Terrain");++cases;
        need(encode(m,bytes,error),error);raw=json::parse(bytes);raw["evidence"]["terrain_after"]["contours_sha256"]="bad";
        payload=raw.dump();need(!decode(reinterpret_cast<const uint8_t*>(payload.data()),payload.size(),decoded,error),"malformed Host contour receipt accepted");++cases;
        controller_flow(direction,objects,false,0);controller_flow(direction,objects,true,0);cases+=2;
        for(int failure=1;failure<=3;++failure){controller_flow(direction,objects,false,failure);++cases;}
    }
    std::cout<<cases<<" Terrain whole-graph history cases PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
