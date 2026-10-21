}
namespace ui_move_runtime::building_clearance {
struct Local {uint64_t calls_before=0;bool restoring=false,finished=false;};
struct Snapshot {std::shared_ptr<Local> local;};
std::vector<std::string> fixture_events;
std::string fixture_failure;
const std::string fixture_scenery_key="action:33333333-3333-4333-8333-333333333333";
action::BuildingState fixture_scenery_state;
bool prepare_history(const action::Binding& binding,const action::Request& request,const Snapshot& source,Snapshot& before,std::string& error,const action::Evidence*){
    fixture_events.push_back("prepare");
    if(!source.local||binding!=action::Binding{2,1}||request.snapshot_uuid!=snapshot||request.target_action_uuid!=fixture_placed||
       (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding))throw std::runtime_error("Actual history clearance preflight arguments differ");
    if(fixture_failure=="prepare"){error="original clearance preflight failed";return false;}
    before.local=std::make_shared<Local>();before.local->calls_before=uint64_t(calls);
    before.local->restoring=request.action==action::Action::UndoBuilding;return true;
}
bool current_bindings(const Snapshot& before,const world_state::Origin& origin,std::vector<world_state::LocalBinding>& out,std::string&){
    if(!before.local||origin.snapshot_uuid!=snapshot)throw std::runtime_error("Actual history current projection identity differs");
    out.clear();if(!before.local->restoring)out.push_back({origin,fixture_scenery_key,8312,fixture_scenery_state});return true;
}
bool settle(const action::Binding& binding,const action::Request& request,const Snapshot& before,const Snapshot* source,uint64_t uid,uintptr_t graph,Snapshot& after,std::string& error){
    fixture_events.push_back("settle");
    if(binding!=action::Binding{2,1}||!before.local||!source||!source->local||uid!=7328||graph!=99||
       uint64_t(calls)!=before.local->calls_before+1)throw std::runtime_error("Actual history clearance settlement did not follow one original graph execution");
    if(fixture_failure=="settle"){error="original clearance graph failed";return false;}
    after.local=std::make_shared<Local>();after.local->restoring=request.action==action::Action::UndoBuilding;return true;
}
bool finish(const action::Binding&,const action::Request&,const Snapshot&,Snapshot& after,const action::Evidence*,std::string& error){
    fixture_events.push_back("finish");
    if(!after.local)throw std::runtime_error("Actual history cleared its settlement receipt");
    if(fixture_failure=="finish"){error="original clearance Record failed";return false;}
    after.local->finished=true;return true;
}
bool publication(const Snapshot& after,const world_state::Origin& origin,std::vector<std::string>& retired,std::vector<world_state::LocalBinding>& restored,std::string& error){
    fixture_events.push_back("publication");retired.clear();restored.clear();
    if(!after.local||!after.local->finished||origin.snapshot_uuid!=snapshot)throw std::runtime_error("Actual history published before complete current Record proof");
    if(fixture_failure=="publication"){error="original clearance publication failed";return false;}
    if(after.local->restoring)restored.push_back({origin,fixture_scenery_key,8312,fixture_scenery_state});
    else retired.push_back(fixture_scenery_key);return true;
}
}
namespace {
