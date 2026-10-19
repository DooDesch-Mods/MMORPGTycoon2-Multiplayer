#pragma once
#include "group_transition.h"
namespace mt2mp::game::group_history {
namespace a=action_session;
enum class Kind:uint8_t {Add=1,Remove=2};
struct Node {
    Kind kind=Kind::Remove;
    std::string key;
    int32_t region=-1;
    std::array<uint32_t,10> saved_pose{};
    std::vector<std::string> members;
    bool operator==(const Node&)const=default;
};
inline a::GroupState canonical(a::GroupState state){
    // Native DoAddToGroup appends a recreated Group to its manager. The stable
    // identity is retained across member changes and manager reordering.
    std::sort(state.groups.begin(),state.groups.end(),[](const auto& x,const auto& y){return x.key<y.key;});return state;
}
inline bool project(const a::GroupState& before,const std::vector<Node>& nodes,a::GroupState& expected,std::string& e){
    auto fail=[&](const std::string& why){e=why;return false;};
    if(nodes.empty())return fail("Group history original node set is empty");
    expected=before;std::set<std::string> targets,touched;
    for(const auto& node:nodes){
        if((node.kind!=Kind::Add&&node.kind!=Kind::Remove)||node.key.empty()||node.members.empty()||node.region<0||
           !targets.insert(node.key).second)return fail("Group history original target/kind/ordered members invalid");
        auto g=std::find_if(expected.groups.begin(),expected.groups.end(),[&](const auto& row){return row.key==node.key;});
        if(node.kind==Kind::Remove&&g==expected.groups.end())return fail("Group history remove target absent key="+node.key);
        if(g!=expected.groups.end()&&g->region!=node.region)return fail("Group history target Region differs key="+node.key);
        if(node.kind==Kind::Add&&g==expected.groups.end()){
            expected.groups.push_back({node.key,node.region,node.saved_pose,{}});g=std::prev(expected.groups.end());
        }
        for(const auto& key:node.members){
            if(!touched.insert(key).second)return fail("Group history original member appears in multiple nodes");
            auto prop=std::find_if(expected.props.begin(),expected.props.end(),[&](const auto& row){return row.key==key;});
            if(prop==expected.props.end())return fail("Group history exact live Scenery member absent key="+key);
            auto member=std::find_if(g->members.begin(),g->members.end(),[&](const auto& row){return row.key==key;});
            if(node.kind==Kind::Remove){
                if(member==g->members.end()||prop->parent!=node.key)return fail("Group history remove reciprocal member differs key="+key);
                g->members.erase(member);prop->parent.clear();
            }else{
                if(member!=g->members.end()||!prop->parent.empty())return fail("Group history add requires exact free Scenery key="+key);
                g->members.push_back({key,{}});prop->parent=node.key;
            }
        }
        if(g->members.empty())expected.groups.erase(g);
    }
    return true;
}
inline bool effect(const a::GroupState& before,const a::GroupState& after,const std::vector<Node>& nodes,std::string& e){
    auto fail=[&](const std::string& why){e=why;return false;};a::GroupState expected;
    if(before.random!=after.random)return fail("Group history original changed RNG");
    if(!project(before,nodes,expected,e))return false;
    if(after.props!=expected.props)return fail("Group history original changed Scenery pose/Region or reciprocal membership");
    const auto actual=canonical(after);expected=canonical(std::move(expected));
    if(actual.groups.size()!=expected.groups.size())return fail("Group history original complete Group count differs");
    for(size_t i=0;i<expected.groups.size();++i){const auto& wanted=expected.groups[i];const auto& got=actual.groups[i];
        if(got.key!=wanted.key||got.region!=wanted.region||got.members.size()!=wanted.members.size())return fail("Group history result Group key/Region/member count differs");
        const auto node=std::find_if(nodes.begin(),nodes.end(),[&](const auto& row){return row.key==wanted.key;});
        // AddProps invokes original _Recenter, which updates the group's pose
        // and relative transforms. RemoveProps preserves surviving values.
        if(node==nodes.end()||node->kind==Kind::Remove){if(got!=wanted)return fail("Group history changed surviving Group transform/order");}
        else for(size_t n=0;n<wanted.members.size();++n)if(got.members[n].key!=wanted.members[n].key)
            return fail("Group history original AddProps member ordering differs");
    }
    return true;
}
inline bool structural_effect(const a::GroupState& before,const a::GroupState& after){
    if(canonical(before)!=before||canonical(after)!=after)return false;
    std::vector<Node> nodes;
    for(const auto& old:before.groups){
        const auto current=std::find_if(after.groups.begin(),after.groups.end(),[&](const auto& v){return v.key==old.key;});
        std::vector<std::string> prior,next;for(const auto& m:old.members)prior.push_back(m.key);
        if(current!=after.groups.end())for(const auto& m:current->members)next.push_back(m.key);
        if(prior==next)continue;
        Node node;node.key=old.key;node.region=old.region;node.saved_pose=old.pose;
        if(next.size()>prior.size()&&std::equal(prior.begin(),prior.end(),next.begin())){
            node.kind=Kind::Add;node.members.assign(next.begin()+prior.size(),next.end());
        }else{node.kind=Kind::Remove;for(const auto& member:prior)if(std::find(next.begin(),next.end(),member)==next.end())node.members.push_back(member);}
        nodes.push_back(std::move(node));
    }
    for(const auto& current:after.groups)if(std::none_of(before.groups.begin(),before.groups.end(),[&](const auto& v){return v.key==current.key;})){
        Node node{Kind::Add,current.key,current.region,current.pose,{}};for(const auto& m:current.members)node.members.push_back(m.key);nodes.push_back(std::move(node));}
    std::string error;return effect(before,after,nodes,error);
}
inline bool same_nodes(const std::vector<Node>& first,const std::vector<Node>& second,bool inverse=false){
    if(first.size()!=second.size())return false;
    for(const auto& node:first){const auto found=std::find_if(second.begin(),second.end(),[&](const auto& v){return v.key==node.key;});
        if(found==second.end()||found->region!=node.region||found->members!=node.members||
           (inverse?found->kind==node.kind:found->kind!=node.kind))return false;}
    return true;
}
}
