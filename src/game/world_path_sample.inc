namespace world_path_runtime {
namespace mixed=mixed_path_state;
namespace identity=mixed_path_identity;
using WorldState=action::WorldPathState;
struct Sample {
    mixed::Snapshot mixed;
    WorldState wire;
    json effects;
};
struct Owner {
    identity::Registry keys;
    std::string snapshot;
};
Owner owner;
action::WorldPathIntent portable(const wall_pass_cursor::WallIntent& source,const std::string& before){
    action::WorldPathIntent out;
    out.kind=action::WorldPathKind::Wall;out.start_region=out.end_region=source.region;
    out.type=source.type;out.node_type=source.node_type;out.name=source.name;
    out.start_node=source.start_node;out.end_node=source.end_node;
    out.start_split=source.start_split;out.end_split=source.end_split;
    out.start=source.start;out.end=source.release;out.quoted_length=source.quoted_length;
    out.segments=source.segments;out.preview=source.preview;out.preview_valid=source.preview_valid;
    out.graph_before_sha256=before;return out;
}
action::WorldPathIntent portable(const wall_pass_cursor::PassIntent& source,const std::string& before){
    action::WorldPathIntent out;
    out.kind=action::WorldPathKind::Pass;out.start_region=source.start_region;out.end_region=source.end_region;
    out.type=source.type;out.name=source.name;out.start_node=source.start_node;out.end_node=source.end_node;
    out.start=source.start;out.end=source.end;out.preview=source.preview;out.preview_valid=source.preview_valid;
    out.graph_before_sha256=before;return out;
}
bool world_path_sample(region::Runtime& r,const std::string& snapshot,int32_t start,int32_t end,
                       Sample& out,std::string& error){
    if(start<0||end<0||snapshot.empty())return fail(error,"Wall/Pass sample Region or snapshot absent");
    region::LocalView first;
    if(!region::identity(r,start,0,first,error))return false;
    uintptr_t map=0;
    if(!region::at(r,r.image,0x13df8c0,map,error))return false;
    mixed_path_native::Census census;
    if(!mixed_path_native::read_world(r.memory,r.image,map,first.world.root,census,error))return false;
    if(owner.snapshot.empty()){
        if(!owner.keys.seed(snapshot,census,error))return false;
        owner.snapshot=snapshot;
    }
    if(owner.snapshot!=snapshot)return fail(error,"Wall/Pass identity World changed without owner retirement");
    Sample value;value.mixed.native=std::move(census);
    if(!mixed_path_graph::build(value.mixed.native,owner.keys.keys(),value.mixed.graph,error)||
       !mixed_path_records::read_all(r.memory,r.image,value.mixed.native,owner.keys.keys(),value.mixed.records,error)||
       !mixed::digest(value.mixed,value.mixed,value.wire.complete_sha256,error))return false;
    value.wire.paths=uint32_t(value.mixed.graph.paths.size());
    value.wire.nodes=uint32_t(value.mixed.graph.nodes.size());
    json regions=json::array();
    for(int32_t id:std::set<int32_t>{start,end}){
        region::LocalView view;action::TerrainState terrain;uintptr_t global=0;
        if(!region::read(r,id,0,view,error)||
           !terrain::terrain_state(r,view,terrain,global,error))return false;
        regions.push_back({region::state_json(view.state),terrain.global_random,
            terrain.release_edits,terrain.contours_sha256,terrain.bugs_sha256,
            terrain.bug_count,terrain.heights_sha256,terrain.height_cell,terrain.height_bits});
    }
    value.effects=std::move(regions);
    if(!region::sha256(value.effects.dump(),value.wire.world_effect_sha256,error))return false;
    out=std::move(value);return true;
}
bool world_path_births(const std::string& action_id,const Sample& before,const mixed_path_native::Census& after,
                       std::vector<identity::Birth>& out,std::string& error){
    return owner.keys.births(action_id,before.mixed.native,after,out,error);
}
}
