#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace {
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* snapshot="22222222-2222-4222-8222-222222222222";
Ledger money(){Ledger v;v.cash=v.record_cash=100;v.record_present=1;v.flat_day=v.record_day=1;return v;}
ZoneState empty(){ZoneState v;v.content_sha256=std::string(64,'a');return v;}
ZoneIntent intent(){ZoneIntent v;v.region=67;v.character_type=0;v.level=1;v.stamps.push_back({{8020,0,8550},{{{1,2},{3,4},{5,6}}}});return v;}
struct SessionMock:Session {
    Context value;explicit SessionMock(uint64_t peer):value{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string&)override{out=value;return true;}
};
struct EngineMock:Engine {
    ZoneState row=empty();Ledger cash=money();Evidence receipt;unsigned executed=0,applied=0;
    bool reject=false,partial=false,wait=false,retirement_bad=false,region_drift=false,retirement_region_drift=false;uint32_t spawn=4;
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome execute_action(const Binding&,const Request& request,std::string& e)override{
        need(request.action==Action::PlaceMonsterZone&&request.zone==intent(),"wrong Host typed Zone");
        if(reject){e="Zone new stroke intersects an existing Zone";return Outcome::RejectedNoEffect;}
        ++executed;receipt={};receipt.before=cash;receipt.zone_before=row;
        ++row.zones;++row.undo_count;row.monsters+=spawn;row.next_tag+=int32_t(spawn);row.created_monsters+=int32_t(spawn);
        ++row.release_edits[5];row.content_sha256=std::string(64,'b');receipt.after=cash;receipt.zone_after=row;receipt.undo_confirmed=true;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request& request,const Evidence& host,std::string& e)override{
        need(request.action==Action::PlaceMonsterZone,"wrong Client action");
        if(!zone_shared_state(row,host.zone_before)||cash!=host.before){e="Zone Client BEFORE state differs";return Outcome::RejectedNoEffect;}
        ++applied;const auto local_before=row;const auto spawned=host.zone_after.monsters-host.zone_before.monsters;
        ++row.zones;++row.undo_count;row.monsters+=spawned;row.next_tag+=int32_t(spawned);row.created_monsters+=int32_t(spawned);
        ++row.release_edits[5];row.content_sha256=host.zone_after.content_sha256;
        cash=host.after;receipt=host;receipt.zone_before=local_before;receipt.zone_after=row;receipt.retirement_confirmed=false;
        if(region_drift){++row.region_random[0];receipt.zone_after=row;}
        if(partial){e="Zone original AddPaint partial failure";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{
        if(wait){e="Host BEFORE frame pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(retirement_bad)++receipt.zone_after.next_tag;if(retirement_region_drift)++receipt.zone_after.region_random[0];return true;}
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}
};
void flow(int mode){
    SessionMock hs(1),cs(2);EngineMock he,ce;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.retirement_bad=mode==4;he.spawn=mode==5?0:4;
    if(mode>=6){he.row.region_random={10238309950409534676ull,84209630741485398ull,17637438588495041383ull,8573256517064726746ull};
        ce.row.region_random={11031208541562408887ull,7264565988762834373ull,9860586388526540594ull,17295355788292699441ull};}
    ce.row.next_tag=7;
    ce.region_drift=mode==7;ce.retirement_region_drift=mode==8;const auto client_before=ce.row;
    Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    need(host.start(0,e)&&client.start(0,e),e);need(client.submit_zone(id,intent(),0,e),e);
    need(!he.executed&&!ce.applied&&ce.row==client_before&&ce.cash==money(),"UI submission caused local effect");const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing serialized Zone packet");auto p=packets.front();packets.pop_front();return (p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
    const auto accepted=deliver();
    if(mode==4){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&!host.status().confirmed&&host.status().cause.find("retirement")!=std::string::npos,"changed Zone retirement accepted");return;}
    need(accepted,e);if(mode==1){need(deliver(),e);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("intersects")!=std::string::npos,"Host reject cause/effects lost");return;}
    need(he.executed==1&&!ce.applied,"Host original did not precede Client application");const auto result=packets.front();const auto applied=deliver();
    if(mode==2||mode==7||mode==8){need(!applied&&ce.applied==1&&client.status().locked,"partial Zone effect escaped Unknown");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"partial Zone ACK became success");return;}
    need(applied,e);if(mode==3){need(!ce.applied&&packets.empty(),"missing BEFORE frame ACKed");ce.wait=false;need(client.tick(2,e),e);}
    need(ce.applied==1&&zone_shared_state(ce.row,he.row)&&ce.row.region_random==client_before.region_random&&ce.cash==he.cash&&ce.cash==money()&&!client.status().confirmed,"Client state/ledger/ACK ordering wrong");
    const auto ack_packet=packets.front();Message actual_ack;need(decode(ack_packet.bytes.data(),ack_packet.bytes.size(),actual_ack,e),e);
    need(actual_ack.evidence.zone_before.region_random==client_before.region_random&&actual_ack.evidence.zone_after.region_random==client_before.region_random,"wire ACK falsified Client Region RNG");
    need(actual_ack.evidence.zone_before.next_tag==7&&actual_ack.evidence.zone_before.created_monsters==0&&
        actual_ack.evidence.zone_after.next_tag==7+int32_t(he.spawn)&&actual_ack.evidence.zone_after.created_monsters==int32_t(he.spawn),"Zone ACK replaced local counters");
    if(mode==9){++actual_ack.evidence.zone_after.global_random[0];need(encode(actual_ack,packets.front().bytes,e),e);
        need(deliver(),e);need(host.status().locked&&!host.status().confirmed&&host.status().cause.find("differs from Host")!=std::string::npos,"Host accepted changed shared global RNG");return;}
    need(deliver(),e);need(deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1,"full peer ACK missing");
    need(host.receive(request.sender,request.bytes.data(),request.bytes.size(),3,e),e);need(client.receive(result.sender,result.bytes.data(),result.bytes.size(),3,e),e);
    if(mode==10){Message altered;need(decode(ack_packet.bytes.data(),ack_packet.bytes.size(),altered,e),e);
        ++altered.evidence.zone_before.region_random[0];++altered.evidence.zone_after.region_random[0];std::vector<uint8_t> bytes;need(encode(altered,bytes,e),e);
        need(!host.receive(2,bytes.data(),bytes.size(),3,e)&&e.find("conflicting duplicate native apply ACK")!=std::string::npos,"changed peer-local duplicate ACK accepted");}
    need(he.executed==1&&ce.applied==1,"duplicate Zone message replayed native effect");need(!client.submit_zone(id,intent(),4,e),"Zone UUID resubmitted");
}
}
int main(){try{for(int mode=0;mode<11;++mode)flow(mode);std::cout<<"11 actual serialized Zone Controller cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
