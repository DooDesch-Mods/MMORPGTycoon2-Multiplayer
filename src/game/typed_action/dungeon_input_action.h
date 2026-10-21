#pragma once
#include "dungeon_palette_action.h"
#include "monster_action.h"
#include "dungeon_gizmo_action.h"
#include "../dungeon_fill_record.h"
#include "../dungeon_gizmo_control.h"
#include "../dungeon_gizmo_reader.h"
#include "../dungeon_scene_key.h"
#include "../network_object_identity.h"
#include "../monster_target_key.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
namespace mt2mp::action_session {
enum class DungeonInputTool:uint8_t {Door,Key,Monster,Paint,Treasure,Gizmos};
struct DungeonInputIntent {
    std::string key,scene,before_sha256,type,variant;
    DungeonInputTool tool=DungeonInputTool::Door;
    std::array<float,3> position{},scale{1,1,1};std::array<float,4> rotation{0,0,0,1};
    int32_t character_type=-1,rank=-1,level=-1,cost=0;bool erase=false;
    std::vector<TerrainShape> shapes;nlohmann::json fill=nullptr;
    std::optional<game::dungeon_gizmo_control::Intent> controls;
    bool operator==(const DungeonInputIntent&)const=default;
};
struct DungeonInputBirth {
    std::string key,scene;DungeonInputTool tool=DungeonInputTool::Door;
    DungeonGizmoRecord gizmo;MonsterActor monster;
    bool operator==(const DungeonInputBirth&)const=default;
};
struct DungeonInputState {
    DungeonSoundState protected_state;
    std::string dungeon_sha256,scenes_sha256,quests_sha256,layout_sha256;
    std::optional<DungeonInputBirth> birth;
    std::optional<std::vector<DungeonGizmoRow>> edits;
    std::optional<std::vector<game::dungeon_gizmo_control::SceneryRow>> scenery_edits;
    std::optional<std::array<int32_t,8>> scenery_release;
    bool operator==(const DungeonInputState&)const=default;
};
inline bool valid_dungeon_input(const DungeonInputIntent& value){
    if(!game::dungeon_scene_key::building_key(value.key)||!game::dungeon_gizmo_identity::scene(value.scene)||
       !value.scene.starts_with(value.key+"/")||!game::network_object_identity::sha(value.before_sha256)||uint8_t(value.tool)>5||value.cost<0||
       !monster_pose(value.position,value.rotation,value.scale))return false;
    if(value.tool==DungeonInputTool::Gizmos)return value.controls&&game::dungeon_gizmo_control::valid(*value.controls)&&
        value.type.empty()&&value.variant.empty()&&value.character_type==-1&&value.rank==-1&&value.level==-1&&value.cost==0&&
        !value.erase&&value.shapes.empty()&&value.fill.is_null()&&value.position==std::array<float,3>{}&&
        value.rotation==std::array<float,4>{0,0,0,1}&&value.scale==std::array<float,3>{1,1,1};
    if(value.controls)return false;
    if(value.tool==DungeonInputTool::Paint){
        if(!value.type.empty()||!value.variant.empty()||value.character_type!=-1||value.rank!=-1||value.level!=-1||value.cost||
           value.position!=std::array<float,3>{}||value.rotation!=std::array<float,4>{0,0,0,1}||value.scale!=std::array<float,3>{1,1,1}||value.shapes.empty())return false;
        if(value.erase?!value.fill.is_null():value.fill.is_null())return false;
        if(!value.erase){std::vector<subscriber_lifecycle::Record> records;std::string error;if(!game::dungeon_fill::records(value.fill,records,error))return false;}
        size_t total=0;for(const auto& shape:value.shapes){if(shape.empty())return false;
            for(const auto& path:shape){if(path.size()<3||path.size()>1000000-total)return false;total+=path.size();
                for(const auto& point:path)if(point.x<-(INT64_MAX>>2)||point.x>(INT64_MAX>>2)||point.z<-(INT64_MAX>>2)||point.z>(INT64_MAX>>2))return false;}}
        return true;
    }
    if(value.erase||!value.shapes.empty()||!value.fill.is_null())return false;
    if(value.tool==DungeonInputTool::Monster)
        return value.type.empty()&&value.variant.empty()&&value.cost==0&&value.character_type>=0&&value.rank>=0&&value.rank<=2&&value.level>=1;
    return !value.type.empty()&&value.type.size()<=256&&value.variant.size()<=256&&value.type.find('\0')==std::string::npos&&
        value.variant.find('\0')==std::string::npos&&value.character_type==-1&&value.rank==-1&&value.level==-1;
}

inline bool dungeon_input_monster(const MonsterActor& value){
    return value.tag==0&&value.rank>=0&&value.rank<=2&&value.character_slot==1&&value.character_type>=0&&
        value.level>=0&&!value.name.empty()&&value.name.size()<=256&&value.name.find('\0')==std::string::npos&&
        monster_pose(value.position,value.rotation,value.scale);
}
inline bool valid_dungeon_input_state(const DungeonInputState& value){
    if(value==DungeonInputState{})return true;
    for(const auto* sha:{&value.dungeon_sha256,&value.scenes_sha256,&value.quests_sha256,&value.layout_sha256})
        if(!game::network_object_identity::sha(*sha))return false;
    if(value.edits){
        if(value.birth)return false;std::set<std::string> keys;
        for(const auto& row:*value.edits){game::dungeon_gizmo_control::Intent single;
            single.command=game::dungeon_gizmo_control::Command::Destroy;
            single.rows.push_back({row.key,row.value,game::dungeon_gizmo_control::pose(row.value),0});
            if(!keys.insert(row.key).second||!game::dungeon_gizmo_control::valid(single))return false;}
    }
    if(bool(value.scenery_release)!=bool(value.scenery_edits))return false;
    if(value.scenery_edits){
        if(!value.edits||value.birth)return false;std::set<std::string> keys;
        for(const auto& row:*value.edits)keys.insert(row.key);
        for(const auto& row:*value.scenery_edits)if(!game::scenery_scene_key::valid(row.key)||!keys.insert(row.key).second||!game::dungeon_gizmo_control::valid(row.value))return false;
    }
    if(!value.birth)return true;
    const auto& born=*value.birth;
    if(!game::dungeon_gizmo_identity::scene(born.scene)||(uint8_t(born.tool)>2&&born.tool!=DungeonInputTool::Treasure))return false;
    if(born.tool==DungeonInputTool::Monster)return game::monster_target_key::valid(born.key)&&born.gizmo==DungeonGizmoRecord{}&&dungeon_input_monster(born.monster);
    if(!game::dungeon_gizmo_identity::key(born.key)||born.monster!=MonsterActor{})return false;
    const auto& row=born.gizmo;
    for(const auto* word:{&row.name,&row.type,&row.variant})if(word->size()>8192||word->find('\0')!=std::string::npos)return false;
    return row.parent.empty()&&monster_pose(row.position,row.rotation,row.scale);
}
inline bool dungeon_input_controls(const std::string& source,const DungeonInputIntent& intent,bool after,
                                   const DungeonInputState& value){
    if(!intent.controls||!value.edits||value.birth)return false;
    const auto& controls=*intent.controls;using Command=game::dungeon_gizmo_control::Command;
    const bool removed=after&&controls.command==Command::Destroy;
    const size_t expected=removed?0:size_t(std::count_if(controls.rows.begin(),controls.rows.end(),[](const auto& row){return !row.scenery;}));
    const size_t scenery_expected=removed?0:controls.rows.size()-expected;
    if(value.edits->size()!=expected||bool(value.scenery_edits)!=game::dungeon_gizmo_control::mixed(controls)||
       (value.scenery_edits&&value.scenery_edits->size()!=scenery_expected))return false;
    size_t gizmo=0,scenery=0;
    if(!removed)for(size_t i=0;i<controls.rows.size();++i){const auto& wanted=controls.rows[i];
        const auto key=after&&controls.command==Command::Duplicate?game::dungeon_gizmo_control::birth(wanted,source,uint32_t(i)):wanted.key;
        if(wanted.scenery){const auto& actual=value.scenery_edits->at(scenery++);auto record=*wanted.scenery;
            if(after&&(controls.command==Command::Move||controls.command==Command::Duplicate))record.pose=wanted.after;
            // Original NotifyMoved/AddScenery derive height from the current
            // scene floor. The complete observed bits must still match on peers.
            if(after&&(controls.command==Command::Move||controls.command==Command::Duplicate))record.height=actual.value.height;
            if(after&&controls.command==Command::Duplicate)record.automatic=false;
            if(actual.key!=key||actual.value!=record)return false;continue;}
        const auto& actual=value.edits->at(gizmo++);if(actual.key!=key)return false;
        auto record=wanted.before;
        if(after&&controls.command==Command::Move)game::dungeon_gizmo_control::pose(record,wanted.after);
        if(after&&controls.command==Command::Duplicate){
            if(actual.value.type!=record.type||actual.value.variant!=record.variant||!actual.value.parent.empty()||
               game::dungeon_gizmo_control::pose(actual.value)!=wanted.after)return false;
        }else if(actual.value!=record)return false;
    }
    return true;
}
inline bool dungeon_input_transition(const std::string& action,const DungeonInputIntent& intent,
                                     const DungeonInputState& before,const DungeonInputState& after){
    if(!valid_dungeon_input(intent)||!game::dungeon_gizmo_identity::uuid(action)||!valid_dungeon_input_state(before)||
       !valid_dungeon_input_state(after)||before==DungeonInputState{}||after==DungeonInputState{}||before.birth)return false;
    auto expected=before;auto& undo=expected.protected_state.dungeon.undo_count;
    if(undo>20)return false;if(undo<20)++undo;
    expected.protected_state.dungeon.global_random=after.protected_state.dungeon.global_random;
    if(before.dungeon_sha256==after.dungeon_sha256&&before.scenes_sha256==after.scenes_sha256)return false;
    expected.dungeon_sha256=after.dungeon_sha256;expected.scenes_sha256=after.scenes_sha256;
    if(intent.tool==DungeonInputTool::Gizmos){
        if(!dungeon_input_controls(action,intent,false,before)||!dungeon_input_controls(action,intent,true,after))return false;
        expected.edits=after.edits;expected.scenery_edits=after.scenery_edits;
        if(expected.scenery_release&&intent.controls->command==game::dungeon_gizmo_control::Command::Duplicate){
            auto& counter=expected.scenery_release->at(2);if(counter==INT32_MAX)return false;++counter;}
        return expected==after;
    }
    if(before.edits||after.edits||before.scenery_edits||after.scenery_edits||before.scenery_release||after.scenery_release)return false;
    if(intent.tool==DungeonInputTool::Paint)return !after.birth&&expected==after;
    if(!after.birth||after.birth->scene!=intent.scene||after.birth->tool!=intent.tool)return false;
    const auto& born=*after.birth;
    if(intent.tool==DungeonInputTool::Monster){const auto& row=born.monster;
        if(born.key!="action:"+action||row.status!=3||row.character_type!=intent.character_type||row.rank!=intent.rank||row.level!=intent.level||
           row.position!=intent.position||row.rotation!=intent.rotation||row.scale!=std::array<float,3>{1,1,1})return false;
    }else{const auto& row=born.gizmo;
        if(born.key!=game::dungeon_gizmo_identity::birth(action,0)||row.type!=intent.type||row.variant!=intent.variant||
           row.position!=intent.position||row.rotation!=intent.rotation||row.scale!=intent.scale)return false;
    }
    expected.birth=born;return expected==after;
}
inline bool dungeon_input_history_transition(const std::string& source,bool undo,const DungeonInputIntent& intent,
                                             const DungeonInputState& before,const DungeonInputState& after){
    if(!valid_dungeon_input(intent)||!game::dungeon_gizmo_identity::uuid(source)||!valid_dungeon_input_state(before)||
       !valid_dungeon_input_state(after)||before==DungeonInputState{}||after==DungeonInputState{})return false;
    auto expected=before;auto& count=expected.protected_state.dungeon.undo_count;
    if(count>20)return false;if(undo){if(!count)return false;--count;}else if(count<20)++count;
    expected.protected_state.dungeon.global_random=after.protected_state.dungeon.global_random;
    if(before.dungeon_sha256==after.dungeon_sha256&&before.scenes_sha256==after.scenes_sha256)return false;
    expected.dungeon_sha256=after.dungeon_sha256;expected.scenes_sha256=after.scenes_sha256;
    expected.quests_sha256=after.quests_sha256;
    // The current retained graph and full before/after native records prove the
    // inverse actor value. No receiver, UID or native list index travels here.
    if(intent.tool==DungeonInputTool::Gizmos){
        if(!dungeon_input_controls(source,intent,undo,before)||!dungeon_input_controls(source,intent,!undo,after))return false;
        expected.edits=after.edits;expected.scenery_edits=after.scenery_edits;
    }else if(before.edits||after.edits||before.scenery_edits||after.scenery_edits||before.scenery_release||after.scenery_release)return false;
    else if(intent.tool==DungeonInputTool::Paint){if(before.birth||after.birth)return false;}
    else{
        const auto& present=undo?before.birth:after.birth;const auto& absent=undo?after.birth:before.birth;
        const auto key=intent.tool==DungeonInputTool::Monster?"action:"+source:game::dungeon_gizmo_identity::birth(source,0);
        if(!present||absent||present->key!=key||present->scene!=intent.scene||present->tool!=intent.tool)return false;
        expected.birth=after.birth;
    }
    return expected==after;
}

template<class Ledger>bool dungeon_input_purchase(const DungeonInputIntent& intent,const Ledger& before,const Ledger& after){
    auto expected=before;
    if(intent.tool==DungeonInputTool::Gizmos){
        if(!intent.controls)return false;const auto& controls=*intent.controls;using Command=game::dungeon_gizmo_control::Command;
        if(controls.command==Command::Move)return before==after;
        const auto amount=int64_t(game::dungeon_gizmo_control::total(controls))*(controls.command==Command::Destroy?1:-1);
        if(controls.command==Command::Duplicate&&!amount)return before==after;
        const auto add=[](int32_t& value,int64_t delta){const auto next=int64_t(value)+delta;if(next<INT32_MIN||next>INT32_MAX)return false;value=int32_t(next);return true;};
        if(expected.record_present!=1||!add(expected.cash,amount)||!add(expected.record_cash,amount)||
           !add(controls.command==Command::Destroy?expected.record_income:expected.record_expense,amount<0?-amount:amount)||
           !add(expected.line_counts[6],controls.command==Command::Duplicate?1:game::dungeon_gizmo_control::cash_count(controls))||!add(expected.line_income[6],amount))return false;
        return expected==after;
    }
    if(intent.tool!=DungeonInputTool::Door&&intent.tool!=DungeonInputTool::Key&&intent.tool!=DungeonInputTool::Treasure)return expected==after;
    const auto add=[](int32_t& value,int64_t amount){const auto next=int64_t(value)+amount;if(next<INT32_MIN||next>INT32_MAX)return false;value=int32_t(next);return true;};
    if(intent.cost<0||expected.record_present!=1||!add(expected.cash,-int64_t(intent.cost))||!add(expected.record_cash,-int64_t(intent.cost))||
       !add(expected.record_expense,intent.cost)||!add(expected.line_counts[6],1)||!add(expected.line_income[6],-int64_t(intent.cost)))return false;
    return expected==after;
}
template<class Ledger>bool dungeon_input_history_cash(bool undo,const DungeonInputIntent& intent,const Ledger& before,const Ledger& after){
    auto expected=before;
    if(intent.tool==DungeonInputTool::Gizmos){
        if(!intent.controls)return false;const auto& controls=*intent.controls;using Command=game::dungeon_gizmo_control::Command;
        if(controls.command==Command::Move)return before==after;
        const auto amount=int64_t(game::dungeon_gizmo_control::total(controls))*(controls.command==Command::Destroy?-1:1)*(undo?1:-1);
        const auto add=[](int32_t& value,int64_t delta){const auto next=int64_t(value)+delta;if(next<INT32_MIN||next>INT32_MAX)return false;value=int32_t(next);return true;};
        if(expected.record_present!=1||!add(expected.cash,amount)||!add(expected.record_cash,amount)||!add(expected.record_income,amount)||
           !add(expected.line_counts[6],game::dungeon_gizmo_control::cash_count(controls))||!add(expected.line_income[6],amount))return false;
        return expected==after;
    }
    const bool cash=intent.tool==DungeonInputTool::Door||intent.tool==DungeonInputTool::Treasure||(intent.tool==DungeonInputTool::Key&&intent.cost!=0);
    if(!cash)return expected==after;const auto amount=undo?int64_t(intent.cost):-int64_t(intent.cost);
    const auto add=[](int32_t& value,int64_t delta){const auto next=int64_t(value)+delta;if(next<INT32_MIN||next>INT32_MAX)return false;value=int32_t(next);return true;};
    if(intent.cost<0||expected.record_present!=1||!add(expected.cash,amount)||!add(expected.record_cash,amount)||!add(expected.record_income,amount)||
       !add(expected.line_counts[6],1)||!add(expected.line_income[6],amount))return false;
    return expected==after;
}
}
