#pragma once
#include "typed_action/group_action.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <set>
namespace mt2mp::game::group_transition {
namespace a=action_session;
inline bool finite(const std::array<uint32_t,10>& pose){
    for(auto v:pose)if(!std::isfinite(std::bit_cast<float>(v)))return false;return true;
}
template<class Key>bool valid(const a::GroupState& state,Key&& key){
    std::set<std::string> groups,members,props;
    for(const auto& g:state.groups){
        if(g.region<0||!g.key.starts_with("group:")||!key(g.key.substr(6))||!groups.insert(g.key).second||!finite(g.pose))return false;
        for(const auto& m:g.members)if(!key(m.key)||!members.insert(m.key).second||!finite(m.relative))return false;
    }
    std::string last;
    for(const auto& p:state.props){
        if(!key(p.key)||p.region<0||!finite(p.pose)||(!last.empty()&&p.key<=last)||!props.insert(p.key).second)return false;last=p.key;
        size_t matches=0;for(const auto& g:state.groups)for(const auto& m:g.members)if(m.key==p.key){if(g.key!=p.parent)return false;++matches;}
        if(matches!=(p.parent.empty()?0u:1u))return false;
    }
    return std::includes(props.begin(),props.end(),members.begin(),members.end());
}
struct Plan {std::string target;std::vector<std::string> added,removed;size_t inverse_count=0;bool create=false;};
inline bool plan(const a::GroupState& before,const a::GroupIntent& intent,Plan& out,std::string& e,const std::string& created_key={}){
    out={};auto fail=[&](const char* why){e=why;return false;};
    if(intent.kind!=a::GroupKind::Group&&intent.kind!=a::GroupKind::Ungroup)return fail("Group operation invalid");
    if(intent.selected.empty()||(intent.kind==a::GroupKind::Group&&intent.selected.size()<2))return fail("Group operation selected count invalid");
    std::set<std::string> selected;
    auto group=[&](const std::string& k)->const a::GroupRow*{for(const auto& v:before.groups)if(v.key==k)return &v;return nullptr;};
    for(const auto& k:intent.selected){if(!selected.insert(k).second)return fail("Group selected canonical target repeats");
        if(const auto* g=group(k)){if(intent.kind==a::GroupKind::Group&&out.target.empty())out.target=g->key;continue;}
        const auto p=std::find_if(before.props.begin(),before.props.end(),[&](const auto& v){return v.key==k;});
        if(p==before.props.end()||!p->parent.empty())return fail("Group selection lacks exact free Scenery/current Group");
    }
    for(const auto& k:intent.selected){if(const auto* g=group(k)){
            if(intent.kind==a::GroupKind::Group&&k==out.target)continue;
            out.removed.push_back(k);for(const auto& p:g->members)out.added.push_back(p.key);
        }else if(intent.kind==a::GroupKind::Group)out.added.push_back(k);}
    if(intent.kind==a::GroupKind::Group){
        if(out.added.empty())return fail("Group selection adds no member");
        out.create=out.target.empty();if(out.create)out.target=created_key.empty()?"group:"+out.added.front():created_key;out.inverse_count=1;
    }else{if(out.removed.empty())return fail("Ungroup selection contains no Group");out.inverse_count=out.removed.size();}
    e.clear();return true;
}
inline bool effect(const a::GroupState& before,const a::GroupState& after,const a::GroupIntent& intent,std::string& e,const std::string& created_key={}){
    Plan p;if(!plan(before,intent,p,e,created_key))return false;
    auto fail=[&](const char* why){e=why;return false;};
    if(after.random!=before.random)return fail("Group original changed global RNG");
    auto expected_props=before.props;
    for(auto& prop:expected_props){
        if(std::find(p.added.begin(),p.added.end(),prop.key)!=p.added.end())prop.parent=intent.kind==a::GroupKind::Group?p.target:"";
    }
    if(after.props!=expected_props)return fail("Group original changed Scenery pose/Region or reciprocal membership differs");
    std::vector<a::GroupRow> survivors;const a::GroupRow* original=nullptr;
    for(const auto& g:before.groups){if(g.key==p.target)original=&g;
        if(std::find(p.removed.begin(),p.removed.end(),g.key)==p.removed.end())survivors.push_back(g);}
    if(intent.kind==a::GroupKind::Ungroup)return after.groups==survivors||fail("Ungroup removed/surviving complete ordered Groups differ");
    auto found=std::find_if(after.groups.begin(),after.groups.end(),[&](const auto& g){return g.key==p.target;});
    if(found==after.groups.end())return fail("Group result target absent");
    std::vector<std::string> keys;if(original)for(const auto& m:original->members)keys.push_back(m.key);
    keys.insert(keys.end(),p.added.begin(),p.added.end());
    if(found->members.size()!=keys.size())return fail("Group complete member count differs");
    for(size_t i=0;i<keys.size();++i)if(found->members[i].key!=keys[i])return fail("Group original member ordering differs");
    const auto first=std::find_if(before.props.begin(),before.props.end(),[&](const auto& v){return v.key==keys.front();});
    const int32_t region=original?original->region:first->region;
    if(found->region!=region)return fail("Group original Region differs");
    if(p.create)survivors.push_back(*found);else for(auto& g:survivors)if(g.key==p.target)g=*found;
    return after.groups==survivors||fail("Group result changed unrelated Groups/order");
}
}
