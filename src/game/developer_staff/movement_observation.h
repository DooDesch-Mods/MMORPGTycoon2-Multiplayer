#pragma once
#include "developer_staff/job_census.h"
#include "remove_diagnostics.h"
#include "region_saved_identity.h"
#include <cmath>
#include <set>

namespace mt2mp::game::developer_staff::job_census {
// Local receiver values are compared between reads and never serialized.
struct MovementOwners {
    uintptr_t shard=0,region=0,quadtree=0,current=0,destination=0,path_task=0,road=0,path_node=0,selection=0;
    uint64_t current_uid=0,destination_uid=0;
    bool operator==(const MovementOwners&)const=default;
};
inline bool observe_movement(const Read& read,const Capture& capture,const Census& census,
                             nlohmann::json& out,std::string& error) {
    out={{"observed",false}};error.clear();
    const auto fail=[&](const std::string& cause){error=cause;return false;};
    const auto at=[&]<class T>(uintptr_t object,size_t offset,T& value,std::string& cause){
        if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-sizeof(T)){
            cause="Developer movement field address overflow";return false;}
        return read(object+offset,&value,sizeof(T),cause);
    };
    if(!read||!capture.image||!capture.pin.origin.map||capture.state.rows.size()!=50||
       capture.local.developers.size()!=50||census.developers.size()!=50)
        return fail("Developer movement requires authorized fixed50 Capture and census");
    const uintptr_t map=reinterpret_cast<uintptr_t>(capture.pin.origin.map);
    if(map>UINTPTR_MAX-0x1c0)return fail("Developer movement root shard address overflow");
    const uintptr_t root=map+0x1c0;
    const auto once=[&](nlohmann::json& result,std::vector<MovementOwners>& owners){
        result=nlohmann::json::array();owners.clear();std::set<int32_t> ids;std::set<uintptr_t> receivers;
        uintptr_t selection=0;
        const auto building=[&](uintptr_t receiver,uint64_t& uid,nlohmann::json& binding){
            binding={{"present",receiver!=0}};if(!receiver)return true;
            if(!selection){uintptr_t ap=0;
                if(!at(capture.image,0x13df630,selection,error)||!at(selection,0,ap,error))return false;
                if(ap!=capture.image+0x17205d0)return fail("Developer movement SelectionManager class differs");}
            remove_diagnostics::LocalTarget target;
            if(!remove_diagnostics::resolve_local_target(selection,receiver,target,error))return false;
            const auto& origin=capture.pin.origin;
            if(!target.normal_building||target.object!=receiver||target.manager!=selection||!target.uid||
               target.record_key.empty()||target.snapshot_id.empty()||target.world_epoch!=origin.world_epoch||
               target.world_generation!=origin.generation)
                return fail("Developer movement Building binding differs from actual Origin/receiver");
            uid=target.uid;
            binding={{"present",true},{"building_key",target.record_key},{"binding_snapshot_uuid",target.snapshot_id},
                     {"native_origin_uuid",origin.uuid},{"type",target.type},{"variant",target.variant}};
            return true;
        };
        for(size_t slot=0;slot<50;++slot){
            const auto& row=capture.state.rows[slot];const auto& dev=census.developers[slot];
            const auto receiver=capture.local.developers[slot];
            if(row.slot!=int32_t(slot)||row.id<0||!receiver||dev.pointer!=receiver||dev.id!=row.id||
               !ids.insert(row.id).second||!receivers.insert(receiver).second)
                return fail("Developer movement fixed50 receiver/ID/slot differs");
            MovementOwners o;std::array<float,4> orientation{};std::array<float,3> scale{},destination{};
            int32_t current_pad=0,destination_pad=0,step_count=0,step_index=0;
            uint8_t dirty=0,moving=0,path_active=0,task_pending=0;
            if(!at(receiver,0x170,o.shard,error)||!at(receiver,0x178,o.region,error)||
               !at(receiver,0x488,o.quadtree,error)||!at(receiver,0x480,dirty,error)||
               !at(receiver,0x100,orientation,error)||!at(receiver,0x11c,scale,error)||
               !at(receiver,0x338,o.current,error)||!at(receiver,0x340,current_pad,error)||
               !at(receiver,0x348,o.destination,error)||!at(receiver,0x350,destination_pad,error)||
               !at(receiver,0x230,destination,error)||!at(receiver,0x269,moving,error)||
               !at(receiver,0x26b,path_active,error)||!at(receiver,0x308,o.path_task,error)||
               !at(receiver,0x310,task_pending,error)||!at(receiver,0x328,step_count,error)||
               !at(receiver,0x330,step_index,error)||!at(receiver,0x2f8,o.road,error)||
               !at(receiver,0x2e8,o.path_node,error))return false;
            if(dirty>1||moving>1||path_active>1||task_pending>1)
                return fail("Developer movement flags are not Boolean");
            for(const auto value:orientation)if(!std::isfinite(value))return fail("Developer movement orientation is not finite");
            for(const auto value:scale)if(!std::isfinite(value))return fail("Developer movement scale is not finite");
            for(const auto value:destination)if(!std::isfinite(value))return fail("Developer movement destination is not finite");
            nlohmann::json region={{"present",o.region!=0}},current,dest;
            if(o.region){int32_t id=-1;uintptr_t selected=0;
                if(!at(o.region,0x834,id,error)||
                   !region_saved_identity::resolve(capture.image,map,root,id,o.region,selected,at,error))return false;
                region["saved_id"]=id;
            }
            if(!building(o.current,o.current_uid,current)||!building(o.destination,o.destination_uid,dest))return false;
            o.selection=selection;
            nlohmann::json job_region=nullptr;
            if(dev.job){
                if(dev.job_index<0||size_t(dev.job_index)>=census.jobs.size()||
                   census.jobs[size_t(dev.job_index)].pointer!=dev.job)
                    return fail("Developer movement Job receiver is absent from census");
                job_region=o.region==census.jobs[size_t(dev.job_index)].region;
            }
            result.push_back({{"id",row.id},{"slot",slot},{"shard_present",o.shard!=0},
                {"shard_is_root",o.shard==root},{"region",std::move(region)},{"region_matches_job",std::move(job_region)},
                {"quadtree_present",o.quadtree!=0},{"quadtree_dirty",dirty!=0},
                {"orientation",orientation},{"scale",scale},{"current_building",std::move(current)},
                {"current_pad",current_pad},{"destination_building",std::move(dest)},{"destination_pad",destination_pad},
                {"movement_destination",destination},{"has_destination",moving!=0},{"path_active",path_active!=0},
                {"path_task_present",o.path_task!=0},{"path_task_pending",task_pending!=0},
                {"travel_step_count",step_count},{"travel_step_index",step_index},
                {"road_present",o.road!=0},{"path_node_present",o.path_node!=0}});
            owners.push_back(o);
        }
        return true;
    };
    nlohmann::json first,second;std::vector<MovementOwners> before,after;
    if(!once(first,before)||!once(second,after))return false;
    if(first!=second||before!=after)return fail("Developer movement fields or local owners changed between reads");
    out={{"observed",true},{"actors",std::move(first)},{"movement_graph_identity_proven",false}};
    return true;
}
}
