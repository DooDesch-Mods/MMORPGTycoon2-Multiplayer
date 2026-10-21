#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline bool weapon_checkbox_mode=false,weapon_checkbox_enabled=true;
inline int32_t character_kind=0;inline bool edited_before=false,equal_apply=false;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline CostumeRecord record(){CostumeRecord v;v.name="Existing costume";v.skeleton="humanoid";v.flag=true;
    CostumePart p;p.bone="head";p.model="head/human";p.pose.position={1,2,3};p.pose.scale={1,1.5f,2};p.model_scale=1.25f;p.colors.push_back({.1f,.2f,.3f,1});
    CostumeAttachment attachment;attachment.name="hat/model";attachment.pose.position={4,5,6};p.attachments.push_back(attachment);v.parts.push_back(p);return v;}
inline CostumeEditIntent intent(){auto v=record();if(!equal_apply){v.name="Changed costume";v.parts[0].pose.position[0]=13;v.parts[0].attachments[0].pose.scale[1]=2;v.parts[0].colors[0][2]=.8f;}CostumeEditIntent result{character_kind,1,std::string(64,'d'),v};
    if(weapon_checkbox_mode){result.record=record();result.weapon_category=CostumeWeaponIntent{weapon_checkbox_enabled?"Axe":"Sword",weapon_checkbox_enabled};}return result;}
inline CostumeEditState before(){CostumeEditState v;v.owner.day=28;v.owner.rename_day=20;v.owner.count=13;v.owner.name=v.owner.definition_name="Existing type";
    v.owner.all_sha256=std::string(64,'a');v.owner.others_sha256=std::string(64,'b');v.owner.edited=edited_before;v.owner.released=true;v.owner.undo_count=2;
    v.owner.release_edits={1,2,3,4,5,6,7,8};v.owner.global_random={11,12,13,14};v.record=record();v.all_sha256=std::string(64,'e');v.count=22;if(weapon_checkbox_mode)v.weapon_categories={"Sword","Staff","Bow"};return v;}
inline CostumeEditState after(CostumeEditState v){if(weapon_checkbox_mode){
    std::vector<std::string> categories;need(costume_weapon_after(v.weapon_categories,*intent().weapon_category,categories),"fixture original checkbox effect absent");
    v.weapon_categories=std::move(categories);v.all_sha256=std::string(64,'f');return v;}v.record=intent().record;v.owner.edited=true;v.owner.rename_day=v.owner.day;v.owner.all_sha256=std::string(64,'c');
    if(!edited_before)++v.owner.release_edits[7];++v.owner.global_random[0];if(!equal_apply)v.all_sha256=std::string(64,'f');return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::EditCharacterCostume;r.peer=2;r.sequence=1;
    r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=costume_key(intent());r.costume_edit=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.costume_edit_before=before();m.evidence.costume_edit_after=after(before());m.evidence.retirement_confirmed=true;return m;}
