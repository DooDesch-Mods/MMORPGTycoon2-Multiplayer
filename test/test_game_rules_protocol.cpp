#include "typed_action/action_session.h"
#include "live_host_state/state.h"
#include "mt2mp/core/hash.h"
#include <deque>
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace gr=mt2mp::game::game_rules;namespace live=mt2mp::game::live_host_state;namespace gm=mt2mp::game::gm_ticket;namespace tutorial=mt2mp::game::tutorial_replication;
namespace {
unsigned checks=0;const char* snapshot="10000000-0000-0000-0000-000000000001";const char* fixture_action_id="20000000-0000-0000-0000-000000000002";
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
gr::State state(){gr::State out;out.xp[0]=0xaabbccdd;out.xp[1]=10;out.respawn_seconds=60;out.available={1,1};return out;}
GameRulesIntent intent(gr::Command command){GameRulesIntent out{command,state(),state()};switch(command){case gr::Command::ApplyXP:out.after.xp[2]=3;break;case gr::Command::RespawnSeconds:out.after.respawn_seconds=90;break;case gr::Command::RespawnDelay:out.after.respawn_delay=true;break;case gr::Command::Features:out.after.features[0]=1;break;}return out;}
GameRulesProof proof(){return {state(),std::string(64,'a')};}
struct S:Session {Context c;S(uint64_t peer):c{77,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=c;return true;}};
struct E:Engine {
    GameRulesProof state=proof();Ledger cash;Evidence receipt;int executed=0,applied=0,mode=0;
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,9};return true;}bool current(const Binding&,const std::string&,std::string&)override{return true;}bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome execute_action(const Binding&,const Request& request,std::string& error)override{need(request.action==Action::GameRules&&request.game_rules.before==state.state,"wrong Host action/before");if(mode==4){error="original current native Design prerequisite differs";return Outcome::RejectedNoEffect;}++executed;receipt={};receipt.before=receipt.after=cash;receipt.game_rules_before=state;state.state=request.game_rules.after;if(request.game_rules.command==gr::Command::Features)state.tutorial_sha256=std::string(64,'b');receipt.game_rules_after=state;return Outcome::Confirmed;}
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& error)override{need(state==host.game_rules_before&&cash==host.before,"Client full rules/tutorial/Ledger before differs");++applied;receipt=host;receipt.retirement_confirmed=false;state=*host.game_rules_after;if(mode==2){state.state.xp[0]^=1;receipt.game_rules_after=state;}if(mode==1){error="original Features cascade interrupted";return Outcome::Unknown;}return Outcome::Confirmed;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)receipt.game_rules_after->tutorial_sha256=std::string(64,'c');return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct W:Transport {uint64_t peer;std::deque<Packet>& packets;W(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,to,bytes});return true;}};
void flow(gr::Command command,int mode){S hs(1),cs(2);E he,ce;ce.mode=mode==1||mode==2?mode:0;he.mode=mode==3||mode==4?mode:0;std::deque<Packet> packets;W hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    need(host.start(0,error)&&client.start(0,error),error);need(client.submit_game_rules(fixture_action_id,intent(command),0,error),error);need(!he.executed&&!ce.applied,"capture executed original early");const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing protocol packet");const auto p=packets.front();packets.pop_front();return(p.to==1?host:client).receive(p.from,p.bytes.data(),p.bytes.size(),1,error);};
    bool accepted=deliver();if(mode==3){need(host.status().locked&&!host.status().confirmed&&host.status().cause.find("retirement")!=std::string::npos,"changed complete retirement proof admitted");return;}need(accepted,error);
    if(mode==4){need(deliver(),error);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect,"no-effect original cause lost");return;}
    const auto result=packets.front();accepted=deliver();if(mode==1||mode==2){need(!accepted&&client.status().locked&&!client.status().confirmed,"partial/foreign rule effect acknowledged");need(deliver(),error);need(host.status().locked&&!host.status().confirmed,"failed ApplyAck became success");return;}
    need(deliver()&&deliver(),error);need(host.status().ack_mask==3&&host.status().confirmed==1&&client.status().confirmed==1&&ce.state==he.state,"complete HostResult/ACK absent");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,error)&&client.receive(result.from,result.bytes.data(),result.bytes.size(),2,error),error);need(he.executed==1&&ce.applied==1,"duplicate replayed original cascade");need(!client.submit_game_rules(fixture_action_id,intent(command),3,error),"retired token resubmitted");
}
void codec(){for(auto command:{gr::Command::ApplyXP,gr::Command::RespawnSeconds,gr::Command::RespawnDelay,gr::Command::Features}){Message value;value.session=77;value.request.action=Action::GameRules;value.request.peer=2;value.request.sequence=1;value.request.action_uuid=fixture_action_id;value.request.snapshot_uuid=snapshot;value.request.canonical_record_key="management:game-rules";value.request.game_rules=intent(command);
    std::vector<uint8_t> bytes;std::string error;Message decoded;need(encode(value,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&value==decoded,error);auto wire=nlohmann::json::parse(bytes);need(wire.at("schema")==35,"Design action requires coherent35");
    auto reject=[&](nlohmann::json bad){const auto text=bad.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"malformed Design wire accepted");};
    auto bad=wire;bad["request"]["game_rules"]["command"]=0;reject(bad);bad=wire;bad["request"]["game_rules"]["after"]["xp"][0]=0;reject(bad);bad=wire;bad["request"]["game_rules"]["extra"]=0;reject(bad);bad=wire;bad["request"]["canonical_record_key"]="management:business";reject(bad);bad=wire;bad["schema"]=34;reject(bad);
    value.kind=Kind::HostResult;value.outcome=Outcome::Confirmed;value.revision=value.request.host_revision=1;value.required_mask=3;value.evidence.game_rules_before=proof();value.evidence.game_rules_after=proof();value.evidence.game_rules_after->state=value.request.game_rules.after;if(command==gr::Command::Features)value.evidence.game_rules_after->tutorial_sha256=std::string(64,'b');value.evidence.retirement_confirmed=true;
    need(encode(value,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==value,error);bad=nlohmann::json::parse(bytes);bad["evidence"]["game_rules_after"]["rules"]["xp"][0]=0;reject(bad);
    bad=nlohmann::json::parse(bytes);bad["evidence"]["game_rules_after"]["tutorial_sha256"]="not-a-digest";reject(bad);
}}
gm::Publication publication(){gm::Publication out;out.queue.kind=mt2mp::game::release_graph::Kind::GameMasterTickets;out.queue.records.push_back({SIZE_MAX,0,{1,0,"vsObjectArray<mmoTicket>"},{}});return out;}
void frame(){live::Frame value;value.session=77;value.epoch=3;value.source_generation=9;value.revision=1;value.snapshot=snapshot;value.values.calendar={1,1,1,0,0,7};value.management.emplace();value.management->gm=publication();value.management->policies.emplace();value.management->rules=state();value.tutorial.emplace();
    std::vector<uint8_t> bytes;std::string error;live::Frame decoded;need(live::encode(value,bytes,error)&&live::decode(bytes.data(),bytes.size(),decoded,error)&&decoded==value,error);const auto wire=nlohmann::json::parse(bytes);need(wire.at("schema")==23,"complete Design must selectFrame23");
    auto reject=[&](nlohmann::json bad){const auto text=bad.dump();need(!live::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"malformed Design full Frame accepted");};
    auto bad=wire;bad["schema"]=22;reject(bad);for(const char* name:{"rules","policies","gm"}){bad=wire;bad["management"].erase(name);reject(bad);}bad=wire;bad["management"]["rules"]["respawn_delay"]=1;reject(bad);
    Message result;result.kind=Kind::HostResult;result.outcome=Outcome::Confirmed;result.session=77;result.revision=1;result.request.snapshot_uuid=snapshot;result.request.action_uuid=fixture_action_id;result.evidence.before=live::ledger(value.values);result.evidence.game_rules_before=proof();
    const auto text=tutorial::encode(*value.tutorial).dump();result.evidence.game_rules_before->tutorial_sha256=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));value.for_action=fixture_action_id;Context client{77,2,1,3,Role::Client,snapshot};
    need(live::admit_before_result(1,client,3,9,nullptr,value,result,0,0,error)==live::Admission::New,error);value.management->rules->xp[0]^=1;
    need(live::admit_before_result(1,client,3,9,nullptr,value,result,0,0,error)==live::Admission::Rejected,"action-before incomplete rules accepted");value.management->rules=state();value.tutorial->initial_complete=true;
    need(live::admit_before_result(1,client,3,9,nullptr,value,result,0,0,error)==live::Admission::Rejected,"action-before different tutorial event cascade accepted");value.tutorial.reset();
    need(live::admit_before_result(1,client,3,9,nullptr,value,result,0,0,error)==live::Admission::Rejected,"action-before missing tutorial accepted");
}
}
int main()try{codec();frame();for(auto command:{gr::Command::ApplyXP,gr::Command::RespawnSeconds,gr::Command::RespawnDelay,gr::Command::Features})for(int mode=0;mode<5;++mode)flow(command,mode);std::cout<<checks<<" Design rules Controller/original cascade proof/Frame23 assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
