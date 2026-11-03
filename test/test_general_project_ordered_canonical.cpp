#include "typed_action/tick_evidence.h"
#include "typed_action/tick_authority.h"
#include "typed_action/tick_wire.h"
#include <iostream>
#include <thread>
namespace a=mt2mp::action_session;
namespace tick=a::tick_commands;
namespace gp=mt2mp::game::general_project;
using json=nlohmann::json;
unsigned fixture_checks=0;
void fixture_need(bool value,const std::string& cause){++fixture_checks;if(!value)throw std::runtime_error(cause);}
#include "general_project_native_record.inc"
const std::string fixture_snapshot="1ad1de1d-a41c-4797-bcb8-398aa3ee3442";
const a::Binding fixture_binding{7,9};
a::Context fixture_context(uint64_t peer){return {77,peer,1,3,peer==1?a::Role::Host:a::Role::Client,fixture_snapshot};}
a::Request fixture_request(){a::Request out;out.action=a::Action::GeneralProject;out.peer=2;out.sequence=1;out.action_uuid=fixture_record.at("action_uuid");out.snapshot_uuid=fixture_snapshot;out.canonical_record_key="development:project";out.general_project=gp::codec::intent(fixture_record.at("operation").at("intent"));return out;}
a::Evidence fixture_evidence(){a::Evidence out;const auto& row=fixture_record.at("operation");out.general_project_before=gp::codec::proof(row.at("before"));out.general_project_after=gp::codec::proof(row.at("after"));out.before.cash=out.before.record_cash=1000;out.before.flat_day=out.before.record_day=1;out.before.record_present=1;out.after=out.before;out.after.cash=out.after.record_cash=900;out.after.record_expense=100;return out;}
struct FixtureSession:a::Session {a::Context value;explicit FixtureSession(uint64_t peer):value(fixture_context(peer)){}bool current(a::Context& out,std::string&)override{out=value;return true;}};
struct FixtureEngine:a::Engine {
    a::Context context;a::Evidence proof;bool executed=false,retired=false;unsigned calls=0,adoptions=0;
    FixtureEngine(uint64_t peer,a::Evidence input):context(fixture_context(peer)),proof(std::move(input)){}
    bool bind(const std::string&,a::Binding& out,std::string&)override{out=fixture_binding;return true;}
    bool current(const a::Binding& binding,const std::string& snapshot,std::string& error)override{if(binding!=fixture_binding||snapshot!=fixture_snapshot){error="fixture current native owner differs";return false;}return true;}
    bool ledger(const a::Binding&,a::Ledger& out,std::string&)override{out=executed?proof.after:proof.before;return true;}
    a::Outcome execute_remove(const a::Binding&,const a::Request&,std::string& error)override{error="fixture unexpected Remove";return a::Outcome::RejectedNoEffect;}
    a::Outcome apply_remove(const a::Binding&,const a::Request&,const a::Evidence&,std::string& error)override{++adoptions;error="fixture unexpected adoption";return a::Outcome::RejectedNoEffect;}
    bool observe_remove(const a::Binding&,const a::Request&,a::Evidence&,std::string& error)override{error="fixture unexpected Remove observation";return false;}
    a::Outcome execute_action(const a::Binding& binding,const a::Request& request,std::string& error)override{
        if(!tick::authority::matches(context,binding,request,error))return a::Outcome::RejectedNoEffect;++calls;executed=true;return a::Outcome::Confirmed;
    }
    bool observe_action(const a::Binding&,const a::Request&,a::Evidence& out,std::string&)override{out=proof;out.retirement_confirmed=retired;return true;}
    bool retire(const a::Binding&,const a::Request&,std::string&)override{retired=true;return true;}
};
struct FixtureController {tick::Queue& queue;tick::State ordered_state()const{return queue.state();}};
struct FixtureRuntime {
    using Outcome=a::Outcome;using Request=a::Request;using Action=a::Action;
    a::Context context_;FixtureController controller_;std::thread::id thread_=std::this_thread::get_id();
    tick::ExecutionReceipt last_execution_;std::string last_evidence_sha256_;uint64_t snapshot_command_order_=0,last_execution_tick_=0,last_execution_committed_tick_=0;
    struct Borrowed{uint64_t tick=1;} fixture_frame;Borrowed* borrowed_=&fixture_frame;
    std::unique_ptr<tick::Executor> executor_;
    FixtureRuntime(uint64_t peer,tick::Queue& queue,FixtureSession& session,FixtureEngine& engine):context_(fixture_context(peer)),controller_{queue}{
        executor_=std::make_unique<tick::Executor>(session,engine,context_,fixture_binding,tick::ClockPort{},[](const a::Context& context,const a::Binding& binding,const tick::Assigned& command,std::string& error){return tick::authority::matches(context,binding,std::get<a::Request>(command.input.command),error);});
    }
    void snapshot_time_executed(const tick::Assigned& command,a::Outcome){fixture_need(std::holds_alternative<a::Request>(command.input.command),"fixture unexpected clock path");}
    a::Outcome produce(tick::Queue& queue,const tick::Assigned& command,std::string& detail){
#include "project_canonical_producer_actual.inc"
    }
    void commit(const tick::Receipt& completed){const auto* frame=borrowed_;
#include "project_canonical_commit_actual.inc"
    }
#include "project_canonical_status_actual.inc"
};
tick::Config fixture_config(uint64_t peer){tick::Config out;out.context=fixture_context(peer);return out;}
struct FixtureRun {tick::Receipt receipt;json status;unsigned calls=0,adoptions=0;};
FixtureRun fixture_consume(uint64_t peer,tick::Queue& queue,a::Evidence proof){
    FixtureSession session(peer);FixtureEngine engine(peer,std::move(proof));FixtureRuntime runtime(peer,queue,session,engine);FixtureRun out;std::string error;
    const bool confirmed=queue.consume(1,[](std::string&){return true;},[&](const tick::Assigned& command,std::string& cause){return tick::authority::run(queue,session,engine,fixture_binding,command,[&](std::string& detail){return runtime.produce(queue,command,detail);},cause);},[](uint64_t,std::string&){return a::Outcome::Confirmed;},out.receipt,error);
    fixture_need(confirmed,error);fixture_need(!tick::authority::active(),"consumed authority escaped");
    auto pending=runtime.execution_status();fixture_need(pending.at("available")==true&&pending.at("command_action")==133&&pending.at("committed_tick")==0,"actual GeneralProject producer/status absent before commit");runtime.commit(out.receipt);out.status=runtime.execution_status();
    const uint64_t retired=peer==1?0:1;
    fixture_need(out.status.at("execution_tick")==1&&out.status.at("committed_tick")==1&&out.status.at("evidence_tick")==retired&&out.status.at("evidence_order")==retired,"actual committed canonical identity differs: "+out.status.dump());
    bool outside=false;std::thread reader([&]{outside=runtime.execution_status().at("available")==false;});reader.join();fixture_need(outside,"canonical reader accepted foreign thread");
    out.calls=engine.calls;out.adoptions=engine.adoptions;return out;
}
void fixture_pair(int defect){std::string error;auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(fixture_config(1),validate),client(fixture_config(2),validate);tick::Assigned assigned;tick::Batch batch;const tick::Input input{77,fixture_snapshot,fixture_request()};
    fixture_need(host.offer(2,input,assigned,error)==tick::Admission::Accepted&&host.seal(batch,error)&&client.admit(1,batch,error)==tick::Admission::Accepted,error);
    auto left=fixture_evidence(),right=left;if(defect==1)++right.general_project_after->random[0];if(defect==2){++right.general_project_before->random[3];++right.general_project_after->random[3];}if(defect==3){++right.before.cash;++right.after.cash;}
    const auto h=fixture_consume(1,host,left),c=fixture_consume(2,client,right);fixture_need(h.calls==1&&c.calls==1&&!h.adoptions&&!c.adoptions,"original count/adoption differs");
    fixture_need(h.status.at("evidence_ack_mask")==0&&c.status.at("evidence_ack_mask")==2,"canonical receipt claims premature full cohort");
    auto remote=c.receipt;if(defect==4)++remote.command_evidence[0].order;if(defect==5)remote.command_evidence.clear();if(defect==6)++remote.tick;
    std::vector<uint8_t> bytes;tick::wire::Packet decoded;
    if(defect==4)fixture_need(!tick::wire::encode({3,remote},bytes,error),"wire accepted evidence order beyond retained batch");
    else{fixture_need(tick::wire::encode({3,remote},bytes,error)&&tick::wire::decode(bytes.data(),bytes.size(),decoded,error),error);fixture_need(bytes.at(4)==(defect==5?1:2),"receipt wire revision differs");remote=std::get<tick::Receipt>(decoded.value);}
    if(defect){const bool accepted=host.acknowledge(2,remote,error);fixture_need(!accepted&&!error.empty(),"mismatched ACK accepted defect="+std::to_string(defect)+" cause="+error);if(defect!=6)fixture_need(host.state().locked,"required canonical mismatch did not lock defect="+std::to_string(defect)+" cause="+error);else fixture_need(!host.state().locked&&error.find("identity differs")!=std::string::npos,"foreign tick did not preserve identity rejection: "+error);}
    else{fixture_need(h.status.at("evidence_sha256")==c.status.at("evidence_sha256")&&host.acknowledge(2,remote,error)&&host.state().evidence_ack_mask==3,error);}
}
void fixture_early_outcome_only(){std::string error;auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(fixture_config(1),validate),client(fixture_config(2),validate);tick::Assigned assigned;tick::Batch batch;
    fixture_need(host.offer(2,{77,fixture_snapshot,fixture_request()},assigned,error)==tick::Admission::Accepted&&host.seal(batch,error)&&client.admit(1,batch,error)==tick::Admission::Accepted,error);
    auto remote=fixture_consume(2,client,fixture_evidence()).receipt;remote.command_evidence.clear();std::vector<uint8_t> bytes;tick::wire::Packet packet;
    fixture_need(tick::wire::encode({3,remote},bytes,error)&&bytes.at(4)==1&&tick::wire::decode(bytes.data(),bytes.size(),packet,error),error);
    fixture_need(!host.acknowledge(2,std::get<tick::Receipt>(packet.value),error)&&host.state().locked&&error.find("canonical command evidence missing")!=std::string::npos,"early old outcome-only ACK accepted");
}
void fixture_rejections(){const auto context=fixture_context(1);tick::Assigned assigned;assigned.order=1;assigned.input={77,fixture_snapshot,fixture_request()};tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=a::Outcome::Confirmed;receipt.native_attempted=true;auto proof=fixture_evidence();proof.retirement_confirmed=true;receipt.evidence=std::make_shared<a::Evidence>(proof);std::string error,digest;
    fixture_need(tick::canonical_evidence(context,assigned,receipt,digest,error)&&tick::evidence_digest(digest),error);
    for(int defect=0;defect<9;++defect){auto command=assigned;auto local=receipt;auto changed=proof;auto session=context;
        if(defect==0)changed.general_project_after.reset();if(defect==1)changed.general_project_after->selected.source.tech="dungeons";if(defect==2)changed.general_project_after->catalogue_sha256=std::string(64,'a');if(defect==3)changed.general_project_after->character_catalogue_sha256=std::string(64,'a');if(defect==4)changed.retirement_confirmed=false;if(defect==5)++local.command.order;if(defect==6)++session.session;if(defect==7)command.input.snapshot_uuid="foreign";if(defect==8)local.native_attempted=false;
        local.evidence=std::make_shared<a::Evidence>(changed);fixture_need(!tick::canonical_evidence(session,command,local,digest,error),"incomplete or mismatched GeneralProject canonical evidence accepted");
    }
    auto command=assigned;auto& request=std::get<a::Request>(command.input.command);request.general_project.command=gp::Command::Workers;request.general_project.workers=1;auto workers=proof;workers.general_project_before=proof.general_project_after;workers.general_project_after=workers.general_project_before;workers.general_project_after->progress.workers=1;workers.after=workers.before;receipt.command=command;receipt.evidence=std::make_shared<a::Evidence>(workers);fixture_need(tick::canonical_evidence(context,command,receipt,digest,error),error);++workers.after.cash;receipt.evidence=std::make_shared<a::Evidence>(workers);fixture_need(!tick::canonical_evidence(context,command,receipt,digest,error),"non-Start GeneralProject Ledger drift accepted");
}
int main()try{for(int defect=0;defect<=6;++defect)fixture_pair(defect);fixture_early_outcome_only();fixture_rejections();std::cout<<fixture_checks<<" actual GeneralProject canonical producer/status/consumed Queue assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
