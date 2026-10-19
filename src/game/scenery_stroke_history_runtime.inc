namespace scenery_stroke {
namespace {
struct History {
    action::Binding binding;action::Request request;action::Evidence evidence;Sample after;
    uintptr_t graph=0,inverse=0;bool attempted=false,returned=false,confirmed=false,retired=false,selection_restored=false;
    std::map<uint64_t,Saved> inverse_make;std::set<uint64_t> inverse_destroy;
    std::vector<action::DuplicateCash> inverse_cash;std::string cause,cleanup_cause;
};
std::map<std::string,History> history_operations;
std::map<std::string,History> history_quarantined;
void retire_histories(){
    for(auto it=history_operations.begin();it!=history_operations.end();){
        if(it->second.attempted&&!it->second.confirmed){trim_sample(it->second.after);auto node=history_operations.extract(it++);history_quarantined.insert(std::move(node));}
        else it=history_operations.erase(it);
    }
}
json history_status(){
    json rows=json::object();for(const auto& [id,op]:history_operations)rows[id]={{"source_action_uuid",op.request.target_action_uuid},{"graph",op.graph},{"inverse",op.inverse},
        {"attempted",op.attempted},{"returned",op.returned},{"selection_restored",op.selection_restored},{"confirmed",op.confirmed},{"retired",op.retired},
        {"cause",op.cause},{"cleanup_cause",op.cleanup_cause}};
    return {{"operations",std::move(rows)},{"quarantined",history_quarantined.size()}};
}
bool history_request(const action::Request& request){return (request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&
    request.canonical_record_key=="scenery-stroke:"+request.target_action_uuid;}
bool source_for(const action::Binding& binding,const action::Request& request,Port*& out,std::string& e){
    const auto found=operations.find(request.target_action_uuid);
    if(!history_request(request)||found==operations.end()||found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||
       !found->second.confirmed||!found->second.retired||!found->second.port)return q::fail(e,"Scenery history lacks its exact confirmed retired source stroke");
    out=found->second.port.get();return true;
}
bool history_graph(Runtime& r,Port& source,bool undo,uintptr_t graph,bool original,std::map<uint64_t,Saved>& stored,
                   duplicate_history::Graph& out,std::string& e){
    const auto& makes=undo?source.removed:source.births;const auto& destroys=undo?source.births:source.removed;
    std::set<uint64_t> ids;for(const auto& [uid,saved]:destroys){(void)saved;ids.insert(uid);}
    return scenery_stroke_history::graph(r.memory,r.image,graph,makes,ids,undo?source.cash:scenery_stroke_history::opposite(source.cash),original,stored,out,e);
}
bool expected_live(Port& source,bool undo,const Sample& before,std::string& e){
    const auto& live=undo?source.evidence.scenery_stroke_result->created:source.evidence.scenery_stroke_result->removed;
    const auto& absent=undo?source.evidence.scenery_stroke_result->removed:source.evidence.scenery_stroke_result->created;
    for(const auto& row:live){const auto actual=before.props.objects.find(row.key);
        if(actual==before.props.objects.end()||actual->second.family!=Family::Scenery||actual->second.region!=row.region||actual->second.pose!=row.pose)
            return q::fail(e,"Scenery history current original full UID/pose domain differs key="+row.key);}
    for(const auto& row:absent)if(before.props.objects.contains(row.key))return q::fail(e,"Scenery history expected retired canonical alias is live key="+row.key);
    return true;
}
}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;Port* source=nullptr;
    if(!r||!installed||!r->ready||r->thread!=GetCurrentThreadId()||request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       !source_for(binding,request,source,e)||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)){
        if(e.empty())e="Scenery history exact pending owner/role is unavailable";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){e="Scenery history already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_scenery_stroke_effect(request,*authority)){e="Scenery history Host evidence is invalid";return action::Outcome::RejectedNoEffect;}
    const bool undo=request.action==action::Action::UndoBuilding;Sample before;uintptr_t graph=0;
    duplicate_history::Graph checked;std::map<uint64_t,Saved> stored;
    if(!verify_routes(*r,true,e)||!sample(*r,binding,source->request.scenery_stroke.region,source->aliases,before,e)||!expected_live(*source,undo,before,e)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e)||
       !history_graph(*r,*source,undo,graph,!source->history_started,stored,checked,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(!content_equal(before.state,*authority->scenery_stroke_before)||before.props.money!=authority->before)){
        e="Scenery history Client complete objects/bugs/Release/Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    for(const auto& [uid,saved]:stored){(void)uid;std::array<uint32_t,3> position{};std::memcpy(position.data(),saved.transform.data()+0x10,sizeof position);uintptr_t actual=0;
        if(!fault(duplicate_calls::region(r->image,saved.root,true,position,actual),"history Make RegionNear",e)||actual!=before.region)
            return action::Outcome::RejectedNoEffect;}
    std::vector<duplicate_native::Handle> selection;std::array<uint8_t,9> filter{};uintptr_t camera=0;
    if(!duplicate_edit::history_local_selection(*r,before.props,selection,filter,camera,e))return action::Outcome::RejectedNoEffect;
    duplicate_edit::HistorySource known;for(const auto& [uid,key]:source->aliases){(void)key;known.known.emplace(uid,duplicate_history::Known{});}
    auto& op=history_operations.emplace(request.action_uuid,History{binding,request}).first->second;
    op.graph=graph;op.evidence.before=before.props.money;op.evidence.scenery_stroke_before=authority?authority->scenery_stroke_before:std::optional{before.state};
    op.evidence.duplicate_cash=checked.cash;
    std::sort(op.evidence.duplicate_cash.begin(),op.evidence.duplicate_cash.end(),[](const auto& a,const auto& b){return std::tie(a.amount,a.position)<std::tie(b.amount,b.position);});
    auto unknown=[&](const std::string& why){e=op.cause=why;std::string cleanup;
        if(duplicate_edit::history_restore(*r,before.props,selection,filter,camera,known,cleanup))op.selection_restored=true;
        else{op.cleanup_cause=cleanup;e=op.cause+="; cleanup: "+cleanup;}return action::Outcome::Unknown;};
    op.attempted=true;source->history_started=true;
    if(authority&&!fault(calls::seed(before.props.random,before.region_random,*authority->scenery_stroke_before),"history authority RNG seed",e))return unknown(e);
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(result!=action::Outcome::Confirmed)return unknown(e);op.returned=true;
    if(!duplicate_edit::history_restore(*r,before.props,selection,filter,camera,known,e))return unknown(e);op.selection_restored=true;
    const auto opposite=undo?action::Action::RedoBuilding:action::Action::UndoBuilding;
    duplicate_history::Graph inverse;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e)||
       !history_graph(*r,*source,!undo,op.inverse,false,op.inverse_make,inverse,e))return unknown(e);
    host::WorldState current_world;duplicate_reader::Census census;
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(*r,uid,object,why);};
    if(!world(*r,current_world,e)||!host::same_world(before.props.world.origin,current_world.origin)||
       !duplicate_reader::census(r->memory,r->image,uintptr_t(current_world.origin.map),current_world.root,find,census,e))return unknown(e);
    std::set<uint64_t> alive;std::map<uint64_t,std::string> prior;
    for(const auto& [object,row]:census.objects){(void)object;if(row.family==Family::Scenery)alive.insert(row.handle.uid);}
    for(const auto& [uid,saved]:before.scenery){
        const auto found=std::find_if(before.props.handles.begin(),before.props.handles.end(),[&](const auto& row){return row.second.uid==uid&&row.second.object==saved.object;});
        if(found==before.props.handles.end()||!prior.emplace(uid,found->first).second)return unknown("Scenery history before full UID lacks its canonical alias");
    }
    auto retire=[&](const auto& ids,auto& keys,std::string& why){return remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,ids,keys,why);};
    auto restore=[&](uint64_t uid,const std::string& key,std::string& why){
        const auto birth=source->births.find(uid),removed=source->removed.find(uid);
        if(birth==source->births.end()&&removed==source->removed.end())return q::fail(why,"Scenery history restore UID is outside the complete original graph");
        const auto& expected=birth!=source->births.end()?birth->second:removed->second;uintptr_t pointer=0;Saved actual;
        return find(uid,pointer,why)&&scenery_make_history::live(r->memory,r->image,pointer,uid,before.props.world.root,before.region,find,actual,why)&&
            (scenery_stroke_history::same_inputs(actual,expected)||q::fail(why,"Scenery history native restored full stored inputs differ before Record capture"))&&
            remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,why);
    };
    auto records=[&](std::string& why){return sample(*r,binding,source->request.scenery_stroke.region,source->aliases,op.after,why)&&
        (same_binding(before,op.after)||q::fail(why,"Scenery history complete Record capture changed its World binding"))&&
        duplicate_edit::history_survivors(before.props,op.after.props,known,why);};
    scenery_alias_order::Receipt settled_aliases;
    if(!scenery_alias_order::capture(prior,alive,source->aliases,retire,restore,records,settled_aliases,e))return unknown(e);
    for(const auto& [uid,saved]:undo?source->removed:source->births){(void)saved;op.inverse_destroy.insert(uid);}
    op.inverse_cash=scenery_stroke_history::opposite(checked.cash);
    std::vector<std::string> restored;for(const auto& [uid,key]:source->aliases){
        const auto prior=before.scenery.find(uid),current=op.after.scenery.find(uid);
        if(current!=op.after.scenery.end()){
            const auto birth=source->births.find(uid);const auto removed=source->removed.find(uid);const auto& expected=birth!=source->births.end()?birth->second:removed->second;
            if(!scenery_stroke_history::same_inputs(current->second,expected))return unknown("Scenery history restored original full UID/type/pose/height differs");
        }
        if(prior==before.scenery.end()&&current!=op.after.scenery.end())restored.push_back(key);
    }
    action::SceneryStrokeEffect effect;
    std::sort(restored.begin(),restored.end());
    if(!scenery_stroke_state::difference(before.props.objects,op.after.props.objects,restored,digest,effect,e,true))return unknown(e);
    op.evidence.after=op.after.props.money;op.evidence.scenery_stroke_after=op.after.state;op.evidence.scenery_stroke_result=effect;
    op.evidence.undo_confirmed=true;op.evidence.selection_confirmed=true;
    if(!action::valid_scenery_stroke_effect(request,op.evidence)||!action::scenery_stroke_wire_fits(request,op.evidence,e))return unknown(e.empty()?"Scenery history original complete effect is invalid":e);
    if(authority&&(op.evidence.before!=authority->before||op.evidence.after!=authority->after||op.evidence.scenery_stroke_before!=authority->scenery_stroke_before||
       op.evidence.scenery_stroke_after!=authority->scenery_stroke_after||op.evidence.scenery_stroke_result!=authority->scenery_stroke_result||
       !duplicate_history::cash_equal(op.evidence.duplicate_cash,authority->duplicate_cash)))return unknown("Scenery history complete original result differs from Host");
    std::set<std::string> expected;for(const auto& row:effect.removed)expected.insert(row.key);
    if(std::set<std::string>(settled_aliases.retired.begin(),settled_aliases.retired.end())!=expected)return unknown("Scenery history retired canonical keys differ from actual original result");
    expected.clear();for(const auto& row:effect.created)expected.insert(row.key);
    if(std::set<std::string>(settled_aliases.restored.begin(),settled_aliases.restored.end())!=expected)return unknown("Scenery history restored canonical keys differ from actual original result");
    trim_sample(op.after);op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=history_operations.find(request.action_uuid);Port* source=nullptr;
    if(!r||r->thread!=GetCurrentThreadId()||found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||
       !source_for(binding,request,source,e))return q::fail(e,e.empty()?"Scenery history observation lacks exact confirmed operation":e);
    auto& op=found->second;Sample current;uintptr_t graph=0;duplicate_history::Graph inverse;
    const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!sample(*r,binding,source->request.scenery_stroke.region,source->aliases,current,e)||!same_binding(op.after,current)||current.state!=op.after.state||
       current.props.money!=op.after.props.money||current.props.undo!=op.after.props.undo||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,graph,e)||graph!=op.inverse||
       !scenery_stroke_graph::read(r->memory,r->image,graph,op.inverse_make,op.inverse_destroy,op.inverse_cash,inverse,e))
        return q::fail(e,e.empty()?"Scenery history actual complete state/inverse changed before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;auto& op=history_operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Scenery history already retired");op.retired=true;return true;
}
bool history_targets(const action::Binding& binding,const action::Request& request,std::map<uint64_t,std::string>& out,std::string& e){
    Port* source=nullptr;if(!source_for(binding,request,source,e))return false;out=source->aliases;return true;
}
bool history_publication(const action::Binding& binding,const action::Request& request,action::SceneryStrokeEffect& out,std::string& e){
    const auto found=history_operations.find(request.action_uuid);
    if(found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.evidence.scenery_stroke_result)
        return q::fail(e,"Scenery history publication lacks exact confirmed native operation");out=*found->second.evidence.scenery_stroke_result;return true;
}
}
