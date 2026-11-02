#pragma once
#include "management_service_shape_read.h"
#include <map>
#include <set>

namespace mt2mp::game::management_service_graph {
// These are the address points written by the native constructors, not vtable symbol bases.
inline constexpr uintptr_t manager_islands_ap=0x1704bf0,manager_member_ap=0x1749040,spatial_hash_ap=0x17471e0,spatial_cell_ap=0x1748fe0;
using OwnerKeys=std::map<uintptr_t,std::string>;
inline bool owner_key(const OwnerKeys& keys,uintptr_t pointer,std::string& out,std::string& e){
    out.clear();if(!pointer)return true;const auto found=keys.find(pointer);
    if(found==keys.end()||found->second.empty())return fail(e,"Service graph source has no current canonical owner");
    out=found->second;return true;
}
// First locked pass: find the exact source Buildings before sampling their
// services outside the lock. The full graph is independently read afterward.
template<class Read>bool discover_sources(Read& at,uintptr_t image,uintptr_t manager,const OwnerKeys& buildings,std::vector<std::string>& out,std::string& e){
    constexpr size_t limit=1000000;out.clear();uintptr_t ap=0;ArrayDescriptor islands;
    if(!at(manager,0x10,ap,e))return false;if(ap!=image+manager_islands_ap)return fail(e,"Service graph discovery manager array class differs");
    if(!array_descriptor(at,manager,0x10,islands,e))return false;
    if(!array_span(islands.data,islands.count,islands.capacity,8,limit))return fail(e,"Service graph discovery island descriptor invalid");
    std::set<uintptr_t> seen_islands;std::set<std::string> seen;size_t total=0;
    for(int32_t i=0;i<islands.count;++i){uintptr_t island=0;if(!at(islands.data,size_t(i)*8,island,e))return false;
        if(!island||island>UINTPTR_MAX-0x100||!seen_islands.insert(island).second)return fail(e,"Service graph discovery Island owner invalid");
        if(!at(island,0xe0,ap,e))return false;if(ap!=image+0x17477e0)return fail(e,"Service graph discovery link array class differs");
        ArrayDescriptor links;if(!array_descriptor(at,island,0xe0,links,e))return false;
        if(!array_span(links.data,links.count,links.capacity,0x38,limit)||size_t(links.count)>limit-total)return fail(e,"Service graph discovery link descriptor invalid");total+=size_t(links.count);
        for(int32_t j=0;j<links.count;++j){const auto link=links.data+size_t(j)*0x38;uintptr_t source=0;
            if(!at(link,0,ap,e))return false;if(ap!=image+0x17477b0)return fail(e,"Service graph discovery link class differs");
            if(!at(link,0x18,source,e))return false;if(source){std::string key;if(!owner_key(buildings,source,key,e))return false;if(seen.insert(key).second)out.push_back(std::move(key));}}
        ArrayDescriptor after;uintptr_t final_island=0;if(!array_descriptor(at,island,0xe0,after,e)||!at(islands.data,size_t(i)*8,final_island,e))return false;
        if(after!=links||final_island!=island)return fail(e,"Service graph discovery descriptors changed");
    }
    ArrayDescriptor after;if(!array_descriptor(at,manager,0x10,after,e))return false;if(after!=islands)return fail(e,"Service graph discovery manager descriptor changed");return true;
}
// The caller supplies current reciprocal owner censuses, holds this manager's
// native read lock for the entire call, then verifies the owners again after
// unlock. These local addresses never enter the returned projection.
template<class Read>bool read_graph(Read& at,uintptr_t image,uintptr_t manager,const std::string& owner,const std::string& selected,
    const OwnerKeys& regions,const OwnerKeys& buildings,const OwnerKeys& paths,const OwnerKeys& gizmos,
    const std::map<std::string,SourceBuilding>& services,Graph& out,std::string& e){
    constexpr size_t limit=1000000;out={};Graph graph;graph.owner=owner;graph.selected=selected;size_t units=0;uintptr_t ap=0,lock=0;
    if(!manager||manager>UINTPTR_MAX-0x70||owner.empty())return fail(e,"Service graph manager or owner absent");
    if(!at(manager,0,lock,e))return false;if(!lock)return fail(e,"Service graph manager lock absent");
    if(!at(manager,0x10,ap,e))return false;if(ap!=image+manager_islands_ap)return fail(e,"Service graph manager island array class differs");
    if(!at(manager,0x68,ap,e))return false;if(ap!=image+manager_member_ap)return fail(e,"Service graph manager member class differs");
    if(!at(manager,0x28,ap,e))return false;if(ap!=image+spatial_hash_ap)return fail(e,"Service graph spatial hash class differs");
    ArrayDescriptor islands,cells;if(!array_descriptor(at,manager,0x10,islands,e)||!array_descriptor(at,manager,0x28,cells,e))return false;
    if(!array_span(islands.data,islands.count,islands.capacity,8,limit)||!array_span(cells.data,cells.count,cells.capacity,24,limit)||
       size_t(islands.count)+size_t(cells.count)>limit)return fail(e,"Service graph manager or cell array bounds differ");
    units=size_t(islands.count)+size_t(cells.count);
    if(!at(manager,0x40,graph.spatial_float_bits[0],e)||!at(manager,0x44,graph.spatial_float_bits[1],e)||
       !at(manager,0x4c,graph.spatial_float_bits[2],e)||!at(manager,0x50,graph.dimensions[0],e)||!at(manager,0x54,graph.dimensions[1],e))return false;
    if(!positive(graph.spatial_float_bits[0])||!finite(graph.spatial_float_bits[1])||!finite(graph.spatial_float_bits[2])||
       graph.dimensions[0]<=0||graph.dimensions[1]<=0||int64_t(graph.dimensions[0])*graph.dimensions[1]!=cells.count)return fail(e,"Service graph spatial dimensions invalid");
    std::vector<uintptr_t> pointers;std::map<uintptr_t,uint32_t> index;
    for(int32_t i=0;i<islands.count;++i){uintptr_t island=0;if(!at(islands.data,size_t(i)*8,island,e))return false;
        if(!island||island>UINTPTR_MAX-0x100||!index.emplace(island,uint32_t(i)).second)return fail(e,"Service graph island pointer is null, unbounded or duplicated");pointers.push_back(island);}
    std::set<std::string> used_services;const auto chosen=services.find(selected);
    if(selected.empty()||chosen==services.end()||chosen->second.key!=selected)return fail(e,"Service graph selected Building lacks complete service census");
    used_services.insert(selected);graph.source_buildings.push_back(chosen->second);
    for(const auto island:pointers){Island row;uintptr_t region=0;
        if(!at(island,0xf8,region,e))return false;if(!region)return fail(e,"Service graph island region absent");if(!owner_key(regions,region,row.region,e))return false;
        if(!read_shape(at,image,island,row.shape,units,e)||!read_shape(at,image,island+0x70,row.contracted,units,e))return false;
        if(!at(island,0xe0,ap,e)||ap!=image+0x17477e0)return fail(e,"Service graph Island link array class differs");
        ArrayDescriptor links;if(!array_descriptor(at,island,0xe0,links,e))return false;
        if(!array_span(links.data,links.count,links.capacity,0x38,limit)||size_t(links.count)>limit-units)return fail(e,"Service graph Island link descriptor invalid");
        units+=size_t(links.count);
        for(int32_t n=0;n<links.count;++n){const uintptr_t link=links.data+size_t(n)*0x38;Link value;uintptr_t building=0,path=0,gizmo=0;
            if(!at(link,0,ap,e))return false;if(ap!=image+0x17477b0)return fail(e,"Service graph link class differs");
            if(!at(link,0x18,building,e)||!at(link,0x20,path,e)||!at(link,0x28,gizmo,e)||!at(link,0x30,value.state,e)||!at(link,0x34,value.native_flag,e))return false;
            if(!owner_key(buildings,building,value.building,e)||!owner_key(paths,path,value.path,e)||!owner_key(gizmos,gizmo,value.gizmo,e))return false;
            if(!value.building.empty()&&used_services.insert(value.building).second){const auto found=services.find(value.building);
                if(found==services.end()||found->second.key!=value.building)return fail(e,"Service graph Building lacks complete service census");graph.source_buildings.push_back(found->second);}
            ArrayDescriptor destinations;if(!array_descriptor(at,link,0,destinations,e))return false;
            if(!array_span(destinations.data,destinations.count,destinations.capacity,16,limit)||size_t(destinations.count)>limit-units)return fail(e,"Service graph destination descriptor invalid");
            units+=size_t(destinations.count);
            for(int32_t j=0;j<destinations.count;++j){uintptr_t target=0;Destination d;
                if(!at(destinations.data,size_t(j)*16,target,e)||!at(destinations.data,size_t(j)*16+8,d.currency0,e)||!at(destinations.data,size_t(j)*16+12,d.currency1,e))return false;
                const auto found=index.find(target);if(found==index.end())return fail(e,"Service graph destination is foreign to owner manager");d.island=found->second;value.destinations.push_back(d);}
            ArrayDescriptor after;if(!array_descriptor(at,link,0,after,e))return false;if(after!=destinations)return fail(e,"Service graph destination descriptor changed");row.links.push_back(std::move(value));
        }
        ArrayDescriptor after;uintptr_t final_region=0;if(!array_descriptor(at,island,0xe0,after,e)||!at(island,0xf8,final_region,e))return false;if(after!=links||final_region!=region)return fail(e,"Service graph link descriptor or region changed");graph.islands.push_back(std::move(row));
    }
    for(int32_t i=0;i<cells.count;++i){const uintptr_t cell=cells.data+size_t(i)*24;if(!at(cell,0,ap,e))return false;if(ap!=image+spatial_cell_ap)return fail(e,"Service graph spatial cell class differs");
        ArrayDescriptor members;if(!array_descriptor(at,cell,0,members,e))return false;
        if(!array_span(members.data,members.count,members.capacity,8,limit)||size_t(members.count)>limit-units)return fail(e,"Service graph cell member descriptor invalid");units+=size_t(members.count);
        std::vector<uint32_t> row;for(int32_t j=0;j<members.count;++j){uintptr_t pointer=0;if(!at(members.data,size_t(j)*8,pointer,e))return false;
            const auto found=index.find(pointer);if(found==index.end())return fail(e,"Service graph cell member is foreign to owner manager");row.push_back(found->second);}
        ArrayDescriptor after;if(!array_descriptor(at,cell,0,after,e))return false;if(after!=members)return fail(e,"Service graph cell descriptor changed");graph.cells.push_back(std::move(row));
    }
    ArrayDescriptor islands_after,cells_after;std::array<uint32_t,3> spatial_after{};std::array<int32_t,2> dimensions_after{};uintptr_t lock_after=0;
    if(!array_descriptor(at,manager,0x10,islands_after,e)||!array_descriptor(at,manager,0x28,cells_after,e)||!at(manager,0,lock_after,e)||
       !at(manager,0x40,spatial_after[0],e)||!at(manager,0x44,spatial_after[1],e)||!at(manager,0x4c,spatial_after[2],e)||
       !at(manager,0x50,dimensions_after[0],e)||!at(manager,0x54,dimensions_after[1],e))return false;
    if(islands_after!=islands||cells_after!=cells||lock_after!=lock||spatial_after!=graph.spatial_float_bits||dimensions_after!=graph.dimensions)return fail(e,"Service graph manager descriptors or spatial header changed");
    for(size_t i=0;i<pointers.size();++i){uintptr_t pointer=0;if(!at(islands.data,i*8,pointer,e))return false;if(pointer!=pointers[i])return fail(e,"Service graph island membership changed");}
    out=std::move(graph);e.clear();return true;
}
}
