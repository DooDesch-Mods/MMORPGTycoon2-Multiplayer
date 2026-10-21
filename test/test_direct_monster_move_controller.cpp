#include "direct_monster_fixture.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace {
constexpr const char* id="33333333-3333-4333-8333-333333333333";
MonsterMoveIntent move_intent(){return {63,{8030,70.576828f,8560}};}
MonsterState moved(MonsterState v){++v.roster.undo_count;v.actor.position=move_intent().position;v.roster.content_sha256=std::string(64,'c');return v;}
constexpr const char* snapshot=snapshot_id;
struct SessionMock:Session {
    Context value;explicit SessionMock(uint64_t peer):value{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string&)override{out=value;return true;}
};
struct EngineMock:Engine {
    MonsterState row=born(empty());Ledger cash=money();Evidence receipt;unsigned executed=0,applied=0;
    bool reject=false,partial=false,wait=false,retirement_bad=false,region_drift=false,retirement_region_drift=false;uint32_t spawn=4;
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome execute_action(const Binding&,const Request& request,std::string& e)override{
        need(request.action==Action::MoveMonster&&request.monster_move==move_intent()&&request.target_action_uuid==action_id,"wrong Host typed Monster");
        if(reject){e="Monster new stroke intersects an existing Monster";return Outcome::RejectedNoEffect;}
        ++executed;receipt={};receipt.before=cash;receipt.monster_before=row;
        row=moved(row);if(!spawn)++row.roster.monsters;
        receipt.after=cash;receipt.monster_after=row;receipt.undo_confirmed=receipt.selection_confirmed=true;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request& request,const Evidence& host,std::string& e)override{
        need(request.action==Action::MoveMonster,"wrong Client action");
        if(!monster_shared_state(row,host.monster_before)||cash!=host.before){e="Monster Client BEFORE state differs";return Outcome::RejectedNoEffect;}
        ++applied;const auto local_before=row;row=moved(row);
        cash=host.after;receipt=host;receipt.monster_before=local_before;receipt.monster_after=row;receipt.retirement_confirmed=false;
        if(region_drift){++row.roster.region_random[0];receipt.monster_after=row;}
        if(partial){e="Monster original AddPaint partial failure";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{
        if(wait){e="Host BEFORE frame pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(retirement_bad)++receipt.monster_after.roster.next_tag;if(retirement_region_drift)++receipt.monster_after.roster.region_random[0];return true;}
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}
};
void flow(int mode){
    SessionMock hs(1),cs(2);auto host_engine=std::make_unique<EngineMock>(),client_engine=std::make_unique<EngineMock>();auto& he=*host_engine;auto& ce=*client_engine;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.retirement_bad=mode==4;he.spawn=mode==5?0:4;
    if(mode>=6){he.row.roster.region_random={10238309950409534676ull,84209630741485398ull,17637438588495041383ull,8573256517064726746ull};
        ce.row.roster.region_random={11031208541562408887ull,7264565988762834373ull,9860586388526540594ull,17295355788292699441ull};}
    ce.row.roster.next_tag=7;ce.row.actor.tag=-11752269;he.row.actor.tag=123;
    ce.region_drift=mode==7;ce.retirement_region_drift=mode==8;const auto client_before=ce.row;
    auto host_controller=std::make_unique<Controller>(hs,he,hw,Logger{}),client_controller=std::make_unique<Controller>(cs,ce,cw,Logger{});auto& host=*host_controller;auto& client=*client_controller;std::string e;
    need(host.start(0,e)&&client.start(0,e),e);need(client.submit_move_monster(id,action_id,move_intent(),0,e),e);
    need(!he.executed&&!ce.applied&&ce.row==client_before&&ce.cash==money(),"UI submission caused local effect");const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing serialized Monster packet");auto p=packets.front();packets.pop_front();return (p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
    const auto accepted=deliver();
    if(mode==4||mode==5){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&!host.status().confirmed&&(mode==5||host.status().cause.find("retirement")!=std::string::npos),"changed Monster retirement accepted");return;}
    need(accepted,e);if(mode==1){need(deliver(),e);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("intersects")!=std::string::npos,"Host reject cause/effects lost");return;}
    need(he.executed==1&&!ce.applied,"Host original did not precede Client application");const auto result=packets.front();const auto applied=deliver();
    if(mode==2||mode==7||mode==8){need(!applied&&ce.applied==1&&client.status().locked,"partial Monster effect escaped Unknown");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"partial Monster ACK became success");return;}
    need(applied,e);if(mode==3){need(!ce.applied&&packets.empty(),"missing BEFORE frame ACKed");ce.wait=false;need(client.tick(2,e),e);}
    need(ce.applied==1&&monster_shared_state(ce.row,he.row)&&ce.row.roster.region_random==client_before.roster.region_random&&ce.cash==he.cash&&ce.cash==money()&&!client.status().confirmed,"Client state/ledger/ACK ordering wrong");
    const auto ack_packet=packets.front();auto ack_storage=std::make_unique<Message>();auto& actual_ack=*ack_storage;need(decode(ack_packet.bytes.data(),ack_packet.bytes.size(),actual_ack,e),e);
    need(actual_ack.evidence.monster_before.roster.region_random==client_before.roster.region_random&&actual_ack.evidence.monster_after.roster.region_random==client_before.roster.region_random,"wire ACK falsified Client Region RNG");
    need(actual_ack.evidence.monster_before.roster.next_tag==client_before.roster.next_tag&&
        actual_ack.evidence.monster_before.roster.created_monsters==client_before.roster.created_monsters&&
        actual_ack.evidence.monster_before.actor.tag==client_before.actor.tag,"wire ACK falsified local serial/counters");
    if(mode==9){++actual_ack.evidence.monster_after.roster.global_random[0];need(encode(actual_ack,packets.front().bytes,e),e);
        need(deliver(),e);need(host.status().locked&&!host.status().confirmed&&host.status().cause.find("differs from Host")!=std::string::npos,"Host accepted changed shared global RNG");return;}
    need(deliver(),e);need(deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1,"full peer ACK missing");
    need(host.receive(request.sender,request.bytes.data(),request.bytes.size(),3,e),e);need(client.receive(result.sender,result.bytes.data(),result.bytes.size(),3,e),e);
    if(mode==10){auto altered_storage=std::make_unique<Message>();auto& altered=*altered_storage;need(decode(ack_packet.bytes.data(),ack_packet.bytes.size(),altered,e),e);
        ++altered.evidence.monster_before.roster.region_random[0];++altered.evidence.monster_after.roster.region_random[0];std::vector<uint8_t> bytes;need(encode(altered,bytes,e),e);
        need(!host.receive(2,bytes.data(),bytes.size(),3,e)&&e.find("conflicting duplicate native apply ACK")!=std::string::npos,"changed peer-local duplicate ACK accepted");}
    need(he.executed==1&&ce.applied==1,"duplicate Monster message replayed native effect");need(!client.submit_move_monster(id,action_id,move_intent(),4,e),"Monster UUID resubmitted");
}
}
int main(){try{for(int mode=0;mode<11;++mode)flow(mode);std::cout<<"11 actual serialized MoveMonster Controller cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
