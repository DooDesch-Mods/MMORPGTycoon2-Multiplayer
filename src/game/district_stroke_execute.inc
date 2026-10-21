namespace district_stroke {
namespace {
struct Shape {poly::Storage value;bool constructed=false,destruction_entered=false;};
struct Operation {
    action::Binding binding;action::Request request;Snapshot before,after;
    identity::Edit edit;identity::Preview preview;action::Evidence evidence;
    inverse::Graph graph;json canonical_graph;
    std::map<uint64_t,std::string> keys;
    std::map<uintptr_t,inverse::Graph> inverses;
    calls::Cursor cursor;scenery_stroke_calls::Context context;LocalSelection selection;
    std::vector<std::unique_ptr<Shape>> shapes;
    bool attempted=false,returned=false,confirmed=false,retired=false,edit_open=false;
    bool remove_entered=false,remove_returned=false;std::string cause;
    int32_t region=-1;
};
std::map<std::string,Operation> operations,history_operations;
Operation* remove_replay=nullptr;
bool invoke_remove(Runtime&,Operation&,std::string&);
district_stroke_transition::Values persisted(const Snapshot& value){
    district_stroke_transition::Values rows;
    for(const auto& [key,row]:value.rows){
        const auto region=value.values.at(key).region;
        const auto stored=district_edit_history::stored(row.row.uid,0,0,value.values.at(key));
        rows.emplace(key,proof::stored(stored,key,region));
    }return rows;
}
bool merge_keys(const std::map<uint64_t,std::string>& source,std::map<uint64_t,std::string>& target,std::string& error){
    for(const auto& [uid,key]:source){const auto [found,inserted]=target.emplace(uid,key);
        if(!inserted&&found->second!=key)return q::fail(error,"District original reused a full UID with a different canonical identity");}
    return true;
}
bool graph_proof(Runtime& runtime,Operation& value,uintptr_t pointer,std::string& error){
    if(!inverse::read(runtime.memory,runtime.image,pointer,value.graph,error)||
       !merge_keys(value.before.keys,value.keys,error)||!merge_keys(value.after.keys,value.keys,error))return false;
    auto regions=value.before.regions;regions.insert(value.after.regions.begin(),value.after.regions.end());
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){return group_edit::find(runtime,uid,object,cause);};
    if(!proof::graph(value.graph,value.request.action_uuid,value.before.world.root,regions,value.keys,find,value.canonical_graph,value.keys,error))return false;
    return district_stroke_transition::restores(value.canonical_graph,persisted(value.after),persisted(value.before),error);
}
bool prepare_shapes(Runtime& runtime,Operation& value,std::string& error){
    const auto& intent=value.request.district_stroke;
    auto make=[&](const action::TerrainShape& shape){
        value.shapes.push_back(std::make_unique<Shape>());auto& owned=*value.shapes.back();poly::BorrowedPaths paths(shape);
        if(!fault(scenery_stroke_calls::shape_make(runtime.image,owned.value,paths.view,owned.constructed),"PolyShape construction",error))return false;
        action::TerrainShape actual;
        if(!poly::read(runtime.memory.process,uintptr_t(&owned.value),actual,error)||actual!=shape)
            return q::fail(error,error.empty()?"District original reconstructed shape differs":error);
        return true;
    };
    if(!make(intent.clip))return false;
    for(const auto& stamp:intent.stamps)if(!make(stamp.shape))return false;
    return true;
}
bool run_stroke(Runtime& runtime,Operation& value,std::string& error){
    const auto& intent=value.request.district_stroke;uintptr_t owner=0,selected=0;
    if(!resolve_region(runtime,value.before,intent.region,owner,error))return false;
    action::TerrainShape current_clip;uintptr_t clip=0;
    if(!q::at(runtime.memory,owner,0x28,clip,error)||!clip||!poly::read(runtime.memory.process,clip,current_clip,error)||current_clip!=intent.clip)
        return q::fail(error,error.empty()?"District stroke clip differs from the current original Region shape":error);
    if(!intent.selected.empty()){
        const auto found=value.before.rows.find(intent.selected);
        if(found==value.before.rows.end()||found->second.saved_region!=intent.region)
            return q::fail(error,"District stroke selected canonical owner is absent or outside the requested Region");
        selected=found->second.row.pointer;
    }
    if(!prepare_shapes(runtime,value,error))return false;
    value.cursor.image=runtime.image;
    if(!fault(calls::construct(value.cursor,intent,owner,selected,&value.shapes.front()->value),"cursor construction",error)||!value.cursor.constructed)return false;
    if(!q::at(runtime.memory,runtime.image,0x13df440,value.context.manager,error)||!value.context.manager||
       !q::at(runtime.memory,runtime.image,0x13df9d0,value.context.cursor,error)||
       !q::typed(runtime.memory,runtime.image,value.context.cursor,0x1786dd0,error))return false;
    if(!fault(scenery_stroke_calls::enter(value.context,value.before.world.root,intent.finish),"original Shard/Cursor context",error)||
       !fault(calls::begin_edit(value.cursor,value.edit_open),"BeginEdit",error)||
       !fault(calls::press(value.cursor,value.context.cursor,intent.press),"HandlePress",error)||!value.cursor.press_returned)return false;
    uintptr_t graph=0;
    if(!q::at(runtime.memory,value.cursor.object,0x790,graph,error)||!graph)
        return q::fail(error,error.empty()?"District original press did not create its graph":error);
    value.attempted=true;
    for(size_t index=0;index<intent.stamps.size();++index){
        uintptr_t active=0,current=0,root=0;
        if(!q::at(runtime.memory,runtime.image,0x1a3aa80,active,error)||active||
           !q::at(runtime.memory,value.context.manager,0x60,root,error)||root!=value.before.world.root||
           !q::at(runtime.memory,value.cursor.object,0x790,current,error)||current!=graph)
            return q::fail(error,error.empty()?"District original stroke root/graph changed between stamps":error);
        const auto route=intent.stamps[index].route;
        if(!fault(calls::paint(value.cursor,reinterpret_cast<calls::Paint>(originals.at(route)),&value.shapes[index+1]->value),"AddPaint",error))return false;
    }
    if(!fault(calls::finish(value.cursor,reinterpret_cast<calls::Finish>(originals[3]),intent.finish),"HandleFinish",error)||!value.cursor.finish_returned)return false;
    value.returned=true;return true;
}
bool cleanup(Runtime& runtime,Operation& value,std::string& error){
    bool okay=true;auto step=[&](const char* name,auto&& call){std::string cause;
        if(call(cause))return;okay=false;if(cause.empty())cause=std::string(name)+" failed without cause";
        if(error.empty())error=cause;else error+="; cleanup: "+cause;
    };
    if(value.context.installed&&!value.context.restored)step("context",[&](auto& cause){return fault(scenery_stroke_calls::restore(value.context),"context restore",cause);});
    if(value.cursor.constructed&&!value.attempted&&!value.cursor.finish_entered){
        // Before AddPaint the private graph contains only its initial Edit.
        // Destroy it, then let the original Finish close the exact EditScope.
        step("empty cursor",[&](auto& cause){uintptr_t graph=0;
            if(!q::at(runtime.memory,value.cursor.object,0x790,graph,cause))return false;
            if(!graph)return fault(calls::finish(value.cursor,reinterpret_cast<calls::Finish>(originals[3]),value.request.district_stroke.finish),"empty Finish",cause);
            value.cursor.finish_entered=true;
            bool destroyed=false;return fault(calls::discard_capture(runtime.image,value.cursor.object,graph,
                reinterpret_cast<calls::Finish>(originals[3]),value.request.district_stroke.finish.data(),destroyed,value.cursor.finish_returned),"empty graph cleanup",cause)&&destroyed&&value.cursor.finish_returned;
        });
    }
    if(value.cursor.constructed&&!value.cursor.destructor_entered&&(!value.attempted||value.cursor.finish_returned))
        step("cursor",[&](auto& cause){return fault(calls::destroy(value.cursor),"cursor destruction",cause);});
    for(auto& shape:value.shapes)if(shape->constructed&&!shape->destruction_entered){shape->destruction_entered=true;
        step("shape",[&](auto& cause){return fault(scenery_stroke_calls::shape_destroy(runtime.image,shape->value),"PolyShape destruction",cause);});}
    return okay;
}
bool after_original(Runtime& runtime,Operation& value,const action::Evidence* authority,uintptr_t inverse_pointer,
                    const std::map<int32_t,std::map<uint64_t,std::string>>& restored,std::string& error,const inverse::Graph* consumed=nullptr){
    if(!identity::preview_edit(value.edit,restored,value.preview,error)||!snapshot(runtime,value.binding,value.after,error,&value.preview))return false;
    if(!host::same_world(value.before.world.origin,value.after.world.origin)||value.before.world.root!=value.after.world.root||
       value.before.world.selection!=value.after.world.selection||value.before.money!=value.after.money||
       !state::changed_region(value.before.values,value.after.values,value.region,error))
        return q::fail(error,error.empty()?"District original changed World/Ledger or another Region":error);
    if(!graph_proof(runtime,value,inverse_pointer,error))return false;
    if(consumed){
        auto regions=value.before.regions;regions.insert(value.after.regions.begin(),value.after.regions.end());
        auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){return group_edit::find(runtime,uid,object,cause);};
        json forward;std::map<uint64_t,std::string> all;
        if(!proof::graph(*consumed,value.request.target_action_uuid,value.before.world.root,regions,value.keys,find,forward,all,error)||
           !district_stroke_transition::restores(forward,persisted(value.before),persisted(value.after),error))return false;
    }
    action::DistrictStrokeEffect effect{value.before.wire,value.after.wire,{},uint32_t(value.graph.nodes.size())};
    if(!digest(value.canonical_graph,effect.inverse_sha,error))return false;
    value.evidence.before=value.before.money;value.evidence.after=value.after.money;
    value.evidence.undo_confirmed=true;value.evidence.district_stroke=effect;
    if(!action::valid_district_stroke_effect(value.request,value.evidence)||
       (authority&&(authority->district_stroke!=value.evidence.district_stroke||authority->after!=value.evidence.after)))
        return q::fail(error,"District original complete effect/inverse differs from request/Host");
    std::map<uint64_t,std::string> retired;
    for(const auto& [uid,key]:value.before.keys)if(!value.after.keys.contains(uid))retired.emplace(uid,key);
    if(!value.selection.restore(runtime,value.before.world.selection,retired,error)||!identity::publish_edit(value.preview,error))return false;
    Snapshot confirmed;
    if(!snapshot(runtime,value.binding,confirmed,error)||!same(value.after,confirmed))return q::fail(error,error.empty()?"District complete values changed at publication":error);
    value.inverses.emplace(value.graph.pointer,value.graph);value.confirmed=true;return true;
}
bool observed(Runtime& runtime,const Operation& value,action::Evidence& out,std::string& error){
    Snapshot current;inverse::Graph graph;
    if(!snapshot(runtime,value.binding,current,error)||!same(value.after,current)||
       !inverse::read(runtime.memory,runtime.image,value.graph.pointer,graph,error)||graph!=value.graph)
        return q::fail(error,error.empty()?"District complete current state/Undo/inverse differs before ACK":error);
    out=value.evidence;out.retirement_confirmed=value.retired;return true;
}
void clear_operations(){operations.clear();history_operations.clear();remove_replay=nullptr;}
json operation_status(){json out=json::object();
    const auto append=[&](const auto& values){for(const auto& [id,value]:values)out[id]={{"attempted",value.attempted},{"returned",value.returned},
        {"confirmed",value.confirmed},{"retired",value.retired},{"cause",value.cause},{"stamp_calls",value.cursor.stamps_entered},{"stamp_returns",value.cursor.stamps_returned}};};
    append(operations);append(history_operations);return out;
}
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!runtime||!runtime->ready||!installed||runtime->thread!=GetCurrentThreadId()||request.action!=action::Action::DistrictStroke||
       request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||!state::intent(request.district_stroke)||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,error)||!code(*runtime,error)){
        if(error.empty())error="District stroke exact Controller/input/role is unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="District original stroke already attempted; no replay";return action::Outcome::Unknown;}
    Snapshot before;
    if(!snapshot(*runtime,binding,before,error))return action::Outcome::RejectedNoEffect;
    uintptr_t region_pointer=0;
    if(!resolve_region(*runtime,before,request.district_stroke.region,region_pointer,error))return action::Outcome::RejectedNoEffect;
    before.regions.emplace(region_pointer,request.district_stroke.region);
    if(authority&&(!action::valid_district_stroke_effect(request,*authority)||authority->district_stroke->before!=before.wire||authority->before!=before.money)){
        error="District Client complete current state/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::district_stroke_wire_fits(request,before.money,before.wire,error))return action::Outcome::RejectedNoEffect;
    auto& value=operations.try_emplace(request.action_uuid).first->second;
    value.binding=binding;value.request=request;value.before=std::move(before);value.region=request.district_stroke.region;
    auto failed=[&](){value.cause=error;return action::Outcome::Unknown;};
    if(!value.selection.capture(*runtime,value.before.world.selection,error)||!identity::prepare_edit(request.action_uuid,value.edit,error)){
        value.cause=error;return action::Outcome::RejectedNoEffect;}
    bool ran=false;replaying=true;
    try{ran=request.district_stroke.kind==Kind::Delete?invoke_remove(*runtime,value,error):run_stroke(*runtime,value,error);}
    catch(const std::exception& exception){error=std::string("District original stroke: ")+exception.what();}
    const bool cleaned=cleanup(*runtime,value,error);replaying=false;
    if(!ran||!cleaned)return failed();
    witness::UndoWitness undo;witness::GraphWitness graph;
    if(!witness::read_undo(runtime->memory.process,runtime->image,undo,error))return failed();
    if(undo==value.before.undo){
        if(!identity::preview_edit(value.edit,{},value.preview,error)||!snapshot(*runtime,binding,value.after,error,&value.preview)||
           !same(value.before,value.after)||authority||!value.selection.restore(*runtime,value.before.world.selection,{},error)||
           !identity::cancel_edit(value.edit,error)){
            if(error.empty())error="District original omitted its graph but changed state or differs from confirmed Host";return failed();}
        value.cause=error="District original stroke produced no World or history effect";return action::Outcome::RejectedNoEffect;
    }
    if(undo.undo.empty()||
       !witness::read_graph(runtime->memory.process,runtime->image,undo.undo.back(),graph,error)||
       !witness::validate_undo_commit(value.before.undo,undo,graph,error)||
       !after_original(*runtime,value,authority,undo.undo.back(),{},error))return failed();
    return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!runtime||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return q::fail(error,"District observation lacks its exact confirmed original stroke");
    return observed(*runtime,found->second,out,error);
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence out;if(!observe(binding,request,out,error))return false;auto& value=operations.at(request.action_uuid);
    if(value.retired)return q::fail(error,"District stroke already retired");
    if(!building_history_slice::remember_district_stroke(request.action_uuid,value.graph.pointer,error))return false;
    value.retired=true;return true;
}
}
