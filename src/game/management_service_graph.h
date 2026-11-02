#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
#include "typed_action/service_membership_action.h"

namespace mt2mp::game::management_service_graph {
// Island indices exist only in one exhaustive snapshot. They are not action IDs.
struct Ring { std::vector<std::array<int64_t,2>> points; bool hole=false;bool operator==(const Ring&)const=default; };
struct Shape {
    bool internal_present=false;
    bool bounds_valid=false;
    std::array<uint32_t,6> bounds{};
    std::vector<Ring> rings;
    std::vector<std::vector<std::array<uint32_t,3>>> sections;
    bool operator==(const Shape&)const=default;
};
struct Destination { uint32_t island=0; int32_t currency0=0,currency1=0;bool operator==(const Destination&)const=default; };
struct Link {
    std::string building,path,gizmo;
    int32_t state=0;
    uint8_t native_flag=0;
    std::vector<Destination> destinations;
    bool operator==(const Link&)const=default;
};
struct Island { std::string region; Shape shape,contracted; std::vector<Link> links;bool operator==(const Island&)const=default; };
struct SourceBuilding { std::string key;action_session::ServiceMembershipState services; };
struct Graph {
    std::string owner,selected;
    std::array<uint32_t,3> spatial_float_bits{};
    std::array<int32_t,2> dimensions{};
    std::vector<Island> islands;
    std::vector<std::vector<uint32_t>> cells;
    std::vector<SourceBuilding> source_buildings;
};
inline bool finite(uint32_t bits){float value;std::memcpy(&value,&bits,sizeof value);return std::isfinite(value);}
inline bool positive(uint32_t bits){float value;std::memcpy(&value,&bits,sizeof value);return std::isfinite(value)&&value>0;}
inline bool fail(std::string& error,const char* why){error=why;return false;}
inline bool shape_json(const Shape& shape,nlohmann::json& out,size_t& units,std::string& error){
    constexpr size_t limit=1000000;
    nlohmann::json rings=nlohmann::json::array(),sections=nlohmann::json::array();
    if(shape.rings.size()>limit-units)return fail(error,"Service graph ring count exceeds bounded receipt");
    units+=shape.rings.size();
    if(shape.sections.size()>limit-units)return fail(error,"Service graph section count exceeds bounded receipt");
    units+=shape.sections.size();
    for(auto word:shape.bounds)if(!finite(word))return fail(error,"Service graph non-finite bounds");
    for(const auto& ring:shape.rings){
        if(ring.points.size()>limit-units)return fail(error,"Service graph geometry exceeds bounded receipt");
        units+=ring.points.size();rings.push_back({ring.hole,ring.points});
    }
    for(const auto& section:shape.sections){
        if(section.size()>limit-units)return fail(error,"Service graph sections exceed bounded receipt");
        units+=section.size();for(const auto& point:section)for(auto word:point)if(!finite(word))return fail(error,"Service graph non-finite vertex");
        sections.push_back(section);
    }
    if(!shape.internal_present&&!shape.rings.empty())return fail(error,"Service graph absent internal shape has rings");
    out=nlohmann::json::array({shape.internal_present,shape.bounds_valid,shape.bounds,std::move(rings),std::move(sections)});return true;
}
inline bool projection(const Graph& graph,std::string& out,std::string& error){
    constexpr size_t limit=1000000;out.clear();error.clear();size_t units=0,text_bytes=0;
    const auto budget=[&](const std::string& value){constexpr size_t maximum=16u*1024u*1024u;if(value.size()>maximum-text_bytes)return false;text_bytes+=value.size();return true;};
    if(graph.owner.empty()||graph.owner.size()>512||graph.selected.empty()||graph.selected.size()>512||graph.islands.size()>limit||graph.cells.size()>limit||
       graph.dimensions[0]<=0||graph.dimensions[1]<=0||uint64_t(graph.dimensions[0])*uint64_t(graph.dimensions[1])!=graph.cells.size())
        return fail(error,"Service graph owner or spatial dimensions invalid");
    if(graph.cells.size()>limit-graph.islands.size())return fail(error,"Service graph island and cell count exceeds bounded receipt");
    if(!budget(graph.owner)||!budget(graph.selected))return fail(error,"Service graph text budget exceeds 16777216 bytes");
    units=graph.islands.size()+graph.cells.size();
    for(auto word:graph.spatial_float_bits)if(!finite(word))return fail(error,"Service graph non-finite spatial header");
    if(!positive(graph.spatial_float_bits[0]))return fail(error,"Service graph spatial cell size is not positive");
    nlohmann::json islands=nlohmann::json::array(),cells=nlohmann::json::array(),sources=nlohmann::json::array();std::vector<std::string> expected_sources{graph.selected};std::set<std::string> encountered_sources{graph.selected};
    for(const auto& island:graph.islands){
        if(island.region.empty()||island.region.size()>512||!budget(island.region)||island.links.size()>limit-units)return fail(error,"Service graph region, text budget or link count invalid");
        units+=island.links.size();nlohmann::json original,contracted,links=nlohmann::json::array();
        if(!shape_json(island.shape,original,units,error)||!shape_json(island.contracted,contracted,units,error))return false;
        for(const auto& link:island.links){
            if(link.building.size()>512||link.path.size()>512||link.gizmo.size()>512||!budget(link.building)||!budget(link.path)||!budget(link.gizmo)||link.destinations.size()>limit-units)return fail(error,"Service graph link owner, text budget or destination bound invalid");
            units+=link.destinations.size();nlohmann::json destinations=nlohmann::json::array();
            if(!link.building.empty()&&encountered_sources.insert(link.building).second)expected_sources.push_back(link.building);
            for(const auto& destination:link.destinations){if(destination.island>=graph.islands.size())return fail(error,"Service graph destination outside island table");destinations.push_back({destination.island,destination.currency0,destination.currency1});}
            links.push_back({link.building,link.path,link.gizmo,link.state,link.native_flag,std::move(destinations)});
        }
        islands.push_back({island.region,std::move(original),std::move(contracted),std::move(links)});
    }
    for(const auto& cell:graph.cells){if(cell.size()>limit-units)return fail(error,"Service graph cell membership exceeds bounded receipt");units+=cell.size();
        for(auto island:cell)if(island>=graph.islands.size())return fail(error,"Service graph cell references foreign island");cells.push_back(cell);}
    if(graph.source_buildings.size()!=expected_sources.size()||graph.source_buildings.size()>limit-units)return fail(error,"Service graph source Building census is incomplete");
    units+=graph.source_buildings.size();std::set<std::string> source_keys;size_t source_index=0;
    for(const auto& source:graph.source_buildings){
        const auto& state=source.services;
        if(source.key.empty()||source.key.size()>512||!source_keys.insert(source.key).second||state.rows.size()>1024||state.characters.size()>10000||state.configs.size()>4096||
           state.rows.size()+state.characters.size()+state.configs.size()>limit-units||(!state.config_present&&!state.configs.empty())||state.spawn_type.empty()||
           state.spawn_type.size()>4096||state.building_definition.size()>4096||!budget(source.key)||!budget(state.spawn_type)||!budget(state.building_definition))return fail(error,"Service graph source Building census or text budget invalid");
        if(source.key!=expected_sources[source_index++])return fail(error,"Service graph source Building order or provenance differs");
        units+=state.rows.size()+state.characters.size()+state.configs.size();nlohmann::json rows=nlohmann::json::array(),characters=nlohmann::json::array(),configs=nlohmann::json::array();
        for(const auto& service:state.rows){
            if(service.type<0||service.type>16||service.character< -1||(service.character>=0&&size_t(service.character)>=state.characters.size())||
               service.config_key.size()>256||service.destination.size()>512||!budget(service.config_key)||!budget(service.destination)||service.counts[0].history.size()>10||service.counts[1].history.size()>10)return fail(error,"Service graph source service binding or text budget invalid");
            if(service.config&&std::none_of(state.configs.begin(),state.configs.end(),[&](const auto& config){return config.key==service.config_key;}))return fail(error,"Service graph source config binding absent");
            nlohmann::json counts=nlohmann::json::array();for(const auto& c:service.counts)counts.push_back({c.today,c.total,c.highest,c.history,c.warning});
            rows.push_back({service.type,service.character,service.index,service.destination,service.config_key,service.price,service.flags,service.prototype,service.config,std::move(counts)});
        }
        for(size_t i=0;i<state.characters.size();++i){const auto& c=state.characters[i];if(c.id!=int32_t(i)||c.name.size()>4096||!budget(c.name))return fail(error,"Service graph source Character or text budget invalid");characters.push_back({c.id,c.name,c.deleted});}
        for(const auto& c:state.configs){if(c.key.size()>4096||!budget(c.key))return fail(error,"Service graph source config or text budget invalid");configs.push_back({c.key,c.price});}
        sources.push_back({source.key,state.building_definition,state.spawn_type,std::move(rows),std::move(characters),state.config_present,std::move(configs)});
    }
    try{out=nlohmann::json::array({"mt2mp-service-price-graph-v1",graph.owner,graph.selected,graph.spatial_float_bits,graph.dimensions,std::move(islands),std::move(cells),std::move(sources)}).dump();}
    catch(const std::exception& failure){error=std::string("Service graph canonical serialization failed: ")+failure.what();return false;}return true;
}
}
