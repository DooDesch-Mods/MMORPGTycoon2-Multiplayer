#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace mt2mp::game::dungeon_gizmo_restore {
struct Expected {uintptr_t node=0,manager=0;bool operator==(const Expected&)const=default;};
struct Created {uintptr_t node=0,object=0;bool operator==(const Created&)const=default;};
class Receipts {
    std::map<uintptr_t,Expected> expected_;std::map<uintptr_t,Created> created_;
public:
    bool prepare(const std::vector<Expected>& expected,std::string& error){
        expected_.clear();created_.clear();for(const auto& row:expected){
            if(!row.node||row.node>UINTPTR_MAX-0x108||!row.manager||!expected_.emplace(row.node,row).second){error="Dungeon original Gizmo creation receipt has an invalid or repeated Make node";return false;}}
        return true;
    }
    bool associate(uintptr_t manager,uintptr_t type,uintptr_t variant,uintptr_t transform,uintptr_t& node,std::string& error)const{
        node=0;if(transform<0x108){error="Dungeon original Gizmo call has no stored Make transform";return false;}
        const auto candidate=transform-0x108;const auto found=expected_.find(candidate);
        if(found==expected_.end()||found->second.manager!=manager||type!=candidate+0xc0||variant!=candidate+0xe0){
            error="Dungeon original Gizmo call does not own the exact stored Make arguments";return false;}
        if(created_.contains(candidate)){error="Dungeon original Make Gizmo executed more than once";return false;}
        node=candidate;return true;
    }
    bool record(uintptr_t node,uintptr_t object,std::string& error){
        if(!expected_.contains(node)||!object){error="Dungeon original Make Gizmo returned no owned object";return false;}
        for(const auto& [other,row]:created_)if(row.object==object){error="Dungeon original Make Gizmo returned an object already owned by another receipt";return false;}
        if(!created_.emplace(node,Created{node,object}).second){error="Dungeon original Make Gizmo receipt was repeated";return false;}return true;
    }
    bool complete(std::vector<Created>& out,std::string& error)const{
        out.clear();if(created_.size()!=expected_.size()){error="Dungeon original Gizmo creation receipt count differs: actual="+std::to_string(created_.size())+" required="+std::to_string(expected_.size());return false;}
        for(const auto& [node,row]:created_)out.push_back(row);return true;
    }
};
}
