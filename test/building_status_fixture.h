#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline int32_t fixture_desired=0,prior=1;inline bool is_dungeon=false,dungeon_ui=false;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline BuildingStatusIntent intent(){return {"action:33333333-3333-4333-8333-333333333333",prior,fixture_desired,dungeon_ui};}
inline BuildingStatusState before(){BuildingStatusState v;v.state=prior;v.bandwidth=prior?.5f:0;v.dungeon=is_dungeon;v.dungeon_ready=is_dungeon;
    v.bugs=12;v.jobs=5;v.linked=2;v.developers=3;v.bugs_sha256=std::string(64,'a');v.expected_bugs_sha256=std::string(64,'b');v.pool_sha256=std::string(64,'c');v.undo_count=2;v.global_random={11,12,13,14};return v;}
inline BuildingStatusState after(BuildingStatusState v){v.state=fixture_desired;v.bandwidth=fixture_desired?.5f:0;v.user_set=true;v.bugs_sha256=v.expected_bugs_sha256;v.pool_sha256=std::string(64,'d');
    if(fixture_desired==2){v.jobs-=v.linked;v.linked=0;}return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::SetBuildingStatus;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.building_status=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.building_status_before=before();m.evidence.building_status_after=after(before());m.evidence.retirement_confirmed=true;return m;}
