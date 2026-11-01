#include "typed_action/tick_commands.h"

#include "placement_ui_diagnostics/admission.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace retained_completion_cases {
using namespace mt2mp::action_session;
namespace tc=mt2mp::action_session::tick_commands;
namespace diag=mt2mp::game::placement_ui_diagnostics;
using json=nlohmann::json;
int checks=0;
void check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
void run(){
 try{
    const auto evidence=json::parse(R"ORD190({"source":"F:\\Projects\\Mods\\MMORPGTycoon2\\_MPTest\\ord190\\runs\\818bddd9-71a5-4a03-bf52-210a61c52e9e\\client-1\\profile\\diagnostics\\multiplayer\\status.json","sha256":"2c7ed16892e805773535c1fd7426a2fec78616aff88a86229ffdeecd4b32ab09","typed_actions":{"started":true,"locked":false,"action_uuid":"8c8321a0-0998-4e93-81d2-bd511545260c","pending":true,"outcome":0,"cause":"","host_revision":36,"ack_mask":0,"required_mask":3},"native_receipts":{"0b1d44eb-fa67-4974-a4c2-e8521b98d382":{"actual_position":[4567.66357421875,17.927597045898438,1151.137451171875],"actual_rotation":[0.0,0.0,0.0,1.0],"actual_scale":[1.0,1.0,1.0],"add":1,"add_attempted":1,"attempted":true,"cash_after":93500,"cash_before":94500,"confirmed":true,"cost":1000,"cursor_behaviour_preserved":0,"cursor_selection_preserved":0,"dungeon_maker":{},"error":"","exception":0,"local_undo_graph":2193071049536,"native_uid":3223,"preview_effect":{"checked":true,"destroy_attempted":false,"destroy_returned":false,"error":"","manager_count_after":-1,"manager_count_before":-1,"present":false,"removed":false,"slot_cleared":true},"private_stage":18,"purchase":1,"purchase_attempted":1,"requested_position":[4567.66357421875,17.927597045898438,1151.137451171875],"undo":1,"undo_attempted":1,"undo_confirmed":true}}})ORD190");
    const std::string id="0b1d44eb-fa67-4974-a4c2-e8521b98d382",world="3390f592-fca9-46e4-92bd-d04e63a1a2c9";
    tc::Config h,c;h.context={9,1,1,3,Role::Host,world};h.initial_order=34;h.first_tick=19564;c=h;c.context.local_peer=2;c.context.role=Role::Client;
    tc::Queue host(h,[](const auto&,auto&){return true;}),client(c,[](const auto&,auto&){return true;});
    std::string error;uint64_t seq=0;unsigned executions=0;
    auto batch=[&](const std::string& action,bool execute=true){
        if(!action.empty()){Request r;r.peer=2;r.sequence=++seq;r.snapshot_uuid=world;r.action_uuid=action;r.action=Action::PlaceBuilding;r.host_revision=host.state().assigned_order;
            tc::Assigned a;check(host.offer(2,{9,world,r},a,error)==tc::Admission::Accepted,"offer");}
        tc::Batch b;check(host.seal(b,error),"seal");check(client.admit(1,b,error)==tc::Admission::Accepted,"admit");
        tc::Receipt hr,cr;auto run=[&](const auto&,auto&){++executions;return execute?Outcome::Confirmed:Outcome::Unknown;};
        const auto prior=host.completed_actions().size();
        check(host.consume(b.tick,[](auto&){return true;},run,[](auto,auto&){return Outcome::Confirmed;},hr,error)==execute,"host consume");
        check(host.completed_actions().size()==prior,"host must wait for actual peer ACK");
        check(client.consume(b.tick,[](auto&){return true;},run,[](auto,auto&){return Outcome::Confirmed;},cr,error)==execute,"client consume");
        if(execute){check(host.acknowledge(2,cr,error),"peer ACK");auto n=host.completed_actions().size();check(host.acknowledge(2,cr,error),"duplicate ACK");check(host.completed_actions().size()==n,"duplicate ACK did not append");}
    };
    batch(id);check(host.completed_actions().size()==1&&client.completed_actions().size()==1,"exact command retained");
    const auto receipt=client.completed_actions().front();
    check(receipt.order==35&&receipt.tick==19564&&receipt.peer==2&&receipt.sequence==1&&receipt.session==9&&receipt.snapshot_uuid==world,"native order identity");
    check(receipt.ack_mask==2&&host.completed_actions().front().ack_mask==3,"local vs aggregate ACK");
    for(int i=0;i<150;++i)batch("");
    check(client.completed_actions().front().action_uuid==id,"empty ticks cannot evict command proof");
    auto row=[&](const tc::CompletedAction& r){return json{{"action_uuid",r.action_uuid},{"snapshot_uuid",r.snapshot_uuid},{"session",r.session},{"order",r.order},{"tick",r.tick},{"peer",r.peer},{"sequence",r.sequence},{"required_mask",r.required_mask},{"ack_mask",r.ack_mask},{"outcome",1},{"confirmed",true}};};
    auto state=evidence.at("typed_actions");auto native=evidence.at("native_receipts");
    auto job=[&]{diag::Job j;j.input.snapshot_uuid=world;j.receipt.capture_count=1;j.receipt.action_uuid=id;j.receipt.phase=placement_ui_probe::Phase::CapturedAwaitingHost;return j;};
    {auto j=job();check(!diag::observe_confirmation(j,state,native,error,false),"retained Ord190 baseline reproduces");check(error=="build_ui_controller_action_missing_or_overwritten_no_replay","original cause");}
    state["ordered_completed_actions"]={{id,row(receipt)}};
    {auto j=job();check(diag::observe_confirmation(j,state,native,error,false),"retained native Ord190 values now complete");check(j.receipt.host_confirmed_count==1,"only one confirmation");}
    for(const char* field:{"action_uuid","snapshot_uuid","session","order","tick","peer","sequence","confirmed","outcome"}){
        auto wrong=state;wrong["ordered_completed_actions"][id].erase(field);auto j=job();check(!diag::observe_confirmation(j,wrong,native,error,false),"missing identity must reject");}
    {auto wrong=state;wrong["locked"]=true;auto j=job();check(!diag::observe_confirmation(j,wrong,native,error,false),"locked controller rejects old success");}
    {auto j=job();check(!diag::observe_confirmation(j,state,native,error,true),"Host cannot use partial ACK");}
    {auto good=state;good["ordered_completed_actions"][id]=row(host.completed_actions().front());auto j=job();check(diag::observe_confirmation(j,good,native,error,true),"Host aggregate completion");}
    for(const char* field:{"confirmed","undo_confirmed","native_uid","purchase","add","undo"}){
        auto wrong=native;wrong[id].erase(field);auto j=job();check(!diag::observe_confirmation(j,state,wrong,error,false),"local native effect remains mandatory");}
    {auto wrong=native;wrong[id]["exception"]=1;auto j=job();check(!diag::observe_confirmation(j,state,wrong,error,false),"native exception remains failure");}
    for(int i=0;i<64;++i)batch("action-"+std::to_string(i));
    check(host.completed_actions().size()==64&&host.completed_actions().front().action_uuid!=id,"bounded 64 commands");
    auto n=host.completed_actions().size();batch("failed",false);check(host.completed_actions().size()==n&&client.completed_actions().size()==n,"failed tick adds no success");
    check(executions==132,"no native replay");
    std::cout<<checks<<" actual Queue/admission checks passed\n";
 }catch(const std::exception& e){std::cerr<<"check "<<checks<<": "<<e.what()<<'\n';throw;}

}

}
