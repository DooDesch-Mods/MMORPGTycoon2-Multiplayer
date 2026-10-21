#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline bool adjustment_mode=false,adjustment_cost=false,adjustment_remove=false;
inline int32_t character_kind=0;inline bool edited_before=false,equal_apply=false;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline AbilityRecord record(){AbilityRecord v;v.text={"Strike","icon","weapon","animation","path","cast","effect","description"};
    v.item_value=2;v.item_kind=1;v.item_flag=true;v.selectors={1,2,3,4,5};v.color={.1f,.2f,.3f,1};v.timing={0,1,2};v.final_values={1,2};v.level=1;
    v.effects.push_back({{0,1,2},{3,4},5});v.costs.push_back({{1,2,3},{4,5},6});if(adjustment_mode){v.effects[0].integers={1,0,0};v.costs[0].integers={2,0,1};v.costs[0].values[0]=-4;}return v;}
inline AbilityEditIntent intent(){auto v=record();if(adjustment_mode){const auto value=(adjustment_cost?v.costs:v.effects)[0].values[0];
    AbilityAdjustment change{adjustment_cost,0,adjustment_remove?-value:(adjustment_cost?-1.f:1.f)};AbilityRecord target;
    need(ability_adjusted_record(v,change,target),"Ability adjustment fixture target invalid");AbilityEditIntent out{character_kind,1,2,std::string(64,'d'),target};out.adjustment=change;return out;}if(!equal_apply){v.text[0]="Strong strike";v.text[7]="A changed description";v.effects[0].values[0]=13;}return {character_kind,1,2,std::string(64,'d'),v};}
inline AbilityEditState before(){AbilityEditState v;v.owner.day=28;v.owner.rename_day=20;v.owner.count=13;v.owner.name=v.owner.definition_name="Existing type";
    v.owner.all_sha256=std::string(64,'a');v.owner.others_sha256=std::string(64,'b');v.owner.edited=edited_before;v.owner.released=true;v.owner.undo_count=2;
    v.owner.release_edits={1,2,3,4,5,6,7,8};v.owner.global_random={11,12,13,14};v.record=record();v.all_sha256=std::string(64,'e');v.count=22;return v;}
inline AbilityEditState after(AbilityEditState v){v.record=intent().record;if(adjustment_mode){v.all_sha256=std::string(64,'f');return v;}v.owner.edited=true;v.owner.rename_day=v.owner.day;v.owner.all_sha256=std::string(64,'c');
    if(!edited_before)++v.owner.release_edits[7];++v.owner.global_random[0];if(!equal_apply)v.all_sha256=std::string(64,'f');return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::EditCharacterAbility;r.peer=2;r.sequence=1;
    r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=ability_key(intent());r.ability_edit=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.ability_edit_before=before();m.evidence.ability_edit_after=after(before());m.evidence.retirement_confirmed=true;return m;}
