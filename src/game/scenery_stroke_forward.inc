namespace scenery_stroke {
namespace {
bool forward_action(action::Action value){return value==action::Action::SceneryBrushStroke||value==action::Action::SceneryEraseStroke;}
bool content_equal(action::SceneryStrokeState actual,const action::SceneryStrokeState& required){
    actual.random=required.random;actual.region_random=required.region_random;return actual==required;
}
bool current_graph(Runtime& r,Port& port,const Sample& current,std::string& e){
    if(!same_binding(port.after,current)||port.after.state!=current.state||port.after.props.money!=current.props.money||port.after.props.undo!=current.props.undo)
        return q::fail(e,"Scenery complete current result changed before ACK");
    std::set<uint64_t> born;for(const auto& [uid,saved]:port.births){
        born.insert(uid);const auto found=current.scenery.find(uid);const bool alive=std::any_of(port.evidence.scenery_stroke_result->created.begin(),port.evidence.scenery_stroke_result->created.end(),
            [&](const auto& row){return row.key==port.aliases.at(uid);});
        if(alive?(found==current.scenery.end()||found->second.object!=saved.object):(found!=current.scenery.end()))
            return q::fail(e,"Scenery complete current birth full UID/lifecycle differs");
    }
    for(const auto& [uid,saved]:port.removed){(void)saved;if(current.scenery.contains(uid))return q::fail(e,"Scenery original removed full UID is still present");}
    scenery_stroke_graph::Graph graph;return scenery_stroke_graph::read(r.memory,r.image,port.receipt.graph,port.removed,born,port.cash,graph,e);
}
}
bool pending(){return bool(capture)||bool(queued);}
void tick(){
    if(!net::session_wire::connected()){retire_session();return;}
    if(!queued)return;auto* r=published.load(std::memory_order_acquire);std::string e;action::Binding binding;
    if(!r||queued->snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(queued->snapshot,binding,e)||binding!=queued->binding){
        reject(e.empty()?"Scenery captured stroke World/snapshot retired before submission":e);queued.reset();return;
    }
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)){reject("Scenery captured stroke Controller is unavailable");queued.reset();return;}
    if(controller.value("pending",true))return;
    auto value=std::move(*queued);queued.reset();
    if(!typed_action_runtime::submit_scenery_stroke(value.id,value.intent,e)){reject(e);return;}++submissions;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!installed||!r->ready||r->thread!=GetCurrentThreadId()||!forward_action(request.action)||!action::valid_scenery_stroke(request.scenery_stroke)||
       request.snapshot_uuid!=net::current_snapshot_id()||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)||
       bool(authority)==Config::instance().is_host){if(e.empty())e="Scenery exact pending action/role is unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)||quarantined.contains(request.action_uuid)){e="Scenery stroke already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_scenery_stroke_effect(request,*authority)){e="Scenery Host complete evidence is invalid";return action::Outcome::RejectedNoEffect;}
    Sample before;if(!sample(*r,binding,request.scenery_stroke.region,{},before,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(!content_equal(before.state,*authority->scenery_stroke_before)||before.props.money!=authority->before)){
        e="Scenery Client complete objects/bugs/Release/Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    op.port=std::make_shared<Port>(Port{*r,binding,request,std::move(before),{},op.native});auto& port=*op.port;
    if(authority)port.authority_before=authority->scenery_stroke_before;
    const bool returned=scenery_stroke_native::execute(port,request.scenery_stroke,op.native,e);
    if(!returned){op.cause=e;
        if(port.no_effect&&op.native.cleanup_cause.empty()&&op.native.context_restored&&op.native.selection_restored&&op.native.cursor_destroyed&&op.native.resources_released){
            port.trim();return action::Outcome::RejectedNoEffect;
        }
        return (op.native.attempted||port.rng_attempted)?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;
    }
    auto unknown=[&](){op.cause=e;return action::Outcome::Unknown;};
    port.evidence.selection_confirmed=op.native.selection_restored;op.evidence=port.evidence;
    if(!action::valid_scenery_stroke_effect(request,op.evidence)||!action::scenery_stroke_wire_fits(request,op.evidence,e)){
        if(e.empty())e="Scenery complete native result fails its action contract";return unknown();}
    if(authority&&(op.evidence.before!=authority->before||op.evidence.after!=authority->after||op.evidence.scenery_stroke_before!=authority->scenery_stroke_before||
       op.evidence.scenery_stroke_after!=authority->scenery_stroke_after||op.evidence.scenery_stroke_result!=authority->scenery_stroke_result)){
        e="Scenery actual original complete result differs from Host";return unknown();}
    std::set<std::string> expected;for(const auto& row:op.evidence.scenery_stroke_result->removed)expected.insert(row.key);
    if(std::set<std::string>(port.settled_aliases.retired.begin(),port.settled_aliases.retired.end())!=expected){e="Scenery original removed canonical aliases differ from actual result";return unknown();}
    for(const auto& row:op.evidence.scenery_stroke_result->created){const auto found=std::find_if(port.aliases.begin(),port.aliases.end(),[&](const auto& alias){return alias.second==row.key;});
        if(found==port.aliases.end()||!remove_diagnostics::admit_created_building(request.snapshot_uuid,row.key,found->first,e))return unknown();}
    op.confirmed=true;port.trim();return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||r->thread!=GetCurrentThreadId()||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.port)
        return q::fail(e,"Scenery observation lacks an exact confirmed operation");
    auto& op=found->second;auto& port=*op.port;Sample current;
    if(!sample(*r,binding,request.scenery_stroke.region,port.aliases,current,e)||!current_graph(*r,port,current,e))return false;
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool publication(const action::Binding& binding,const action::Request& request,action::SceneryStrokeEffect& out,std::string& e){
    const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.evidence.scenery_stroke_result)
        return q::fail(e,"Scenery publication lacks an exact confirmed native operation");
    out=*found->second.evidence.scenery_stroke_result;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe(binding,request,actual,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Scenery stroke already retired");
    if(!building_history_slice::remember_scenery(request.action_uuid,op.native.graph,e))return false;
    op.retired=true;return true;
}
void retire_session(){
    if(capture||queued)reject("Scenery captured stroke cancelled after session retirement");capture.reset();queued.reset();
    retire_histories();
    for(auto it=operations.begin();it!=operations.end();){
        if(!it->second.confirmed&&(it->second.native.attempted||(it->second.port&&it->second.port->rng_attempted))){
            if(it->second.port){trim_sample(it->second.port->before);trim_sample(it->second.port->after);}
            auto node=operations.extract(it++);quarantined.insert(std::move(node));
        }else it=operations.erase(it);
    }
}
json status(){
    json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"action",uint8_t(op.request.action)},{"region",op.request.scenery_stroke.region},{"mode",op.request.scenery_stroke.mode},{"stamps",op.request.scenery_stroke.stamps.size()},{"attempted",op.native.attempted},{"stamp_calls",op.native.stamp_calls},
        {"stamp_returns",op.native.stamp_returns},{"graph",op.native.graph},{"graph_committed",op.native.graph_committed},
        {"context_restored",op.native.context_restored},{"selection_restored",op.native.selection_restored},{"cursor_destroyed",op.native.cursor_destroyed},
        {"resources_released",op.native.resources_released},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.native.cleanup_cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submissions},{"rejected",rejections},{"cause",cause},
        {"quarantined",quarantined.size()},{"operations",std::move(rows)},{"history",history_status()}};
}
}
