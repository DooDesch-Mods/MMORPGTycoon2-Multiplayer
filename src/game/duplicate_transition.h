#pragma once
#include "typed_action/duplicate_action.h"
#include "building_uptime.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
namespace mt2mp::game::duplicate_transition {
namespace a=action_session;
inline bool sha(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool pose(const std::array<uint32_t,10>& p){
    double norm=0;for(size_t i=0;i<p.size();++i){const auto f=std::bit_cast<float>(p[i]);if(!std::isfinite(f))return false;if(i<4)norm+=double(f)*f;}
    return std::abs(norm-1)<0.002&&std::bit_cast<float>(p[7])>0&&std::bit_cast<float>(p[8])>0&&std::bit_cast<float>(p[9])>0;
}
template<class Key>bool valid(const a::DuplicateIntent& v,Key&& key){
    if(v.rows.empty())return false;std::set<std::string> sources,created,members;int64_t cost=0;
    for(auto n:v.cash_position)if(!std::isfinite(std::bit_cast<float>(n)))return false;
    for(const auto& row:v.rows){const bool group=row.family==a::DuplicateFamily::Group;
        if((!group&&!key(row.key))||(group&&(!row.key.starts_with("group:")||!key(row.key.substr(6))))||
           row.family<a::DuplicateFamily::Building||row.family>a::DuplicateFamily::Group||!sources.insert(row.key).second||
           row.region<0||row.cost<0||!sha(row.record_sha)||!pose(row.before)||!pose(row.after)||row.created_keys.empty())return false;
        if(row.captured_uptime&&(row.family!=a::DuplicateFamily::Building||!building_uptime::valid(*row.captured_uptime)))return false;
        cost+=row.cost;if(cost>INT32_MAX)return false;
        if(group){if(row.members.empty()||row.members.size()!=row.created_keys.size())return false;
            for(const auto& m:row.members)if(!key(m)||!members.insert(m).second)return false;
        }else if(!row.members.empty()||row.created_keys.size()!=1)return false;
        for(const auto& k:row.created_keys)if(!k.starts_with("action:")||!key(k)||!created.insert(k).second)return false;
    }
    for(const auto& k:created)if(sources.contains(k)||members.contains(k))return false;
    for(const auto& k:members)if(sources.contains(k))return false;
    return true;
}
template<class Key>bool valid(const a::DuplicateState& v,Key&& key){
    if(!sha(v.untouched_sha)||v.sources.empty())return false;std::set<std::string> keys;
    if(v.history){const auto& history=*v.history;
        if(!sha(history.services_sha)||!sha(history.quests_sha)||!sha(history.related_sha))return false;
        std::set<std::string> related;
        for(const auto& candidate:history.related_keys)if(!key(candidate)||!related.insert(candidate).second)return false;
        for(const auto& row:v.created)if(related.contains(row.key))return false;
    }
    auto rows=[&](const auto& values){for(const auto& row:values){const bool group=row.family==a::DuplicateFamily::Group;
            if((group?(!row.key.starts_with("group:")||!key(row.key.substr(6))):!key(row.key))||!keys.insert(row.key).second||
               row.family<a::DuplicateFamily::Building||row.family>a::DuplicateFamily::Group||row.region<0||!pose(row.pose)||!sha(row.record_sha))return false;
            if(!row.parent.empty()&&(!row.parent.starts_with("group:")||!key(row.parent.substr(6))))return false;
            if(group){if(!row.parent.empty()||row.members.empty())return false;
                std::set<std::string> members;for(const auto& k:row.members)if(!key(k)||!members.insert(k).second)return false;
            }else if(!row.members.empty())return false;
        }return true;};
    return rows(v.sources)&&rows(v.created);
}
inline bool effect(const a::DuplicateIntent& intent,const a::DuplicateState& before,const a::DuplicateState& after,std::string& e){
    auto fail=[&](const std::string& s){e=s;return false;};
    if(before.history||after.history)return fail("Duplicate placement contains unrelated history evidence");
    if(!before.created.empty()||before.sources.size()!=intent.rows.size()||before.sources!=after.sources||before.untouched_sha!=after.untouched_sha)
        return fail("Duplicate source records/order or unrelated complete census changed");
    size_t index=0;bool building=false,scenery=false;
    for(size_t i=0;i<intent.rows.size();++i){const auto& row=intent.rows[i];const auto& prior=before.sources[i];
        // Native admission proves the full captured SHA with only its explicit
        // uptime sequence restored. Evidence keeps the actual Host-before SHA,
        // which must remain unchanged through Clone and match on the Client.
        if(prior.key!=row.key||(!row.captured_uptime&&prior.record_sha!=row.record_sha)||
           (row.captured_uptime&&(row.family!=a::DuplicateFamily::Building||!building_uptime::valid(*row.captured_uptime)))||prior.family!=row.family||prior.region!=row.region||prior.pose!=row.before||prior.members!=row.members)
            return fail("Duplicate requested original complete source differs index="+std::to_string(i));
        if(index>=after.created.size())return fail("Duplicate original result count too short");
        const auto& top=after.created[index++];const bool group=row.family==a::DuplicateFamily::Group;
        const auto expected_key=group?"group:"+row.created_keys.front():row.created_keys.front();
        if(top.key!=expected_key||top.family!=row.family||top.pose!=row.after||!top.parent.empty())
            return fail("Duplicate original result key/family/requested pose differs index="+std::to_string(i));
        if(group){if(top.members!=row.created_keys)return fail("Duplicate original Group child order differs");
            for(const auto& k:row.created_keys){if(index>=after.created.size())return fail("Duplicate original Group child result absent");
                const auto& child=after.created[index++];if(child.key!=k||child.family!=a::DuplicateFamily::Scenery||child.parent!=top.key||child.region!=top.region)
                    return fail("Duplicate original Group child canonical owner differs");}
        }else if(!top.members.empty())return fail("Duplicate non-Group result contains members");
        building|=row.family==a::DuplicateFamily::Building;scenery|=row.family==a::DuplicateFamily::Scenery;
    }
    if(index!=after.created.size())return fail("Duplicate original result contains extra objects");
    for(size_t i=0;i<8;++i){const int64_t delta=int64_t(after.release[i])-before.release[i];
        if(delta!=int64_t((i==1&&building)||(i==2&&scenery)))return fail("Duplicate original Release edit differs index="+std::to_string(i));}
    return true;
}
}
