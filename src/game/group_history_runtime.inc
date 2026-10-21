struct HistorySource {
    std::map<uint64_t,group_history::Known> known;
    std::vector<group_history::Node> undo_nodes,redo_nodes;
};
struct HistoryOperation {
    action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    std::vector<group_history::Node> nodes;uintptr_t source_graph=0,inverse_graph=0;
    bool attempted=false,original_returned=false,selection_restored=false,confirmed=false,retired=false;
    std::string cause,cleanup_cause;
};
std::map<std::string,HistorySource> history_sources;
std::map<std::string,HistoryOperation> history_operations;

bool history_nodes(Runtime& r,const Sample& state,uintptr_t graph,const HistorySource& source,
                   std::vector<group_history::LocalNode>& out,std::string& e){
    auto uid=[&](uint64_t value,uintptr_t& object,std::string& why){return find(r,value,object,why);};
    auto region=[&](int32_t id,uintptr_t& object,std::string& why){
        auto field=[&](uintptr_t p,size_t offset,auto& value,std::string& error){return q::at(r.memory,p,offset,value,error);};
        return region_saved_identity::resolve(r.image,uintptr_t(state.world.origin.map),state.world.root,id,0,object,field,why);};
    auto text=[&](uintptr_t object,std::string& value,std::string& why){return prop_text::text(r,object,value,why);};
    return group_history::code(r.memory,r.image,e)&&group_history::read(r.memory,r.image,state.world.root,graph,state.local,source.known,uid,region,text,out,e);
}
std::vector<group_history::Node> history_values(const std::vector<group_history::LocalNode>& rows){
    std::vector<group_history::Node> out;for(const auto& row:rows)out.push_back(row.value);return out;
}
bool remember_history(Runtime& r,const Operation& op,std::string& e){
    if(history_sources.contains(op.request.action_uuid))return q::fail(e,"Group source history already registered");
    HistorySource source;std::string restore_name;
    auto text=[&](uintptr_t object,std::string& value,std::string& why){return prop_text::text(r,object,value,why);};
    if(!group_history::restore_name(r.memory,r.image,text,restore_name,e))return false;
    for(const auto* state:{&op.before,&op.after})for(const auto& row:state->local.state.groups){
        const auto handle=state->local.handles.at(row.key);const auto name=state->group_names.find(row.key);
        if(name==state->group_names.end())return q::fail(e,"Group history source original name absent");
        const auto found=source.known.find(handle.uid);
        if(found!=source.known.end()&&(found->second.row.key!=row.key||found->second.name!=name->second))
            return q::fail(e,"Group source changed canonical UID/name");
        source.known[handle.uid]={handle.uid,row,name->second,restore_name};
    }
    std::vector<group_history::LocalNode> nodes;
    if(!history_nodes(r,op.after,op.native.graph,source,nodes,e))return false;
    source.undo_nodes=history_values(nodes);action::GroupState projected;
    if(!group_history::project(op.after.local.state,source.undo_nodes,projected,e)||
       !building_history_slice::remember_group(op.request.action_uuid,op.native.graph,e))return false;
    history_sources.emplace(op.request.action_uuid,std::move(source));return true;
}

bool history_identity(Runtime& r,const Sample& before,const Sample& after,const HistorySource& source,std::string& e){
    if(!same_binding(before,after)||before.money!=after.money)return q::fail(e,"Group history World/manager/full Ledger changed");
    for(const auto& prop:before.local.state.props){const auto found=after.local.handles.find(prop.key);
        if(found==after.local.handles.end()||found->second!=before.local.handles.at(prop.key))return q::fail(e,"Group history replaced live Scenery full UID key="+prop.key);}
    for(const auto& group:after.local.state.groups){const auto h=after.local.handles.at(group.key);const auto old=before.local.handles.find(group.key);
        const auto known=source.known.find(h.uid);
        if(old!=before.local.handles.end()){
            if(old->second!=h||before.group_names.at(group.key)!=after.group_names.at(group.key))return q::fail(e,"Group history replaced surviving Group full UID/name");
        }else if(known==source.known.end()||known->second.row.key!=group.key||known->second.restore_name!=after.group_names.at(group.key))
            return q::fail(e,"Group history recreated foreign Group UID/name");
    }
    for(const auto& group:before.local.state.groups)if(!after.local.handles.contains(group.key)){uintptr_t actual=0;
        if(!find(r,before.local.handles.at(group.key).uid,actual,e)||actual)return q::fail(e,e.empty()?"Group history deleted Group full UID remains registered":e);}
    return true;
}

bool history_ready(Port& port,const Sample& before,std::string& e){
    uintptr_t shards=0;
    if(!q::at(port.r.memory,before.world.selection,0xc8,port.old_filter,e)||
       !q::at(port.r.memory,port.r.image,0x13df440,shards,e)||!q::at(port.r.memory,shards,0x60,port.camera_shard,e))return false;
    if(std::any_of(port.old_filter.begin(),port.old_filter.end(),[](auto value){return value>1;}))return q::fail(e,"Group history local Selection filter invalid");
    for(auto object:before.local.raw){group_reader::Handle handle{object};uintptr_t actual=0;
        if(!q::at(port.r.memory,object,0x18,handle.uid,e)||!handle.uid||!find(port.r,handle.uid,actual,e)||actual!=object)
            return q::fail(e,e.empty()?"Group history saved local Selection full UID differs":e);
        port.old_selection.push_back(handle);
    }
    return true;
}
bool history_restore(Port& port,const Sample& before,const Sample& after,std::string& e){
    std::vector<uintptr_t> desired;std::set<uintptr_t> unique;
    auto add=[&](uintptr_t object){if(unique.insert(object).second)desired.push_back(object);};
    auto member=[&](const std::string& key){const auto value=std::find_if(after.local.state.props.begin(),after.local.state.props.end(),[&](const auto& row){return row.key==key;});
        if(value==after.local.state.props.end())return q::fail(e,"Group history local Selection member absent");
        const auto handle=after.local.handles.find(value->parent.empty()?key:value->parent);
        if(handle==after.local.handles.end())return q::fail(e,"Group history local Selection reciprocal target absent");add(handle->second.object);return true;};
    for(const auto& h:port.old_selection){
        const auto old=std::find_if(before.local.handles.begin(),before.local.handles.end(),[&](const auto& row){return row.second==h;});
        if(old!=before.local.handles.end()&&!old->first.starts_with("group:")){if(!member(old->first))return false;continue;}
        uintptr_t found=0;if(!find(port.r,h.uid,found,e))return false;
        if(found==h.object){add(found);continue;}
        if(old==before.local.handles.end())return q::fail(e,"Group history lost unrelated selected full UID");
        const auto group=std::find_if(before.local.state.groups.begin(),before.local.state.groups.end(),[&](const auto& row){return row.key==old->first;});
        if(group==before.local.state.groups.end())return q::fail(e,"Group history removed selection lacks complete old Group");
        for(const auto& row:group->members)if(!member(row.key))return false;
    }
    const bool selected=port.select(desired,e);const auto selection_cause=e;
    const auto code=filter_call(port.r.image,before.world.selection,port.old_filter);
    if(code)return q::fail(e,(selected?std::string{}:selection_cause+"; cleanup: ")+"Group history local filter restoration failed SEH="+std::to_string(code));
    if(!selected){e=selection_cause;return false;}
    std::vector<uintptr_t> raw,filtered;std::array<uint8_t,9> actual_filter{};uintptr_t shards=0,camera=0;
    if(!group_reader::selection(port.r.memory,port.r.image,before.world.selection,raw,filtered,e)||
       !q::at(port.r.memory,before.world.selection,0xc8,actual_filter,e)||!q::at(port.r.memory,port.r.image,0x13df440,shards,e)||
       !q::at(port.r.memory,shards,0x60,camera,e))return false;
    return (raw==desired&&actual_filter==port.old_filter&&camera==port.camera_shard)||q::fail(e,"Group history restored Selection/filter/camera differs");
}

action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||!installed||GetCurrentThreadId()!=r->thread||
       (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       request.canonical_record_key!="groups:"+request.target_action_uuid||request.snapshot_uuid!=net::current_snapshot_id()||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,e)||bool(authority)==Config::instance().is_host){
        if(e.empty())e="Group history exact pending direction/role/current snapshot unavailable";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){e="Group history already attempted; no replay";return action::Outcome::Unknown;}
    const auto original=operations.find(request.target_action_uuid);const auto remembered=history_sources.find(request.target_action_uuid);
    if(original==operations.end()||remembered==history_sources.end()||!original->second.confirmed||!original->second.retired||
       original->second.binding!=binding||original->second.request.snapshot_uuid!=request.snapshot_uuid){e="Group history source is not confirmed in current native World";return action::Outcome::RejectedNoEffect;}
    auto& source=remembered->second;const bool undo=request.action==action::Action::UndoBuilding;
    const auto& expected=undo?source.undo_nodes:source.redo_nodes;
    Sample before;uintptr_t graph=0;std::vector<group_history::LocalNode> local;
    if(expected.empty()||!sample(*r,binding,props(original->second.before.local),before,e)||
       !same_binding(original->second.after,before)||!building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e)||
       !history_nodes(*r,before,graph,source,local,e)){
        if(e.empty())e="Group history current source/direction/graph unavailable";return action::Outcome::RejectedNoEffect;}
    auto nodes=history_values(local);action::GroupState projected;
    if(!group_history::same_nodes(expected,nodes)||!group_history::project(before.local.state,nodes,projected,e)){
        if(e.empty())e="Group history exact original ordered node members differ";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_group_history_effect(request,*authority)||authority->before!=before.money||
       authority->group_before!=group_history::canonical(before.local.state)||
       !group_history::effect(before.local.state,*authority->group_after,nodes,e))){
        if(e.empty())e="Group history Client complete prior/effect differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::group_history_wire_fits(request,group_history::canonical(before.local.state),group_history::canonical(projected),before.money,e))return action::Outcome::RejectedNoEffect;
    Port port{*r,binding,before};if(!history_ready(port,before,e))return action::Outcome::RejectedNoEffect;
    auto& op=history_operations.emplace(request.action_uuid,HistoryOperation{binding,request,before}).first->second;
    op.nodes=nodes;op.source_graph=graph;op.attempted=true;
    const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(outcome==action::Outcome::RejectedNoEffect){op.attempted=false;op.cause=e;return outcome;}
    op.original_returned=outcome==action::Outcome::Confirmed;
    const auto original_cause=e;std::string read_cause;bool read=sample(*r,binding,props(before.local),op.after,read_cause);
    if(!op.original_returned)e=original_cause;else if(!read)e=read_cause;
    if(!op.original_returned||!read){
        op.cause=e.empty()?"Group history original did not confirm complete graph":e;
        std::string cleanup;if(read&&history_restore(port,before,op.after,cleanup))op.selection_restored=true;
        else if(!cleanup.empty()){op.cleanup_cause=cleanup;op.cause+="; cleanup: "+cleanup;}
        e=op.cause;return action::Outcome::Unknown;
    }
    auto fail=[&](){op.cause=e;std::string cleanup;
        if(history_restore(port,before,op.after,cleanup))op.selection_restored=true;
        else{op.cleanup_cause=cleanup;op.cause+="; cleanup: "+cleanup;}e=op.cause;return action::Outcome::Unknown;};
    if(!history_identity(*r,before,op.after,source,e)||!group_history::effect(before.local.state,op.after.local.state,nodes,e))return fail();
    const auto opposite=undo?action::Action::RedoBuilding:action::Action::UndoBuilding;
    std::vector<group_history::LocalNode> inverse;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse_graph,e)||
       !history_nodes(*r,op.after,op.inverse_graph,source,inverse,e))return fail();
    const auto inverse_values=history_values(inverse);
    if(!group_history::same_nodes(nodes,inverse_values,true)){e="Group history original inverse member/direction set differs";return fail();}
    for(const auto& row:inverse)if(row.value.kind==group_history::Kind::Add){
        const auto prior=std::find_if(before.local.state.groups.begin(),before.local.state.groups.end(),[&](const auto& g){return g.key==row.value.key;});
        if(prior==before.local.state.groups.end()||prior->pose!=row.value.saved_pose){e="Group history inverse lost exact original Group transform";return fail();}}
    if(!history_restore(port,before,op.after,e)){op.cause=e;return action::Outcome::Unknown;}op.selection_restored=true;
    Sample settled;if(!sample(*r,binding,props(before.local),settled,e)||!same_binding(op.after,settled)||
       settled.local.state!=op.after.local.state||settled.local.handles!=op.after.local.handles||settled.group_names!=op.after.group_names||
       settled.undo!=op.after.undo||settled.money!=before.money){op.cause=e.empty()?"Group history complete result changed during local Selection restoration":e;e=op.cause;return action::Outcome::Unknown;}
    op.after=std::move(settled);op.evidence.before=op.evidence.after=before.money;op.evidence.undo_confirmed=true;
    op.evidence.group_before=group_history::canonical(before.local.state);op.evidence.group_after=group_history::canonical(op.after.local.state);
    if(!action::valid_group_history_effect(request,op.evidence)||(authority&&op.evidence.group_after!=authority->group_after)){
        e=op.cause="Group history complete original result differs from Host";return action::Outcome::Unknown;}
    (undo?source.redo_nodes:source.undo_nodes)=inverse_values;op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=history_operations.find(request.action_uuid);
    if(!r||r->thread!=GetCurrentThreadId()||found==history_operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed)return q::fail(e,"Group history observation lacks exact confirmed operation");
    const auto& op=found->second;Sample actual;if(!sample(*r,binding,props(op.after.local),actual,e))return false;
    if(!same_binding(op.after,actual)||actual.local.state!=op.after.local.state||actual.local.handles!=op.after.local.handles||
       actual.group_names!=op.after.group_names||actual.money!=op.after.money||actual.undo!=op.after.undo)
        return q::fail(e,"Group history complete current full UID/pose/members/name/Undo/Ledger differs before ACK");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence current;if(!observe_history(binding,request,current,e))return false;auto& op=history_operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Group history already retired");op.retired=true;return true;
}
