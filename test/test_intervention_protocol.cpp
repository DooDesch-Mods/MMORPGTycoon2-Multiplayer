#include "typed_action/action_session.h"
#include <deque>
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace i=mt2mp::game::interventions;
namespace {
unsigned checks=0;const std::string snapshot="10000000-0000-0000-0000-000000000001",token="20000000-0000-0000-0000-000000000002",sha(64,'a');
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
i::Target target(i::Family family){return {family,family==i::Family::Boss?sha+":world:boss:4":family==i::Family::Toon?"subscriber:17":"action:"+token};}
i::Proof proof(i::Family family){i::Proof out;out.target=target(family);out.actor={sha,5,6,2};if(family==i::Family::Toon)out.subscriber=out.actor;out.actors_sha=out.zones_sha=out.economy_sha=out.gm_sha=out.marketing_sha=out.tutorial_sha=sha;return out;}
struct S:Session{Context c;S(uint64_t peer):c{77,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=c;return true;}};
struct E:Engine{
    i::Proof state;Ledger cash;Evidence receipt;int executed=0,applied=0,mode=0;
    explicit E(i::Family family):state(proof(family)){}
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,9};return true;}bool current(const Binding&,const std::string&,std::string&)override{return true;}bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome execute_action(const Binding&,const Request& request,std::string& error)override{
        need(request.action==Action::Intervention&&request.intervention.target==state.target&&request.intervention.before_sha==state.actor.sha,"Host intervention captured wrong complete original target/before");
        if(mode==4){error="original native Command prerequisite differs";return Outcome::RejectedNoEffect;}
        ++executed;receipt={};receipt.before=receipt.after=cash;receipt.intervention_before=state;
        if(i::control_command(request.intervention.command)){
            const auto& input=*request.intervention.control;auto& value=*state.control;
            if(request.intervention.command==i::Command::ControlAcquire){value.controlled=true;value.peer=request.peer;value.lease=input.lease;}
            else if(request.intervention.command==i::Command::ControlRelease)value={};
            else if(request.intervention.command==i::Command::ControlInput){value.sequence=input.sequence;value.jump=input.jump;value.selected=input.selected;}
        }else if(mode!=5){state.actor.sha=std::string(64,'b');state.actors_sha=state.actor.sha;if(state.subscriber)state.subscriber->sha=state.actor.sha;}
        receipt.intervention_after=state;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& error)override{
        need(host.intervention_before&&host.intervention_after&&state==*host.intervention_before&&cash==host.before,"Client complete current Actor/Subscriber/cascade/Ledger differs before original");
        ++applied;receipt=host;receipt.retirement_confirmed=false;state=*host.intervention_after;
        if(mode==2){state.zones_sha=std::string(64,'c');receipt.intervention_after=state;}
        if(mode==1){error="original intervention native cascade interrupted";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)receipt.intervention_after->marketing_sha=std::string(64,'d');return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct W:Transport{uint64_t peer;std::deque<Packet>& queue;W(uint64_t p,std::deque<Packet>& q):peer(p),queue(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{queue.push_back({peer,to,bytes});return true;}};
void flow(i::Family family,i::Command command,int mode){
    S hs(1),cs(2);E he(family),ce(family);he.mode=mode==3||mode==4||mode==5?mode:0;ce.mode=mode==1||mode==2?mode:0;
    std::deque<Packet> packets;W hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    need(host.start(0,error)&&client.start(0,error),error);const i::Intent intent{target(family),command,sha};need(client.submit_intervention(token,intent,0,error),error);need(!he.executed&&!ce.applied,"UI capture ran original intervention early");const auto request=packets.front();
    const auto deliver=[&]{need(!packets.empty(),"intervention missing actual Controller packet");const auto p=packets.front();packets.pop_front();return(p.to==1?host:client).receive(p.from,p.bytes.data(),p.bytes.size(),1,error);};
    const bool accepted=deliver();
    if(mode==3){need(host.status().locked&&!host.status().confirmed&&host.status().cause.find("retirement")!=std::string::npos,"changed current complete intervention retirement witness was accepted");return;}
    need(accepted,error);
    if(mode==4){need(deliver(),error);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect,"rejected original intervention became an attempted success");return;}
    const auto result=packets.front();const bool applied=deliver();
    if(mode==1||mode==2){need(!applied&&client.status().locked&&!client.status().confirmed,"partial/foreign complete intervention result was acknowledged");need(deliver(),error);need(host.status().locked&&!host.status().confirmed,"failed intervention ApplyAck became success");return;}
    need(applied&&deliver()&&deliver(),error);need(host.status().ack_mask==3&&host.status().confirmed==1&&client.status().confirmed==1&&he.state==ce.state,"intervention complete HostResult/native readback/ACK absent");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,error)&&client.receive(result.from,result.bytes.data(),result.bytes.size(),2,error),error);need(he.executed==1&&ce.applied==1,"duplicate intervention replayed an original native transaction");need(!client.submit_intervention(token,intent,3,error),"retired intervention token resubmitted");
}
void control_flow(){
    S hs(1),cs(2);E he(i::Family::Toon),ce(i::Family::Toon);he.state={};he.state.target=target(i::Family::Toon);he.state.control.emplace();ce.state=he.state;
    std::deque<Packet> packets;W hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    need(host.start(0,error)&&client.start(0,error),error);
    for(int n=0;n<3;++n){i::Intent intent;intent.target=he.state.target;intent.command=n==0?i::Command::ControlAcquire:n==1?i::Command::ControlInput:i::Command::ControlRelease;intent.control.emplace();intent.control->lease=token;
        if(n==1){intent.control->sequence=5;intent.control->jump=1;intent.control->input.axis1=1;intent.control->selected=target(i::Family::Npc);}
        std::string id=token;id.back()=char('2'+n);need(client.submit_intervention(id,intent,uint64_t(n*10),error),error);
        unsigned deliveries=0;std::optional<Packet> result;
        while(!packets.empty()){const auto packet=packets.front();packets.pop_front();Message decoded;need(decode(packet.bytes.data(),packet.bytes.size(),decoded,error),error);
            if(decoded.kind==Kind::HostResult){result=packet;auto forged=decoded;forged.evidence.intervention_after->control->peer=3;std::vector<uint8_t> bytes;need(!encode(forged,bytes,error),"foreign Control peer proof accepted");}
            need((packet.to==1?host:client).receive(packet.from,packet.bytes.data(),packet.bytes.size(),uint64_t(n*10+1),error),error);need(++deliveries<8,"Control Controller packet loop");}
        need(host.status().confirmed==uint64_t(n+1)&&client.status().confirmed==uint64_t(n+1)&&he.state==ce.state,"Control ordered result/ACK or native proof missing");
        need(result&&client.receive(result->from,result->bytes.data(),result->bytes.size(),uint64_t(n*10+2),error),error);need(he.executed==n+1&&ce.applied==n+1,"Control duplicate reran original native operation");
    }
    need(!he.state.control->controlled,"Control release did not retire owner");
}
void codec(){for(auto family:{i::Family::Boss,i::Family::Monster,i::Family::Npc,i::Family::Toon}){
    Message message;message.session=77;message.request.action=Action::Intervention;message.request.peer=2;message.request.sequence=1;message.request.action_uuid=token;message.request.snapshot_uuid=snapshot;message.request.canonical_record_key=target(family).key;message.request.intervention={target(family),i::Command::Kill,sha};
    std::vector<uint8_t> bytes;std::string error;Message decoded;need(encode(message,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,error);auto wire=nlohmann::json::parse(bytes);need(wire.at("schema")==35,"intervention requires current coherent Action35");
    const auto rejected=[&](nlohmann::json value){const auto text=value.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"malformed intervention owner packet admitted");};
    auto bad=wire;bad["schema"]=34;rejected(bad);bad=wire;bad["request"]["intervention"]["command"]=0;rejected(bad);bad=wire;bad["request"]["canonical_record_key"]="subscriber:18";rejected(bad);bad=wire;bad["request"]["intervention"]["target"]["local_uid"]=1;rejected(bad);
    message.kind=Kind::HostResult;message.outcome=Outcome::Confirmed;message.revision=message.request.host_revision=1;message.required_mask=3;message.evidence.intervention_before=proof(family);message.evidence.intervention_after=proof(family);message.evidence.retirement_confirmed=true;
    need(encode(message,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,error);wire=nlohmann::json::parse(bytes);
    for(const char* field:{"actor","subscriber","actors_sha","zones_sha","economy_sha","gm_sha","marketing_sha","tutorial_sha"}){bad=wire;bad["evidence"]["intervention_after"].erase(field);rejected(bad);}
    bad=wire;bad["evidence"]["intervention_after"]["target"]["key"]="subscriber:18";rejected(bad);
}}
}
int main()try{codec();control_flow();for(auto family:{i::Family::Boss,i::Family::Monster,i::Family::Npc,i::Family::Toon})for(auto command:{i::Command::Heal,i::Command::Kill,i::Command::Warn,i::Command::Ban,i::Command::Respawn,i::Command::RaiseLevel,i::Command::LowerLevel})if(i::valid(i::Intent{target(family),command,sha}))for(int mode=0;mode<6;++mode)flow(family,command,mode);std::cout<<checks<<" intervention138 original-once, full native proof, no-op, HostResult and ACK assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
