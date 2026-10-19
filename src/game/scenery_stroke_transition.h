#pragma once
#include "typed_action/scenery_stroke_action.h"
#include "duplicate_transition.h"
#include <map>

namespace mt2mp::game::scenery_stroke_transition {
namespace a=action_session;
inline bool shape(const a::TerrainShape& value,size_t& points,size_t budget){
    if(value.empty())return false;
    for(const auto& path:value){
        if(path.size()<3||points>budget||path.size()>budget-points)return false;
        points+=path.size();
        for(const auto& p:path)if(p.x<-(INT64_MAX>>2)||p.x>(INT64_MAX>>2)||p.z<-(INT64_MAX>>2)||p.z>(INT64_MAX>>2))return false;
    }
    return true;
}
inline bool valid(const a::SceneryStrokeIntent& v,size_t budget){
    const bool brush=v.kind==a::SceneryStrokeKind::Paint;
    if((!brush&&v.kind!=a::SceneryStrokeKind::Erase)||v.region<0||v.stamps.empty()||v.stamps.size()>budget)return false;
    if(brush){
        if((v.mode!=1&&v.mode!=2)||v.type.empty()||v.type.size()>256||v.variant.size()>256||
           v.type.find('\0')!=std::string::npos||v.variant.find('\0')!=std::string::npos||
           (v.random_variant?!v.variant.empty():v.variant.empty()))return false;
    }else if(v.mode!=1||!v.type.empty()||!v.variant.empty()||v.replace||v.random_variant||v.bounds!=std::array<uint32_t,6>{})return false;
    for(auto bits:v.finish_position)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    for(auto bits:v.bounds)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    size_t points=0,dots=0;
    for(const auto& stamp:v.stamps){
        if(!shape(stamp.shape,points,budget)||dots>budget||stamp.dots.size()>budget-dots)return false;
        dots+=stamp.dots.size();
        if((!brush||v.mode!=2)&&!stamp.dots.empty())return false;
        for(const auto& pose:stamp.dots)if(!duplicate_transition::pose(pose))return false;
    }
    return true;
}
inline bool valid(const a::SceneryStrokeState& v){
    return duplicate_transition::sha(v.props_sha)&&duplicate_transition::sha(v.bugs_sha)&&v.bug_count<=1000000;
}
template<class Key>bool object(const a::DuplicateObject& row,Key&& key){
    const bool group=row.family==a::DuplicateFamily::Group;
    if((group?(!row.key.starts_with("group:")||!key(row.key.substr(6))):!key(row.key))||
       row.family<a::DuplicateFamily::Building||row.family>a::DuplicateFamily::Group||row.region<0||
       !duplicate_transition::sha(row.record_sha)||!duplicate_transition::pose(row.pose))return false;
    if(!row.parent.empty()&&(!row.parent.starts_with("group:")||!key(row.parent.substr(6))))return false;
    if(!group)return row.members.empty();
    if(!row.parent.empty())return false;std::set<std::string> members;
    for(const auto& member:row.members)if(!key(member)||!members.insert(member).second)return false;
    return true;
}
template<class Key>bool valid(const a::SceneryStrokeEffect& v,Key&& key,size_t budget,bool history=false){
    if(!duplicate_transition::sha(v.untouched_sha)||v.created.size()>budget||v.removed.size()>budget||v.changed.size()>budget)return false;
    std::set<std::string> before,after;
    for(const auto& row:v.removed)if(!object(row,key)||row.family!=a::DuplicateFamily::Scenery||!before.insert(row.key).second)return false;
    for(const auto& row:v.created)if(!object(row,key)||row.family!=a::DuplicateFamily::Scenery||(!history&&!row.key.starts_with("action:"))||!after.insert(row.key).second)return false;
    for(const auto& change:v.changed){
        const auto& b=change.before;const auto& c=change.after;
        if(!object(b,key)||!object(c,key)||b==c||b.key!=c.key||b.family!=c.family||b.region!=c.region||b.pose!=c.pose||b.parent!=c.parent||
           !before.insert(b.key).second||!after.insert(c.key).second)return false;
        if(b.family==a::DuplicateFamily::Scenery)return false;
        if(b.family==a::DuplicateFamily::Building&&b.members!=c.members)return false;
        if(b.family==a::DuplicateFamily::Group){
            size_t index=0;
            for(const auto& member:b.members){
                if(index<c.members.size()&&c.members[index]==member){++index;continue;}
                if(std::none_of(v.removed.begin(),v.removed.end(),[&](const auto& row){return row.key==member&&row.parent==b.key;}))return false;
            }
            if(index!=c.members.size())return false;
        }
    }
    for(const auto& row:v.created)if(before.contains(row.key)||!row.parent.empty())return false;
    return true;
}
template<class Key>bool effect(const a::SceneryStrokeIntent& intent,const a::SceneryStrokeState& before,const a::SceneryStrokeState& after,
                              const a::SceneryStrokeEffect& result,Key&& key,size_t budget,std::string& e){
    auto fail=[&](const char* cause){e=cause;return false;};
    if(!valid(intent,budget)||!valid(before)||!valid(after)||!valid(result,key,budget))return fail("Scenery stroke complete intent/state/result is invalid");
    const bool brush=intent.kind==a::SceneryStrokeKind::Paint;
    if((!brush||intent.mode==1)&&!result.created.empty())return fail("Scenery erasure created an object");
    if(brush&&intent.mode==2&&!intent.replace&&!result.removed.empty())return fail("Scenery non-replacing brush removed an object");
    for(const auto* rows:{&result.removed,&result.created})for(const auto& row:*rows)
        if(row.region!=intent.region)return fail("Scenery stroke changed another Region");
    for(const auto& change:result.changed){
        if(change.before.region!=intent.region)return fail("Scenery stroke related object belongs to another Region");
        if(!brush&&change.before.family!=a::DuplicateFamily::Group)return fail("Scenery eraser changed an unrelated owned record");
    }
    const bool edited=!result.removed.empty()||!result.created.empty();
    if(!edited)return fail("Scenery stroke has no original object effect");
    if(before.props_sha==after.props_sha)return fail("Scenery stroke complete prop state did not change");
    for(size_t i=0;i<8;++i)if(int64_t(after.release[i])-before.release[i]!=(brush&&i==2?1:0))
        return fail("Scenery stroke original Release edit differs");
    if(!brush&&(before.random!=after.random||before.region_random!=after.region_random||before.bugs_sha!=after.bugs_sha||before.bug_count!=after.bug_count))
        return fail("Scenery eraser changed RNG or bug state");
    return true;
}
}
