#include "typed_action/tick_commands.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace tick=mt2mp::action_session::tick_commands;
void need(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
tick::Config config(uint64_t peer){tick::Config c;c.context={77,peer,1,3,peer==1?Role::Host:Role::Client,"snapshot-A"};return c;}
tick::Input input(Action action,bool group){Request r;r.peer=2;r.sequence=1;r.action=action;r.action_uuid="action";r.target_action_uuid="source";
    r.snapshot_uuid="snapshot-A";r.canonical_record_key=(group?"groups:":"selection:")+std::string(action==Action::EditGroup||action==Action::MoveSelection?"action":"source");return {77,"snapshot-A",r};}
int main(){try{
    auto valid=[](const tick::Input&,std::string&){return true;};
    auto current=[](std::string&){return true;};auto advance=[](uint64_t,std::string&){return Outcome::Confirmed;};
    unsigned cases=0;std::string e;
    for(bool group:{false,true}) for(auto action:{group?Action::EditGroup:Action::MoveSelection,Action::UndoBuilding,Action::RedoBuilding}){
        auto request=input(action,group);need(tick::requires_evidence(request),"selection evidence not mandatory");
        {tick::Queue host(config(1),valid);tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;
            need(host.offer(2,request,assigned,e)==tick::Admission::Accepted&&host.seal(batch,e),e);
            need(!host.consume(1,current,[](const auto&,std::string&){return Outcome::Confirmed;},advance,receipt,e)&&
                 receipt.outcome==Outcome::Unknown&&e.find("Required canonical")!=std::string::npos,"missing evidence accepted");++cases;}
        for(bool equal:{true,false}){
            tick::Queue host(config(1),valid),client(config(2),valid);tick::Assigned assigned;tick::Batch batch;tick::Receipt h,c;
            need(host.offer(2,request,assigned,e)==tick::Admission::Accepted&&host.seal(batch,e),e);
            need(client.admit(1,batch,e)==tick::Admission::Accepted,e);
            auto consume=[&](tick::Queue& q,tick::Receipt& receipt,char digit){return q.consume(1,current,[&](const auto& command,std::string& error){
                return q.bind_evidence(command,std::string(64,digit),error)?Outcome::Confirmed:Outcome::Unknown;},advance,receipt,e);};
            need(consume(host,h,'a')&&consume(client,c,equal?'a':'b'),e);
            if(equal){need(host.state().evidence_ack_mask!=3,"retired before full cohort ACK");
                need(host.acknowledge(2,c,e)&&host.state().evidence_ack_mask==3,"full cohort ACK did not retire evidence");}
            else need(!host.acknowledge(2,c,e)&&host.state().locked&&e.find("differs")!=std::string::npos,"unequal evidence accepted");
            ++cases;
        }
    }
    std::cout<<cases<<" ordered edit evidence queue cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
