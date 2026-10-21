#include "dungeon_building_fixture.h"
#include <iostream>
namespace dungeon_building_controller_fixture {
using namespace dungeon_building_fixture;
struct SessionPort:Session {Context value;explicit SessionPort(uint64_t peer):value{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot_id}{}
    bool current(Context& out,std::string&)override{out=value;return true;}};
struct EnginePort:Engine {
    bool world=false;int mode=0;unsigned executed=0,applied=0;Ledger current_money=money();Evidence receipt;
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=current_money;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("Wrong untyped remove route");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("Wrong untyped Client route");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("Wrong untyped observation route");}
    Outcome execute_action(const Binding&,const Request& request,std::string& error)override{
        need(request.dungeon_building&&request.dungeon_scene==(world?"world":std::string("action:")+source_id+"/template"),"Dungeon request lost exact Scene owner");
        if(mode==1){error="Original Dungeon source owner unavailable";return Outcome::RejectedNoEffect;}
        ++executed;receipt=message(request.action,world,true).evidence;receipt.retirement_confirmed=false;current_money=receipt.after;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request& request,const Evidence& authority,std::string& error)override{
        need(request.dungeon_building&&authority.dungeon_building_effect&&authority.dungeon_building_effect->mapping.operation==request.action_uuid,"Client lost full ordered Dungeon mapping");
        ++applied;receipt=authority;receipt.retirement_confirmed=false;current_money=receipt.after;
        if(mode==2){error="Original Dungeon replay returned partial Unknown";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;
        if(mode==3)receipt.dungeon_building_effect->after.references=std::string(64,'f');return true;}
};
struct Packet {uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}};
void flow(Action action,bool world,bool host_origin,int mode){SessionPort hs(1),cs(2);
    auto he=std::make_unique<EnginePort>(),ce=std::make_unique<EnginePort>();he->world=ce->world=world;he->mode=mode==1||mode==3?mode:0;ce->mode=mode==2?2:0;
    std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    auto host=std::make_unique<Controller>(hs,*he,hw,Logger{}),client=std::make_unique<Controller>(cs,*ce,cw,Logger{});std::string error;
    need(host->start(0,error)&&client->start(0,error),error);auto& sender=host_origin?*host:*client;const auto input=request(action,world);
    const bool submitted=sender.submit_dungeon_building(action_id,action,input.canonical_record_key,input.dungeon_scene,input.building,input.target_action_uuid,0,error);
    need(submitted||(mode==3&&host_origin&&host->status().locked),error);need(!ce->applied,"Capture executed Client before Host ordering");
    if(!host_origin)need(!he->executed,"Capture executed Host before request delivery");
    std::optional<Packet> request_packet,result_packet;size_t delivered=0;
    while(!packets.empty()){need(++delivered<=8,"Unbounded controlled Dungeon packet exchange");auto packet=packets.front();packets.pop_front();
        auto value=std::make_unique<Message>();need(decode(packet.bytes.data(),packet.bytes.size(),*value,error),error);
        if(value->kind==Kind::Request)request_packet=packet;if(value->kind==Kind::HostResult)result_packet=packet;
        if(mode==4&&value->kind==Kind::ApplyAck&&value->outcome==Outcome::Confirmed){value->evidence.dungeon_building_effect->after.references=std::string(64,'f');need(encode(*value,packet.bytes,error),error);}
        const bool ok=(packet.target==1?*host:*client).receive(packet.sender,packet.bytes.data(),packet.bytes.size(),1,error);
        if(!ok)need((mode==2&&client->status().locked)||((mode==3||mode==4)&&host->status().locked),"Unexpected Dungeon action delivery failure: "+error);
    }
    if(mode==1){need(!he->executed&&!ce->applied&&sender.status().outcome==Outcome::RejectedNoEffect&&sender.status().cause.find("source owner unavailable")!=std::string::npos,"Rejected original lost cause or entered engine");return;}
    if(mode==2){need(he->executed==1&&ce->applied==1&&host->status().locked&&client->status().locked&&!host->status().confirmed,"Partial Client Dungeon effect escaped Unknown");return;}
    if(mode==3){need(he->executed==1&&!ce->applied&&host->status().locked&&!host->status().confirmed,"Changed final reference proof was confirmed");return;}
    if(mode==4){need(he->executed==1&&ce->applied==1&&host->status().locked&&!host->status().confirmed,"Changed Client full Scene ACK confirmed");return;}
    need(he->executed==1&&ce->applied==1&&host->status().confirmed==1&&client->status().confirmed==1&&he->receipt==ce->receipt,"Full Scene/reference/inverse proof did not confirm both peers once");
    if(request_packet){const auto& packet=*request_packet;need(host->receive(packet.sender,packet.bytes.data(),packet.bytes.size(),3,error),error);}
    if(result_packet){const auto& packet=*result_packet;need(client->receive(packet.sender,packet.bytes.data(),packet.bytes.size(),3,error),error);}
    need(he->executed==1&&ce->applied==1,"Duplicate delivery replayed Dungeon engine call");
    need(!sender.submit_dungeon_building(action_id,action,input.canonical_record_key,input.dungeon_scene,input.building,input.target_action_uuid,4,error),"Retired Dungeon UUID was submitted twice");
}
}
int main()try{using namespace dungeon_building_controller_fixture;unsigned cases=0;
    for(const auto action:{Action::PlaceBuilding,Action::MoveBuilding,Action::RemoveSnapshotBuilding,Action::UndoBuilding,Action::RedoBuilding})
        for(bool world:{false,true})for(bool host:{false,true})for(int mode=0;mode<5;++mode){flow(action,world,host,mode);++cases;}
    std::cout<<cases<<" actual Dungeon Building Controller/ACK/full-proof/Unknown/no-replay flows passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
