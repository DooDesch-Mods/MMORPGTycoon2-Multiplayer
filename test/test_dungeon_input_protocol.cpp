#include "test_dungeon_input_fixture.h"
#include <iostream>
#include <memory>
#include <cstring>
#include "dungeon_gizmo_name_bound.h"
#include "gizmo_name_bound_cases.inc"
#include "gizmo_control_protocol_cases.inc"
#include "scene_scenery_control_protocol_cases.inc"
int main()try{using namespace input_fixture;size_t checks=0;std::string error;std::vector<uint8_t> bytes;
    const auto check=[&](bool ok,const std::string& cause){++checks;need(ok,cause);};
    const auto rejects=[&](nlohmann::json value,const std::string& cause){const auto text=value.dump();auto decoded=std::make_unique<Message>();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),cause);};
    for(unsigned tool=0;tool<5;++tool)for(bool scene_instance:{false,true})for(int32_t native_price:{0,250})for(int32_t native_rank:{0,1,2}){
        selected_tool=DungeonInputTool(tool);instance=scene_instance;price=native_price;rank=native_rank;
        for(bool host_result:{false,true}){auto original_storage=std::make_unique<Message>(host_result?result():request());const auto& original=*original_storage;
            auto decoded=std::make_unique<Message>();check(encode(original,bytes,error),error);
            check(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==original,error);
            const auto document=nlohmann::json::parse(bytes.begin(),bytes.end());check(document.at("schema")==36,"new input did not use exact schema36");
            auto bad=document;bad["schema"]=35;rejects(bad,"Action66 accepted old schema35");
            bad=document;bad["request"]["foreign"]=0;rejects(bad,"unknown request field accepted");
            bad=document;bad["evidence"]["foreign"]=0;rejects(bad,"unknown evidence field accepted");
            bad=document;bad["request"]["dungeon_input"]["tool"]=6;rejects(bad,"unproved input tool accepted");
            bad=document;bad["request"]["dungeon_input"]["cost"]=1.5;rejects(bad,"fractional native price accepted");
            bad=document;bad["request"]["action"]=57;rejects(bad,"Dungeon payload admitted as District action");
        }
        auto valid_storage=std::make_unique<Message>(result());auto& valid=*valid_storage;check(dungeon_input_wire_fits(valid.request,valid.evidence.dungeon_input_before,money(),error),error);
        for(int change=0;change<8;++change){auto bad_storage=std::make_unique<Message>(valid);auto& bad=*bad_storage;
            if(change==0)++bad.evidence.after.line_counts[7];if(change==1)bad.evidence.dungeon_input_after.quests_sha256=std::string(64,'9');
            if(change==2)bad.evidence.dungeon_input_after.layout_sha256=std::string(64,'9');if(change==3)bad.evidence.undo_confirmed=false;
            if(change==4)bad.evidence.selection_confirmed=true;if(change==5)bad.evidence.target_retired=true;
            if(change==6)bad.evidence.dungeon_input_after.protected_state.dungeon.price++;
            if(change==7)bad.request.dungeon_palette.key=intent().key;
            check(!encode(bad,bytes,error),"foreign effect or family field accepted change="+std::to_string(change));
        }
        auto history_storage=std::make_unique<Message>(valid);auto& history=*history_storage;history.request.action=Action::UndoBuilding;history.request.action_uuid="44444444-4444-4444-8444-444444444444";
        history.request.target_action_uuid=action_id;history.request.canonical_record_key=std::string("dungeon-input:")+action_id;
        history.request.dungeon_input={};history.evidence.dungeon_input_origin=intent();
        history.evidence.dungeon_input_before=after(before());history.evidence.dungeon_input_after=before();
        history.evidence.before=paid();history.evidence.after=paid();
        const bool cash=selected_tool==DungeonInputTool::Treasure||selected_tool==DungeonInputTool::Door||(selected_tool==DungeonInputTool::Key&&price!=0);
        if(cash){auto& ledger=history.evidence.after;ledger.cash+=price;ledger.record_cash+=price;ledger.record_income+=price;
            ++ledger.line_counts[6];ledger.line_income[6]+=price;}
        auto decoded=std::make_unique<Message>();check(encode(history,bytes,error),error);check(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==history,error);
        history.request.action=Action::RedoBuilding;std::swap(history.evidence.dungeon_input_before,history.evidence.dungeon_input_after);
        history.evidence.before=history.evidence.after;history.evidence.after=history.evidence.before;
        if(cash){auto& ledger=history.evidence.after;ledger.cash-=price;ledger.record_cash-=price;ledger.record_income-=price;
            ++ledger.line_counts[6];ledger.line_income[6]-=price;}
        check(encode(history,bytes,error),error);check(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==history,error);
        history.evidence.dungeon_input_origin.reset();check(!encode(history,bytes,error),"history lost original fixed tool authority");
    }
    selected_tool=DungeonInputTool::Door;instance=false;price=0;rank=0;
    {
        auto large_storage=std::make_unique<Message>(result());auto& large=*large_storage;
        large.evidence.dungeon_input_after.birth->gizmo.name=std::string(8192,'\1');
        check(dungeon_input_wire_fits(large.request,large.evidence.dungeon_input_before,money(),error),error);
        check(encode(large,bytes,error),error);
        check(bytes.size()<max_payload,"maximum escaped native name does not fit ordinary Dungeon receipt");
        large.request.action=Action::UndoBuilding;large.request.action_uuid="44444444-4444-4444-8444-444444444444";
        large.request.target_action_uuid=action_id;large.request.canonical_record_key=std::string("dungeon-input:")+action_id;
        large.request.dungeon_input={};large.evidence.dungeon_input_origin=intent();
        const auto restored=large.evidence.dungeon_input_after.birth;
        check(dungeon_input_wire_fits(large.request,large.evidence.dungeon_input_after,money(),error,&*large.evidence.dungeon_input_origin),error);
        large.request.action=Action::RedoBuilding;
        check(!dungeon_input_wire_fits(large.request,large.evidence.dungeon_input_before,money(),error,&*large.evidence.dungeon_input_origin),
              "Redo preflight lost retained original birth");
        check(dungeon_input_wire_fits(large.request,large.evidence.dungeon_input_before,money(),error,&*large.evidence.dungeon_input_origin,&*restored),error);
    }
    selected_tool=DungeonInputTool::Paint;
    {
        auto oversized=std::make_unique<Message>(request());
        auto& path=oversized->request.dungeon_input.shapes[0][0];
        path.resize(3000,{INT64_MAX>>3,INT64_MAX>>3});
        check(!dungeon_input_wire_fits(oversized->request,before(),money(),error),"oversized paint accepted before mutation");
        check(error.find("requires ")!=std::string::npos&&error.find("60000")!=std::string::npos,
              "oversized preflight did not retain required and allowed byte counts");
        check(!encode(*oversized,bytes,error),"oversized paint bypassed actual encoder packet limit");
    }
    gizmo_name_bound_fixture::run(check);
    gizmo_control_protocol_fixture::run(check);
    scene_scenery_control_protocol_fixture::run(check);
    std::cout<<checks<<" Dungeon input exact schema36/full-result/history cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
