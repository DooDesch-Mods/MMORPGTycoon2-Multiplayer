#include "character_rename_fixture.h"
#include "typed_action/tick_evidence.h"
#include <iostream>
namespace tick=mt2mp::action_session::tick_commands;
Context context(uint64_t peer){return {77,peer,1,3,peer==1?Role::Host:Role::Client,snapshot_id};}
int main()try{
    unsigned checks=0;std::string error;
    auto check=[&](bool ok,const std::string& why){++checks;need(ok,why);};
    for(int32_t kind=0;kind<5;++kind){character_kind=kind;
        auto value=request().request;tick::Input input{77,snapshot_id,value};
        check(tick::requires_evidence(input),"Rename canonical evidence was optional");
        auto stat=input;std::get<Request>(stat.command).action=Action::SetCharacterHealth;
        check(!tick::requires_evidence(stat),"Rename evidence gate expanded to unimplemented stats");
        for(int variant=0;variant<3;++variant){
            tick::Config hc;hc.context=context(1);tick::Config cc;cc.context=context(2);
            auto valid=[](const tick::Input&,std::string&){return true;};
            tick::Queue host(hc,valid),client(cc,valid);tick::Assigned assigned;tick::Batch batch;tick::Receipt h,c;
            check(host.offer(2,input,assigned,error)==tick::Admission::Accepted&&host.seal(batch,error),error);
            check(client.admit(1,batch,error)==tick::Admission::Accepted,error);
            auto consume=[&](tick::Queue& queue,tick::Receipt& receipt,uint64_t peer){
                return queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& cause){
                    tick::ExecutionReceipt execution;execution.command=command;execution.outcome=Outcome::Confirmed;
                    auto evidence=result().evidence;
                    if(peer==2&&variant==1)evidence.character_rename_after.all_sha256=std::string(64,'d');
                    if(peer==2&&variant==2)++evidence.character_rename_after.global_random[0];
                    execution.evidence=std::make_shared<const Evidence>(evidence);std::string digest;
                    if(!tick::canonical_evidence(context(peer),command,execution,digest,cause)||!queue.bind_evidence(command,digest,cause))return Outcome::Unknown;
                    return Outcome::Confirmed;
                },[](uint64_t,std::string&){return Outcome::Confirmed;},receipt,error);
            };
            check(consume(host,h,1)&&consume(client,c,2),error);
            if(variant==0)check(host.acknowledge(2,c,error)&&host.state().evidence_ack_mask==3,"Complete Name effect lacked cohort ACK");
            else check(!host.acknowledge(2,c,error)&&host.state().locked&&error.find("differs")!=std::string::npos,"Changed complete effect or RNG ACK accepted");
        }
        tick::Assigned assigned{1,input};tick::ExecutionReceipt missing;missing.command=assigned;missing.outcome=Outcome::Confirmed;std::string digest;
        check(!tick::canonical_evidence(context(1),assigned,missing,digest,error),"Missing complete Name effect accepted");
    }
    std::cout<<checks<<" actual ordered Character Rename canonical codec/queue/ACK checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
