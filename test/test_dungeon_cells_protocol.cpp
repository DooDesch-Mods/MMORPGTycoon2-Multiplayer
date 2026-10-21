#include "test_dungeon_palette_fixture.h"
#include <iostream>
#include <memory>
std::unique_ptr<Message> cell_result(DungeonCellMode mode,bool defaults){
    auto storage=std::unique_ptr<Message>(new Message(result()));auto& m=*storage;auto& intent=m.request.dungeon_palette;intent.edit=DungeonPaletteEdit::Cells;intent.defaults=false;intent.colors={};intent.packed={};
    intent.stroke.mode=mode;intent.stroke.cells={0,1};if(mode==DungeonCellMode::Paint){intent.stroke.theme="granite";intent.stroke.theme_packed={21,22,23,24};}
    auto& before=m.evidence.dungeon_palette_before;before.defaults=defaults;before.stroke={std::string(64,'a'),std::string(64,'b'),std::string(64,'c'),{{0,before.cells[0]},{1,before.cells[1]}}};before.cells.clear();
    auto& after=m.evidence.dungeon_palette_after;after=before;after.stroke.dungeon_sha=std::string(64,'d');after.stroke.scenes_sha=std::string(64,'e');after.stroke.quests_sha=std::string(64,'f');
    ++after.protected_state.dungeon.undo_count;after.protected_state.dungeon.global_random[0]++;
    for(auto& cell:after.stroke.cells){if(mode==DungeonCellMode::Close){cell.value.open=false;continue;}
        if(!cell.value.open&&defaults)for(size_t i=0;i<4;++i)after.palette[i]=intent.stroke.theme_packed[i];
        cell.value.open=true;cell.value.theme=intent.stroke.theme;std::copy_n(after.palette.begin(),4,cell.value.colors.begin());}
    m.evidence.undo_confirmed=true;return storage;
}
int main()try{
    unsigned checks=0;std::string error;std::vector<uint8_t> bytes;auto decoded_storage=std::make_unique<Message>();auto& decoded=*decoded_storage;
    const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    for(const auto mode:{DungeonCellMode::Paint,DungeonCellMode::Close})for(bool defaults:{false,true}){
        auto value_storage=cell_result(mode,defaults);auto& value=*value_storage;check(encode(value,bytes,error),error);check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==value,"Complete cell graph witnesses did not roundtrip");
        check(dungeon_palette_wire_fits(value.request,value.evidence.dungeon_palette_before,money(),error),error);
        const auto invalid=[&](auto mutate){auto changed_storage=std::make_unique<Message>(value);auto& changed=*changed_storage;mutate(changed);check(!encode(changed,bytes,error),"Foreign or incomplete Dungeon cell operation was accepted");};
        invalid([](auto& m){m.request.dungeon_palette.stroke.cells.push_back(0);});invalid([](auto& m){m.request.dungeon_palette.stroke.cells[0]=-1;});
        invalid([](auto& m){m.request.dungeon_palette.stroke.mode=DungeonCellMode(2);});invalid([](auto& m){m.evidence.undo_confirmed=false;});
        invalid([](auto& m){m.evidence.dungeon_palette_after.stroke.dungeon_sha="bad";});invalid([](auto& m){m.evidence.dungeon_palette_after.stroke.scenes_sha="bad";});
        invalid([](auto& m){m.evidence.dungeon_palette_after.stroke.quests_sha="bad";});invalid([](auto& m){m.evidence.dungeon_palette_after.stroke.cells[0].index=2;});
        invalid([](auto& m){m.evidence.dungeon_palette_after.stroke.cells.pop_back();});invalid([](auto& m){m.evidence.dungeon_palette_after.palette.back()^=1;});
        invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.atmosphere++;});invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.dungeon.undo_count++;});
        invalid([](auto& m){m.evidence.dungeon_palette_after.cells.push_back({});});invalid([](auto& m){m.evidence.after.cash++;});
        check(encode(value,bytes,error),error);auto wire=nlohmann::json::parse(bytes);wire["request"]["dungeon_palette"]["stroke"]["cells"][0]=1.5;
        auto malformed=wire.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Fractional native cell index accepted");
    }
    auto skipped_storage=cell_result(DungeonCellMode::Paint,false);auto& skipped=*skipped_storage;skipped.evidence.dungeon_palette_after.stroke.cells[1]=skipped.evidence.dungeon_palette_before.stroke.cells[1];
    check(encode(skipped,bytes,error),"Original unavailable/unchanged cell could not be skipped");
    auto no_change_storage=cell_result(DungeonCellMode::Close,false);auto& no_change=*no_change_storage;no_change.evidence.dungeon_palette_after.stroke.cells=no_change.evidence.dungeon_palette_before.stroke.cells;
    check(!encode(no_change,bytes,error),"No-op cell stroke claimed a committed effect");
    auto large_storage=cell_result(DungeonCellMode::Close,false);auto& large=*large_storage;for(int32_t i=2;i<6000;++i){large.request.dungeon_palette.stroke.cells.push_back(i);
        large.evidence.dungeon_palette_before.stroke.cells.push_back({i,{std::string(100,'s'),{1,2,3,4},true}});}
    check(!dungeon_palette_wire_fits(large.request,large.evidence.dungeon_palette_before,money(),error),"Oversized original cell receipts admitted before mutation");
    for(const auto direction:{Action::UndoBuilding,Action::RedoBuilding}){
        auto history_storage=cell_result(DungeonCellMode::Close,false);auto& history=*history_storage;history.request.action=direction;
        history.request.target_action_uuid="44444444-4444-4444-8444-444444444444";history.request.canonical_record_key="dungeon-tiles:"+history.request.target_action_uuid;history.request.dungeon_palette={};
        history.evidence.dungeon_palette_after=history.evidence.dungeon_palette_before;
        history.evidence.dungeon_palette_after.stroke.dungeon_sha=std::string(64,'e');
        auto& count=history.evidence.dungeon_palette_after.protected_state.dungeon.undo_count;direction==Action::UndoBuilding?--count:++count;
        check(encode(history,bytes,error),error);check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==history,"Original Dungeon history lost its exact schema family");
        check(dungeon_palette_wire_fits(history.request,history.evidence.dungeon_palette_before,money(),error),error);
        for(int mode=0;mode<6;++mode){auto wrong_storage=std::make_unique<Message>(history);auto& wrong=*wrong_storage;if(mode==0)wrong.request.canonical_record_key="quests:"+wrong.request.target_action_uuid;
            if(mode==1)wrong.request.target_action_uuid=action_id;if(mode==2)wrong.evidence.undo_confirmed=false;
            if(mode==3)wrong.evidence.dungeon_palette_after.protected_state.dungeon.undo_count+=2;
            if(mode==4)wrong.evidence.dungeon_palette_after.protected_state.atmosphere++;
            if(mode==5)wrong.evidence.dungeon_palette_after.stroke.cells[0].index++;
            check(!encode(wrong,bytes,error),"Foreign, unconfirmed or incomplete original Dungeon history accepted");}
        check(encode(history,bytes,error),error);auto wrong_schema=nlohmann::json::parse(bytes);wrong_schema["schema"]=10u;
        const auto malformed=wrong_schema.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Dungeon history accepted old Building schema");
    }
    for(const auto edit:{DungeonPaletteEdit::Colors,DungeonPaletteEdit::Options}){palette_edit=edit;auto old_storage=std::unique_ptr<Message>(new Message(result()));auto& old=*old_storage;
        check(encode(old,bytes,error),error);auto wire=nlohmann::json::parse(bytes);
        check(wire["request"]["dungeon_palette"].size()==6&&!wire["request"]["dungeon_palette"].contains("stroke")&&wire["evidence"]["dungeon_palette_before"].size()==4,"Existing Colors/Options wire shape changed");}
    std::cout<<checks<<" Dungeon cell complete schema, native transitions and old palette compatibility comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
