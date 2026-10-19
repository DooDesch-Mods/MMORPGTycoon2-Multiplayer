namespace group_edit {
namespace {
namespace q=group_reader::q;
using Command=void(__fastcall*)(void*);using Single=void(__fastcall*)(void*,void*);
Command original_group=nullptr,original_ungroup=nullptr;Single original_single=nullptr;
bool installed=false,install_attempted=false;std::string install_cause,cause;
group_current_identity::Registry group_identities;
group_current_identity::Scope group_scope(const action::Binding& binding,const host::WorldState& world){
    return {net::current_snapshot_id(),binding.native_epoch,binding.native_generation,world.root};
}
bool canonical_group(const action::Binding& binding,const host::WorldState& world,uint64_t uid,
                     const std::string& derived,std::string& key,std::string& e){
    return group_identities.bind(group_scope(binding,world),uid,derived,key,e);
}
struct Sample {host::WorldState world;group_reader::Local local;action::Ledger money;witness::UndoWitness undo;uintptr_t random_owner=0;std::map<std::string,std::string> group_names;};
struct Pending {action::Binding binding;std::string snapshot;action::GroupIntent intent;Sample before;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;group_native::Receipt native;
    bool confirmed=false,retired=false;std::string cause;};
std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0,submitted=0,rejections=0;
void reject(const std::string& e){++rejections;if(cause!=e){cause=e;LOG_ERROR("[GroupEdit] {}",e);}}
uint32_t lookup(uintptr_t image,uint64_t uid,uintptr_t& out){
    __try{out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool find(Runtime& r,uint64_t uid,uintptr_t& out,std::string& e){const auto code=lookup(r.image,uid,out);
    return !code||q::fail(e,"Group original full UID lookup failed SEH="+std::to_string(code));}
bool sample(Runtime& r,const action::Binding& binding,const std::vector<uintptr_t>& extra,Sample& out,std::string& e){
    out={};if(!world(r,out.world,e)||out.world.origin.world_epoch!=binding.native_epoch||out.world.origin.generation!=binding.native_generation)
        return q::fail(e,e.empty()?"Group complete current native binding differs":e);
    const auto snapshot=net::current_snapshot_id();
    auto owner=[&](uintptr_t region,int32_t& id,std::string& why){uintptr_t actual=0;
        if(!q::at(r.memory,region,0x834,id,why))return false;
        auto read=[&](uintptr_t p,size_t offset,auto& v,std::string& error){return q::at(r.memory,p,offset,v,error);};
        return region_saved_identity::resolve(r.image,uintptr_t(out.world.origin.map),out.world.root,id,region,actual,read,why);};
    auto bind=[&](uintptr_t object,uint64_t uid,std::string& key,int32_t& region_id,std::string& why){remove_diagnostics::LocalTarget target;uintptr_t region=0;
        if(!remove_diagnostics::resolve_local_target(out.world.selection,object,target,why)||!target.normal_scenery||target.uid!=uid||
           target.snapshot_id!=snapshot||target.world_epoch!=binding.native_epoch||target.world_generation!=binding.native_generation||
           !q::at(r.memory,object,0x178,region,why)||!owner(region,region_id,why))return q::fail(why,why.empty()?"Group Scenery canonical/full UID/current owner differs":why);
        key=target.record_key;return true;};
    auto lookup_uid=[&](uint64_t uid,uintptr_t& p,std::string& why){return find(r,uid,p,why);};
    auto bind_group=[&](uintptr_t,uint64_t uid,const std::string& derived,std::string& key,std::string& why){
        return canonical_group(binding,out.world,uid,derived,key,why);};
    if(!group_reader::read(r.memory,r.image,out.world.root,out.world.selection,extra,bind,owner,lookup_uid,bind_group,out.local,e)||
       !q::at(r.memory,r.image,0x164b250,out.random_owner,e)||!q::at(r.memory,out.random_owner,0,out.local.state.random,e)||
       !ledger(out.money,e)||!witness::read_undo(r.memory.process,r.image,out.undo,e))return false;
    for(const auto& row:out.local.state.groups){std::string name;
        if(!prop_text::text(r,out.local.handles.at(row.key).object+0x38,name,e))return false;
        out.group_names.emplace(row.key,std::move(name));}
    host::WorldState after;if(!world(r,after,e)||!host::same_world(out.world.origin,after.origin)||out.undo.active_graph)
        return q::fail(e,e.empty()?"Group complete read world/idle Undo changed":e);
    return true;
}
std::vector<uintptr_t> props(const group_reader::Local& local){std::vector<uintptr_t> out;
    for(const auto& row:local.state.props)out.push_back(local.handles.at(row.key).object);return out;}
bool same_binding(const Sample& a,const Sample& b){return host::same_world(a.world.origin,b.world.origin)&&a.world.root==b.world.root&&
    a.world.selection==b.world.selection&&a.local.manager==b.local.manager&&a.random_owner==b.random_owner;}
uint32_t command(Command fn,uintptr_t receiver){__try{fn(reinterpret_cast<void*>(receiver));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct Array {uintptr_t ap=0;const uintptr_t* data=nullptr;int32_t count=0,capacity=0;};
uint32_t select_call(uintptr_t image,uintptr_t receiver,const Array& array){
    __try{reinterpret_cast<void(*)(void*,const Array*)>(image+0x2d5910)(reinterpret_cast<void*>(receiver),&array);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t filter_call(uintptr_t image,uintptr_t receiver,const std::array<uint8_t,9>& filter){
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x2d58f0)(reinterpret_cast<void*>(receiver),filter.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t create_call(uintptr_t image,uintptr_t manager,uintptr_t root,uintptr_t region,uintptr_t& out){
    __try{out=reinterpret_cast<uintptr_t(*)(void*)>(image+0x278bb0)(reinterpret_cast<void*>(manager));
        if(out){reinterpret_cast<void(*)(void*,void*)>(image+0x4cf6e0)(reinterpret_cast<void*>(out),reinterpret_cast<void*>(root));
            reinterpret_cast<void(*)(void*,void*)>(image+0x4d0140)(reinterpret_cast<void*>(out),reinterpret_cast<void*>(region));}return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t delete_empty_call(uintptr_t image,uintptr_t object){
    __try{reinterpret_cast<void(*)(void*)>(image+0x4cf170)(reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Port {
    Runtime& r;const action::Binding& binding;Sample& initial;Sample after;
    std::array<uint8_t,9> old_filter{};std::vector<group_reader::Handle> old_selection;uintptr_t camera_shard=0;
    bool filter_changed=false;
    bool ready(const group_reader::Local& before,std::string& e){
        uintptr_t cursor=0,behaviour=0,shards=0;uint8_t allowed=0;
        if(!q::at(r.memory,r.image,0x13df9d0,cursor,e)||!q::typed(r.memory,r.image,cursor,0x1786dd0,e)||
           !q::at(r.memory,cursor,0x190,behaviour,e)||!q::at(r.memory,behaviour,0x120,allowed,e)||
           !q::at(r.memory,before.selection,0xc8,old_filter,e)||!q::at(r.memory,r.image,0x13df440,shards,e)||!q::at(r.memory,shards,0x60,camera_shard,e))return false;
        if(allowed!=1||std::any_of(old_filter.begin(),old_filter.end(),[](auto v){return v>1;}))return q::fail(e,"Group original local Selection admission/filter not ready");
        for(auto p:before.raw){group_reader::Handle h{p};uintptr_t actual=0;
            if(!q::at(r.memory,p,0x18,h.uid,e)||!h.uid||!find(r,h.uid,actual,e)||actual!=p)return q::fail(e,e.empty()?"Group saved local Selection full UID differs":e);old_selection.push_back(h);}
        return true;
    }
    bool create(const group_reader::Local& before,const group_transition::Plan& plan,group_reader::Handle& out,std::string& e){
        const auto object=before.handles.at(plan.added.front()).object;uintptr_t region=0;
        if(!q::at(r.memory,object,0x178,region,e))return false;
        const auto code=create_call(r.image,before.manager,initial.world.root,region,out.object);
        if(out.object&&!q::at(r.memory,out.object,0x18,out.uid,e))return false;
        if(code||!out.object||!out.uid)return q::fail(e,"Group original creation/root binding failed SEH="+std::to_string(code));
        uintptr_t root=0,actual_region=0,found=0;int32_t count=-1;
        if(!q::typed(r.memory,r.image,out.object,0x1749de0,e)||!q::at(r.memory,out.object,0x170,root,e)||
           !q::at(r.memory,out.object,0x178,actual_region,e)||!q::at(r.memory,out.object,0x220,count,e)||!find(r,out.uid,found,e))return false;
        if(root!=initial.world.root||actual_region!=region||count!=0||found!=out.object)return q::fail(e,"Group new empty full UID/root/Region differs");
        return group_identities.birth(group_scope(binding,initial.world),out.uid,plan.target,e);
    }
    bool select(const std::vector<uintptr_t>& targets,std::string& e){
        std::array<uint8_t,9> all;all.fill(1);filter_changed=true;const auto code=filter_call(r.image,initial.world.selection,all);
        if(code)return q::fail(e,"Group original all-category selection filter failed SEH="+std::to_string(code));
        const Array array{r.image+0x1748c50,targets.data(),int32_t(targets.size()),int32_t(targets.size())};
        const auto fault=select_call(r.image,initial.world.selection,array);
        std::vector<uintptr_t> raw,filtered;
        if(fault)return q::fail(e,"Group original SetSelection failed SEH="+std::to_string(fault));
        if(!group_reader::selection(r.memory,r.image,initial.world.selection,raw,filtered,e))return false;
        return (raw==targets&&filtered==targets)||q::fail(e,"Group original requested ordered Selection not established");
    }
    bool original(action::GroupKind kind,std::string& e){const auto code=command(kind==action::GroupKind::Group?original_group:original_ungroup,initial.world.selection);
        return !code||q::fail(e,"Group original complete command failed SEH="+std::to_string(code));}
    bool read_after(const group_reader::Local& before,group_reader::Local& out,std::string& e){
        if(!sample(r,binding,props(before),after,e)||!same_binding(initial,after)||initial.money!=after.money)return q::fail(e,e.empty()?"Group original World/Ledger/manager changed":e);
        for(const auto& p:before.state.props)if(before.handles.at(p.key)!=after.local.handles.at(p.key))return q::fail(e,"Group original replaced surviving Scenery full UID");
        for(const auto& g:before.state.groups){const auto current=after.local.handles.find(g.key);const auto old=before.handles.at(g.key);
            if(current!=after.local.handles.end()){if(current->second!=old)return q::fail(e,"Group original replaced surviving Group full UID");}
            else{uintptr_t found=0;if(!find(r,old.uid,found,e)||found)return q::fail(e,e.empty()?"Removed Group full UID remains registered":e);}}
        out=after.local;return true;
    }
    bool inverse(const group_reader::Local& before,const group_reader::Local& value,const group_transition::Plan& plan,
                 const group_reader::Handle& provisional,uintptr_t& graph,std::string& e){
        if(after.undo.undo.empty())return q::fail(e,"Group original inverse graph absent");graph=after.undo.undo.back();witness::GraphWitness actual;
        if(!witness::read_graph(r.memory.process,r.image,graph,actual,e)||!witness::validate_undo_commit(initial.undo,after.undo,actual,e))return false;
        if(actual.operations!=int32_t(plan.inverse_count))return q::fail(e,"Group original inverse operation count differs");
        const bool grouping=!plan.target.empty();std::vector<group_inverse::Target> expected;
        for(size_t i=0;i<plan.inverse_count;++i){group_inverse::Target target;
            const auto& key=grouping?plan.target:plan.removed[i];const auto handle=grouping?value.handles.at(key):before.handles.at(key);
            target.group_uid=handle.uid;target.root=initial.world.root;
            if(provisional.object&&grouping&&handle!=provisional)return q::fail(e,"Group result replaced provisional full UID");
            if(grouping){for(const auto& member:plan.added)target.members.push_back(before.handles.at(member));}
            else{const auto& old=*std::find_if(before.state.groups.begin(),before.state.groups.end(),[&](const auto& g){return g.key==key;});
                target.pose=old.pose;auto field=[&](uintptr_t p,size_t offset,auto& out,std::string& why){return q::at(r.memory,p,offset,out,why);};
                if(!region_saved_identity::resolve(r.image,uintptr_t(initial.world.origin.map),target.root,old.region,0,target.region,field,e))return false;
                for(const auto& member:old.members)target.members.push_back(before.handles.at(member.key));}
            expected.push_back(std::move(target));
        }
        auto lookup_uid=[&](uint64_t uid,uintptr_t& p,std::string& why){return find(r,uid,p,why);};
        return group_inverse::read(r.memory,r.image,graph,grouping,expected,lookup_uid,e);
    }
    bool cleanup_empty(const group_reader::Handle& h,std::string& e){
        uintptr_t found=0;int32_t count=-1;
        if(!h.uid||!find(r,h.uid,found,e)||found!=h.object||!q::typed(r.memory,r.image,h.object,0x1749de0,e)||!q::at(r.memory,h.object,0x220,count,e))return false;
        if(count)return q::fail(e,"Group failed preparation is no longer empty; cleanup not attempted");
        const auto code=delete_empty_call(r.image,h.object);if(code)return q::fail(e,"Group empty preparation destructor failed SEH="+std::to_string(code));
        if(!find(r,h.uid,found,e)||found)return q::fail(e,"Group empty preparation full UID remains");
        Sample current;if(!sample(r,binding,props(initial.local),current,e))return false;
        return (current.local.state==initial.local.state&&current.money==initial.money&&current.undo==initial.undo)||q::fail(e,"Group empty preparation cleanup changed complete state/Undo/Ledger");
    }
    bool restore(const group_reader::Local& before,const group_reader::Local& value,const group_transition::Plan& plan,bool returned,std::string& e){
        std::vector<uintptr_t> desired;std::set<uintptr_t> unique;
        auto add=[&](uintptr_t p){if(unique.insert(p).second)desired.push_back(p);};
        for(const auto& h:old_selection){uintptr_t found=0;if(!find(r,h.uid,found,e))return false;
            if(found==h.object){
                if(returned){const auto prior=std::find_if(before.state.props.begin(),before.state.props.end(),[&](const auto& row){return before.handles.at(row.key)==h;});
                    if(prior!=before.state.props.end()){const auto current=std::find_if(value.state.props.begin(),value.state.props.end(),[&](const auto& row){return row.key==prior->key;});
                        if(current!=value.state.props.end()&&!current->parent.empty()&&current->parent!=prior->parent){add(value.handles.at(current->parent).object);continue;}}}
                add(found);continue;}
            if(!returned)return q::fail(e,"Group failed original removed local Selection target; unsafe restore blocked");
            auto old=std::find_if(before.handles.begin(),before.handles.end(),[&](const auto& entry){return entry.second==h;});
            if(old==before.handles.end()||std::find(plan.removed.begin(),plan.removed.end(),old->first)==plan.removed.end())return q::fail(e,"Group local Selection lost unrelated full UID");
            if(!plan.target.empty()){const auto next=value.handles.find(plan.target);if(next==value.handles.end())return q::fail(e,"Group replacement selection target unavailable");add(next->second.object);}
            else{const auto& g=*std::find_if(before.state.groups.begin(),before.state.groups.end(),[&](const auto& row){return row.key==old->first;});for(const auto& member:g.members)add(value.handles.at(member.key).object);}
        }
        const Array array{r.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
        const auto code=select_call(r.image,before.selection,array);if(code)return q::fail(e,"Group local Selection restoration failed SEH="+std::to_string(code));
        if(filter_changed){const auto fault=filter_call(r.image,before.selection,old_filter);if(fault)return q::fail(e,"Group local filter restoration failed SEH="+std::to_string(fault));}
        std::vector<uintptr_t> raw,filtered;std::array<uint8_t,9> filter;uintptr_t shards=0,shard=0;
        if(!group_reader::selection(r.memory,r.image,before.selection,raw,filtered,e)||!q::at(r.memory,before.selection,0xc8,filter,e)||
           !q::at(r.memory,r.image,0x13df440,shards,e)||!q::at(r.memory,shards,0x60,shard,e))return false;
        return (raw==desired&&filter==old_filter&&shard==camera_shard)||q::fail(e,"Group restored local Selection/filter/current Shard differs");
    }
};
bool capture(Runtime& r,uintptr_t receiver,uintptr_t single,action::GroupKind kind,std::string& e){
    const auto controller=typed_action_runtime::controller_status();action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!installed||!r.ready||r.thread!=GetCurrentThreadId()||captured||r.pending||!controller.value("started",false)||controller.value("pending",true)||
       controller.value("locked",true)||!typed_action_runtime::network_binding(snapshot,binding,e))return q::fail(e,e.empty()?"Group UI capture requires ready idle bound Controller":e);
    host::WorldState w;if(!world(r,w,e)||receiver!=w.selection)return q::fail(e,e.empty()?"Group normal UI SelectionManager differs":e);
    std::vector<uintptr_t> raw,filtered;if(!group_reader::selection(r.memory,r.image,receiver,raw,filtered,e))return false;
    if(single&&(filtered!=std::vector<uintptr_t>{single}))return q::fail(e,"Group InfoWindow target is not exact current selection");
    if(!group_reader::ui(r.memory,r.image,receiver,single,e))return false;
    Sample before;if(!sample(r,binding,filtered,before,e))return false;action::GroupIntent intent;intent.kind=kind;
    for(auto p:filtered){auto found=std::find_if(before.local.handles.begin(),before.local.handles.end(),[&](const auto& entry){return entry.second.object==p;});
        if(found==before.local.handles.end())return q::fail(e,"Group normal selected target lacks canonical owner");intent.selected.push_back(found->first);}
    group_transition::Plan plan;if(!action::valid_group(intent)||!group_transition::plan(before.local.state,intent,plan,e))return false;
    captured=Pending{binding,snapshot,std::move(intent),std::move(before)};++captures;return true;
}
void group_entry(void* receiver){auto* r=published.load(std::memory_order_acquire);
    if(!r||!net::session_wire::connected()||uintptr_t(_ReturnAddress())!=r->image+0x74bf65){original_group(receiver);return;}
    try{std::string e;if(!capture(*r,uintptr_t(receiver),0,action::GroupKind::Group,e))reject(e);}catch(const std::exception& x){reject(std::string("Group capture: ")+x.what());}}
void ungroup_entry(void* receiver){auto* r=published.load(std::memory_order_acquire);
    if(!r||!net::session_wire::connected()||uintptr_t(_ReturnAddress())!=r->image+0x74bf85){original_ungroup(receiver);return;}
    try{std::string e;if(!capture(*r,uintptr_t(receiver),0,action::GroupKind::Ungroup,e))reject(e);}catch(const std::exception& x){reject(std::string("Ungroup capture: ")+x.what());}}
void single_entry(void* receiver,void* target){auto* r=published.load(std::memory_order_acquire);
    if(!r||!net::session_wire::connected()||uintptr_t(_ReturnAddress())!=r->image+0x718fc8){original_single(receiver,target);return;}
    try{std::string e;if(!capture(*r,uintptr_t(receiver),uintptr_t(target),action::GroupKind::Ungroup,e))reject(e);}catch(const std::exception& x){reject(std::string("Group InfoWindow capture: ")+x.what());}}
}
#include "group_enable.inc"
#include "group_history_runtime.inc"
bool pending(){return bool(captured);}
void retire_session(){captured.reset();operations.clear();history_sources.clear();history_operations.clear();cause.clear();}
void tick(){
    if(!net::session_wire::connected()){if(captured)reject("Group source cancelled after session retirement");retire_session();return;}
    if(!captured)return;auto value=std::move(*captured);captured.reset();auto* r=published.load(std::memory_order_acquire);std::string e;action::Binding binding;Sample current;
    if(!r||!typed_action_runtime::network_binding(value.snapshot,binding,e)||binding!=value.binding||!sample(*r,binding,props(value.before.local),current,e)||
       !same_binding(value.before,current)||current.local.state!=value.before.local.state||current.local.handles!=value.before.local.handles||current.money!=value.before.money||current.undo!=value.before.undo){reject(e.empty()?"Group source full state changed before submission":e);return;}
    const auto id=uuid(e);if(id.empty()||!typed_action_runtime::submit_group(id,value.intent,e)){reject(e);return;}++submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||!installed||r->thread!=GetCurrentThreadId()||request.action!=action::Action::EditGroup||!action::valid_group(request.group)||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,e)||bool(authority)==Config::instance().is_host||request.snapshot_uuid!=net::current_snapshot_id())
        {if(e.empty())e="Group exact pending request/current role unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Group already attempted; no replay";return action::Outcome::Unknown;}
    std::vector<uintptr_t> extra;for(const auto& key:request.group.selected)if(!key.starts_with("group:")){remove_diagnostics::LocalTarget target;
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,e)||!target.normal_scenery)return action::Outcome::RejectedNoEffect;extra.push_back(target.object);}
    Sample before;if(!sample(*r,binding,extra,before,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(!action::valid_group_effect(request,*authority)||authority->group_before!=before.local.state||authority->before!=before.money)){
        e="Group Client complete prior ownership/order/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::group_wire_fits(request,before.local.state,before.money,e))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request}).first->second;op.before=before;op.evidence.before=before.money;op.evidence.group_before=before.local.state;
    Port port{*r,binding,op.before};group_reader::Local after;
    if(!group_native::execute(port,request.group,op.before.local,after,op.native,e,"group:action:"+request.action_uuid)){op.cause=e;return op.native.attempted?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;}
    if(!sample(*r,binding,props(before.local),op.after,e)||!same_binding(before,op.after)||op.after.local.state!=after.state||op.after.undo!=port.after.undo){op.cause=e.empty()?"Group complete restored readback differs":e;e=op.cause;return action::Outcome::Unknown;}
    op.evidence.after=op.after.money;op.evidence.group_after=op.after.local.state;op.evidence.undo_confirmed=op.evidence.selection_confirmed=true;
    if(!action::valid_group_effect(request,op.evidence)||(authority&&(op.evidence.group_after!=authority->group_after||op.evidence.after!=authority->after))){e=op.cause="Group original complete result differs from requested/Host effect";return action::Outcome::Unknown;}
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return q::fail(e,"Group observation lacks exact confirmed operation");
    const auto& op=found->second;Sample actual;if(!sample(*r,binding,props(op.after.local),actual,e))return false;
    if(!same_binding(op.after,actual)||actual.local.state!=op.after.local.state||actual.local.handles!=op.after.local.handles||actual.money!=op.after.money||actual.undo!=op.after.undo)
        return q::fail(e,"Group current full UID/census/Undo/Ledger differs from confirmed effect");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){action::Evidence actual;if(!observe(binding,request,actual,e))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return q::fail(e,"Group operation already retired");
    auto* r=published.load(std::memory_order_acquire);if(!r||!remember_history(*r,op,e))return false;op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.native.attempted},{"original_calls",op.native.original_calls},
    {"original_returned",op.native.original_returned},{"selection_restored",op.native.selection_restored},{"graph",op.native.graph},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.native.cleanup_cause}};
    json history=json::object();for(const auto& [id,op]:history_operations)history[id]={{"source",op.request.target_action_uuid},{"attempted",op.attempted},
        {"original_returned",op.original_returned},{"source_graph",op.source_graph},{"inverse_graph",op.inverse_graph},{"selection_restored",op.selection_restored},
        {"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.cleanup_cause}};
    return {{"history",std::move(history)},{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejections},{"cause",cause},{"operations",std::move(rows)}};}
}
