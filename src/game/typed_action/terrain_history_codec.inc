bool terrain_history_effect(const Request& r,const Evidence& e){
    const auto& before=e.region_before;const auto& after=e.region_after;
    const auto& terrain_before=e.terrain_before;const auto& terrain_after=e.terrain_after;
    if(!valid_region(before)||!valid_region(after)||before==RegionState{}||after==RegionState{}||
       before.id!=after.id||before.preset!=after.preset||before.styles!=after.styles||
       !valid_terrain_state(terrain_before)||!valid_terrain_state(terrain_after)||
       !digest(terrain_before.contours_sha256)||!digest(terrain_after.contours_sha256)||
       !digest(e.region_retired_sha256)||terrain_before.release_edits!=terrain_after.release_edits||
       terrain_before.heights_sha256!=terrain_after.heights_sha256||terrain_before.height_cell!=terrain_after.height_cell||
       terrain_before.height_bits!=terrain_after.height_bits||terrain_before.height_cell!=-1||
       e.before!=e.after||!e.undo_confirmed||e.selection_confirmed||e.target_retired||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    if(r.action==Action::UndoBuilding?after.scenery_count<before.scenery_count:after.scenery_count>before.scenery_count)return false;
    if(before.scenery_count==after.scenery_count&&
       (before.scenery_sha256!=after.scenery_sha256||before.random!=after.random||
        terrain_before.global_random!=terrain_after.global_random||terrain_before.bugs_sha256!=terrain_after.bugs_sha256||
        terrain_before.bug_count!=terrain_after.bug_count))return false;
    return true;
}
