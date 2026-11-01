#include "snapshot_construction.h"
#include "terrain_saved_height_grid.h"
#include "live_host_state/memory_query_timing.h"

namespace mt2mp::game::snapshot_construction {
namespace region=ui_region_runtime;
namespace host=ui_region_runtime::host;
namespace terrain=ui_terrain_runtime;
namespace paths=ui_path_runtime::world_path_runtime;
using Json=nlohmann::json;
template<class Values>Json bits(const Values& values){
    Json out=Json::array();for(float value:values)out.push_back(std::bit_cast<uint32_t>(value));return out;
}
bool saved_height_hash(region::Runtime& r,uintptr_t receiver,std::string& digest,std::string& error){
    terrain_height::SavedGrid grid;
    if(!terrain_height::read_saved_bits(r.image,receiver,grid,
        [&](uintptr_t base,size_t offset,auto& value,std::string& cause){return region::at(r,base,offset,value,cause);},error))return false;
    return region::sha256(Json{{"width",grid.width},{"height",grid.height},
        {"cell_size_bits",std::bit_cast<uint32_t>(grid.cell_size)},
        {"origin_bits",bits(grid.origin)},{"height_bits",grid.samples}}.dump(),digest,error);
}
bool read(uintptr_t image,uintptr_t map,const region_saved_identity::Index& saved,Json& out,std::string& error){
    out=Json();auto* r=terrain::runtime(error);if(!r)return false;
    const auto reject=[&](const std::string& cause){error="Snapshot construction: "+cause;return false;};
    const auto snapshot=net::current_snapshot_id();
    host::WorldState world;
    if(r->image!=image||snapshot.empty())return reject("image or snapshot identity differs");
    if(!region::world(*r,world,error))return false;
    if(world.root!=map+0x1c0||uintptr_t(world.origin.map)!=map)return reject("Map or root owner differs");
    const auto field=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return region::at(*r,p,offset,value,cause);};
    const auto current=[&]{region_saved_identity::Index actual;
        if(!region_saved_identity::capture(image,map,world.root,actual,field,error))return false;
        std::sort(actual.saved.begin(),actual.saved.end());
        if(actual!=saved)return reject("saved Region owner/index changed");
        host::WorldState after;if(!region::world(*r,after,error))return false;
        return (host::same_world(world.origin,after.origin)&&world.root==after.root&&
            snapshot==net::current_snapshot_id())||reject("world or session changed during read");
    };
    if(!current())return false;
    Json regions=Json::array();std::map<uint64_t,int32_t> buildings;
    for(const auto& [id,receiver]:saved.saved){
        region::LocalView view;action_session::RegionPropertyState property;
        action_session::RegionAppearance appearance;
        std::string height_hash,contour_hash;
        {
            // Reuse page metadata only across these pure memory readers. The
            // original contour getter can allocate, so it runs after this scope.
            live_host_state::apply_timing::PageReadScope fields;
            if(!region::property_read(*r,id,receiver,property,view,error)||
               !region::appearance_read_native(*r,receiver,appearance,error)||
               !saved_height_hash(*r,receiver,height_hash,error)){
                error="Snapshot construction Region id="+std::to_string(id)+" owned="+
                    std::to_string(property.owned)+" type="+std::to_string(property.type)+": "+error;
                return false;
            }
        }
        if(!terrain::contour_hash(*r,receiver,contour_hash,error)||
           !region::property_buildings(*r,view,error))return false;
        for(auto uid:view.building_uids)if(!buildings.emplace(uid,id).second)
            return reject("Building is present in more than one saved Region");
        auto row=region::state_json(view.state);row.erase("random");
        row["name"]=property.name;row["type"]=property.type;row["type_random"]=property.type_random;
        row["level"]=property.level;row["max_level"]=property.max_level;row["owned"]=property.owned;
        row["status"]=property.status;row["heights_sha256"]=height_hash;row["contours_sha256"]=contour_hash;
        row["appearance"]={{"tint",bits(appearance.tint)},{"sky",bits(appearance.sky)},
            {"light",bits(appearance.light)},{"ambient",bits(appearance.ambient)},
            {"particles_enabled",appearance.particles_enabled},{"particles",appearance.particles},
            {"particle_color",bits(appearance.particle_color)}};
        regions.push_back(std::move(row));
    }
    std::vector<uint64_t> uids;uids.reserve(buildings.size());for(const auto& [uid,id]:buildings)uids.push_back(uid);
    std::vector<remove_diagnostics::LocalTarget> targets;
    if(!remove_diagnostics::snapshot_building_targets(uids,targets,error))return false;
    if(targets.size()!=uids.size())return reject("Building identity census is incomplete");
    Json building_rows=Json::array();std::set<std::string> keys;std::set<uint64_t> seen;
    for(const auto& target:targets){
        const auto found=buildings.find(target.uid);
        if(found==buildings.end()||!seen.insert(target.uid).second||target.record_key.empty()||
           target.snapshot_id!=snapshot||!keys.insert(target.record_key).second||
           target.world_epoch!=world.origin.world_epoch||target.world_generation!=world.origin.generation)
            return reject("Building canonical identity is absent, duplicated or belongs to another world");
        native::BuildingState value;if(!native::read_building(target.uid,value,error))return false;
        uintptr_t actual=0;const auto lookup=region::property_registered_building(image,target.uid,actual);
        if(lookup||actual!=target.object||value.prop.uid!=target.uid||value.type!=target.type||value.variant!=target.variant)
            return reject("Building typed receiver or definition changed during read");
        building_rows.push_back({{"key",target.record_key},{"region",found->second},{"type",value.type},{"variant",value.variant},
            {"position",bits(std::array<float,3>{value.prop.x,value.prop.y,value.prop.z})},
            {"rotation",bits(value.rotation)},{"scale",bits(value.scale)}});
    }
    std::sort(building_rows.begin(),building_rows.end(),[](const Json& a,const Json& b){return a.at("key")<b.at("key");});
    mixed_path_native::Census census;mixed_path_graph::Snapshot graph;
    if(!mixed_path_native::read_world(r->memory,image,map,world.root,census,error))return false;
    // No path command has established an owner yet: derive initial keys locally,
    // without changing the command registry during this observation.
    mixed_path_identity::Registry initial;
    const std::map<uint64_t,std::string>* path_keys=nullptr;
    if(paths::owner.snapshot.empty()){
        if(!initial.seed(snapshot,census,error))return false;path_keys=&initial.keys();
    }else{
        if(paths::owner.snapshot!=snapshot)return reject("Path registry belongs to another snapshot");
        path_keys=&paths::owner.keys.keys();
    }
    if(!mixed_path_graph::build(census,*path_keys,graph,error))return false;
    // Native history can reorder buckets. Canonical identities own comparison.
    for(const auto* name:{"paths","nodes","remote"})std::sort(graph.rows.at(name).begin(),graph.rows.at(name).end());
    if(!current())return false;
    out={{"schema","mt2-static-construction-v2"},{"regions",std::move(regions)},
         {"root_buildings",std::move(building_rows)},{"path_graph",std::move(graph.rows)}};
    error.clear();return true;
}
}
