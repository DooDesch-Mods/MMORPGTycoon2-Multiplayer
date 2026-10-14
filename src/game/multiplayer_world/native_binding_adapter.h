#pragma once
#include "world_state.h"
#include "mt2mp/game/native_engine.h"
#include "remove_diagnostics.h"

namespace mt2mp::world_state {
inline Building building_state(const game::native::BuildingState& value){
    Building result;result.type=value.type;result.variant=value.variant;
    result.x=value.prop.x;result.y=value.prop.y;result.z=value.prop.z;
    result.rotation=value.rotation;result.scale=value.scale;result.alive=true;
    return result;
}
// Resolve the Root target and read native BuildingState immediately before this projection.
inline bool bind_root_readback(const Origin& origin,const std::string& shared_id,
    const game::remove_diagnostics::LocalTarget& target,const game::native::BuildingState& readback,
    LocalBinding& result,std::string& error){
    result={};
    if(!valid_object_key(shared_id)||target.snapshot_id!=origin.snapshot_uuid||target.record_key!=shared_id||
       target.world_epoch!=origin.world_epoch||target.world_generation!=origin.generation||
       !target.uid||target.uid!=readback.prop.uid||target.type!=readback.type||target.variant!=readback.variant){
        error="world_root_target_and_native_readback_binding_differs";return false;
    }
    result={origin,shared_id,target.uid,building_state(readback)};error.clear();return true;
}
}
