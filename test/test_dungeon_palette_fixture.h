#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline bool equal_apply=false,sandbox=false,default_palette=false;inline int32_t previous_count=1;
inline DungeonPaletteEdit palette_edit=DungeonPaletteEdit::Colors;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline DungeonPaletteIntent intent(){DungeonPaletteIntent v;v.key="action:33333333-3333-4333-8333-333333333333";v.before_sha256=std::string(64,'a');v.defaults=default_palette;v.edit=palette_edit;
    for(size_t i=0;i<4;++i){v.colors[i]={float(i)*.1f,.25f,.5f,1.f};v.packed[i]=uint32_t(0xff804000+i);}return v;}
inline DungeonPaletteState before(){DungeonPaletteState v;auto& sound=v.protected_state;for(size_t i=0;i<sound.light.size();++i)sound.light[i]=float(i+1)*.1f;sound.atmosphere=3;
    auto& d=sound.dungeon;d.desired=previous_count;d.price=100;d.sandbox=sandbox;d.instances={{0,true},{2,false}};d.global_random={11,12,13,14};d.undo_count=2;
    v.defaults=equal_apply?default_palette:!default_palette;v.palette={1,2,3,4,0xfeedcafe};v.cells={{"stone",{11,12,13,14},true},{"brick",{21,22,23,24},false}};
    if(equal_apply)for(size_t i=0;i<4;++i)v.palette[i]=intent().packed[i];return v;}
inline DungeonPaletteState after(DungeonPaletteState v){v.defaults=intent().defaults;for(size_t i=0;i<4;++i)v.palette[i]=intent().packed[i];return v;}
inline Ledger money(){Ledger v;v.cash=v.record_cash=10000;v.flat_day=v.record_day=28;v.record_present=1;v.record_income=500;v.record_expense=100;v.line_counts[0]=2;v.line_income[0]=-100;return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::SetDungeonPalette;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.dungeon_palette=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.before=m.evidence.after=money();m.evidence.dungeon_palette_before=before();m.evidence.dungeon_palette_after=after(before());m.evidence.retirement_confirmed=true;return m;}
