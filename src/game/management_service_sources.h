#pragma once
#include "management_service_graph.h"
#include "service_membership_reader.h"
#include <map>
#include <set>

namespace mt2mp::game::management_service_graph {
struct BuildingSource {uintptr_t object=0,shard=0,region=0;uint64_t uid=0;std::string key;bool operator==(const BuildingSource&)const=default;};
struct ServiceSourcePin {BuildingSource owner;service_membership_reader::Local local;action_session::ServiceMembershipState state;bool operator==(const ServiceSourcePin&)const=default;};
// Supplied Buildings must come from the complete current reciprocal owner
// census. Destination resolves an actual full-UID Building through that census.
template<class Read,class Text,class Destination,class Find>
bool source_services(Read& at,Text& text,Destination& destination,Find& find,uintptr_t image,uintptr_t expected_shard,
    const std::vector<BuildingSource>& supplied,std::map<std::string,SourceBuilding>& out,std::vector<ServiceSourcePin>& pins,std::string& error){
    out.clear();pins.clear();std::set<uintptr_t> objects;std::set<uint64_t> uids;size_t units=0,text_bytes=0;
    const auto budget=[&](size_t n){constexpr size_t limit=16u*1024u*1024u;if(n>limit-text_bytes){error="Service graph source text bytes exceed limit: current="+std::to_string(text_bytes)+" next="+std::to_string(n)+" required<=16777216";return false;}text_bytes+=n;return true;};
    for(const auto& source:supplied){
        if(!source.object||!source.uid||source.key.empty()||source.shard!=expected_shard||!source.region||!objects.insert(source.object).second||!uids.insert(source.uid).second){error="Service graph source Building census has foreign or duplicate owner";return false;}
        uint64_t uid=0;uintptr_t shard=0,region=0,found=0;
        if(!at(source.object,0x18,uid,error)||!at(source.object,0x170,shard,error)||!at(source.object,0x178,region,error)||!find(source.uid,found,error))return false;
        if(uid!=source.uid||found!=source.object||shard!=source.shard||region!=source.region){error="Service graph source Building UID or reciprocal owner changed";return false;}
        ServiceSourcePin pin;pin.owner=source;
        if(!service_membership_reader::sample(at,text,destination,image,source.object,pin.state,pin.local,error))return false;
        const auto count=pin.state.rows.size()+pin.state.characters.size()+pin.state.configs.size();if(count>1000000-units){error="Service graph source rows exceed limit: current="+std::to_string(units)+" next="+std::to_string(count)+" required<=1000000";return false;}units+=count;
        if(!budget(source.key.size())||!budget(pin.state.building_definition.size())||!budget(pin.state.spawn_type.size()))return false;
        for(const auto& row:pin.state.rows)if(!budget(row.destination.size())||!budget(row.config_key.size()))return false;
        for(const auto& row:pin.state.characters)if(!budget(row.name.size()))return false;
        for(const auto& row:pin.state.configs)if(!budget(row.key.size()))return false;
        action_session::ServiceMembershipState after;service_membership_reader::Local local_after;
        if(!service_membership_reader::sample(at,text,destination,image,source.object,after,local_after,error))return false;
        if(after!=pin.state||local_after!=pin.local){error="Service graph complete source Services changed during read";return false;}
        if(!out.emplace(source.key,SourceBuilding{source.key,pin.state}).second){error="Service graph source Building canonical key duplicated";return false;}
        pins.push_back(std::move(pin));
    }
    error.clear();return true;
}
}
