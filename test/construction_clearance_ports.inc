namespace clearance_placement_fixture {
std::vector<std::string> events;
std::string failure;
bool with_scenery=false;
const std::string scenery_key="action:aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
action::BuildingState scenery;
void clear(){events.clear();failure.clear();with_scenery=false;scenery={};scenery.type="cactus";
    scenery.variant="p_ext_treecactus_01.vmb";scenery.alive=true;}
bool stage(const char* name,std::string& error){events.push_back(name);
    if(failure==name){error=std::string("original placement clearance ")+name+" failure";return false;}return true;
}
}
namespace mt2mp::game::ui_move_runtime::building_clearance {
struct Local {
    action::Binding binding;action::Request request;
    unsigned original_before=0,construction_before=0;
    uint64_t uid=0;uintptr_t graph=0;bool finished=false;
};
bool capture(const action::Binding& binding,const action::Request& request,Snapshot& out,std::string& error,const action::Evidence*){
    out={};
    fixture::need(binding==fixture::binding&&request==fixture::request&&request.action==action::Action::PlaceBuilding,
        "clearance capture received a foreign original placement");
    if(fixture::actual_scenery)return true;
    if(!clearance_placement_fixture::stage("capture",error))return false;
    out.local=std::make_shared<Local>();out.local->binding=binding;out.local->request=request;
    out.local->original_before=fixture::places;out.local->construction_before=fixture::finishes;return true;
}
bool current_bindings(const Snapshot& before,const world_state::Origin& origin,std::vector<world_state::LocalBinding>& out,std::string& error){
    out.clear();fixture::need(bool(before.local)&&origin==world_state::Origin{},"clearance current binding lost exact fixture World");
    if(!clearance_placement_fixture::stage("current",error))return false;
    if(clearance_placement_fixture::with_scenery)out.push_back({origin,clearance_placement_fixture::scenery_key,700,clearance_placement_fixture::scenery});
    return true;
}
bool settle(const action::Binding& binding,const action::Request& request,const Snapshot& before,const Snapshot* source,
            uint64_t uid,uintptr_t graph,Snapshot& after,std::string& error){
    fixture::need(before.local&&binding==before.local->binding&&request==before.local->request&&!source&&uid==500&&graph==600&&
        fixture::places==before.local->original_before+1,
        "clearance settlement lost original-once/exact native UID/validated Undo graph");
    after={};if(!clearance_placement_fixture::stage("settle",error))return false;
    after.local=std::make_shared<Local>(*before.local);after.local->uid=uid;after.local->graph=graph;return true;
}
bool finish(const action::Binding& binding,const action::Request& request,const Snapshot& before,Snapshot& after,const action::Evidence*,std::string& error){
    fixture::need(before.local&&after.local&&after.local->binding==binding&&after.local->request==request&&
        fixture::places==before.local->original_before+1&&fixture::finishes==before.local->construction_before+1&&
        after.local->uid==500&&after.local->graph==600,
        "clearance Record finish preceded exact construction Job proof or lost native receipt");
    if(!clearance_placement_fixture::stage("finish",error))return false;after.local->finished=true;return true;
}
bool publication(const Snapshot& after,const world_state::Origin& origin,std::vector<std::string>& retired,
                 std::vector<world_state::LocalBinding>& restored,std::string& error){
    retired.clear();restored.clear();fixture::need(after.local&&after.local->finished&&origin==world_state::Origin{},
        "clearance World publication preceded completed original Record proof");
    if(!clearance_placement_fixture::stage("publication",error))return false;
    if(clearance_placement_fixture::with_scenery)retired.push_back(clearance_placement_fixture::scenery_key);
    return true;
}
}
namespace ui_move_runtime {namespace building_clearance=mt2mp::game::ui_move_runtime::building_clearance;}
