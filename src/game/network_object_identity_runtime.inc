namespace mt2mp::game::network_identity {
namespace {
bool current_object(uintptr_t pointer,uint64_t uid,objects::Scope& scope,objects::Entry& out,std::string& e){
    out={};Roots roots;if(!roots_read(roots,e)||!object_scope(roots,scope,e))return false;
    Binding row;objects::Kind kind;if(vtable(reinterpret_cast<void*>(pointer),node_vtable)){kind=objects::Kind::Node;if(!node_read(reinterpret_cast<void*>(pointer),roots,row,e))return false;}
    else if(vtable(reinterpret_cast<void*>(pointer),0x16fca60)){kind=objects::Kind::Pipe;if(!pipe_read(reinterpret_cast<void*>(pointer),roots,row,e))return false;}
    else {e="Network object current class differs";return false;}
    void* found=nullptr;const auto seh=lookup_native(uid,found);if(!uid||row.uid!=uid||seh||reinterpret_cast<uintptr_t>(found)!=pointer){e="Network object current full UID/FindUID differs SEH="+std::to_string(seh);return false;}
    out=object_entry(kind,row);return true;
}
}
bool prepare_aliases(const std::string& id,const std::string& digest,const aliases::Manifest& value,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);
    if(!valid_uuid(id)||!valid_sha(digest)){e="Network aliases exact UUID/SHA invalid";return false;}if(!aliases::valid(value,e))return false;
    if(capture_active||(!s.pending.empty())){e="Network aliases cannot replace an active SaveStream decision";return false;}
    if(client_active&&(s.uuid!=id||s.sha!=digest)){e="Network aliases differ from active original load";return false;}
    prepared_alias_uuid=id;prepared_alias_sha=digest;prepared_aliases=value;e.clear();return true;
}
bool begin_original_load(const std::string& id,const std::string& digest,std::string& e){
    {auto& s=shared();std::lock_guard lock(s.mutex);if(client_active){if(s.uuid!=id||s.sha!=digest){e="Network original load differs from staged snapshot";return false;}e.clear();return true;}}
    if(!begin_client_load(id,digest,0,e))return false;
    auto& s=shared();std::lock_guard lock(s.mutex);s.original_load=true;e.clear();return true;
}
bool finalize_original_load(const std::string& id,const std::string& digest,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);
    if(s.state=="failed"){e=s.error;return false;}
    if(!client_active||s.role!="client"||s.uuid!=id||s.sha!=digest||prepared_alias_uuid!=id||prepared_alias_sha!=digest)
        return fail_locked(s,"Network original completion differs from exact prepared file",&e);
    if(!main_gate(s,true,e))return false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e)||world.uuid!=id||world.sha256!=digest)
        return fail_locked(s,e.empty()?"Network original completed NativeOrigin UUID/SHA differs":e,&e);
    Roots roots;std::string cause;if(!roots_read(roots,cause)||world.map!=roots.map)return fail_locked(s,cause.empty()?"Network original completed root differs":cause,&e);
    if(s.original_load)s.expected=uint32_t(s.bindings.size());
    return publish(s,roots,e);
}
bool snapshot_aliases(const std::string& id,const std::string& digest,aliases::Manifest& out,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);out={};
    if(s.state!="host_ready"||s.uuid!=id||s.sha!=digest||captured_alias_uuid!=id||captured_alias_sha!=digest){e="Network companion requires exact sealed Host file mappings";return false;}
    if(!aliases::valid(captured_aliases,e))return false;out=captured_aliases;e.clear();return true;
}
bool object_key(uintptr_t object,uint64_t uid,std::string& key,ObjectEntry& out,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);key.clear();objects::Scope scope;objects::Entry actual;
    if(GetCurrentThreadId()!=s.main_thread||!current_object(object,uid,scope,actual,e)||!object_registry.resolve(scope,object,uid,key,e)||!object_registry.lookup(scope,key,out,e))return false;
    if(out!=actual){e="Network canonical object current manager/class/UID differs";return false;}e.clear();return true;
}
bool object_target(const std::string& key,ObjectEntry& out,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);out={};Roots roots;objects::Scope scope;objects::Entry entry,actual;
    if(GetCurrentThreadId()!=s.main_thread||!roots_read(roots,e)||!object_scope(roots,scope,e)||!object_registry.lookup(scope,key,entry,e)||!current_object(entry.pointer,entry.uid,scope,actual,e))return false;
    if(actual!=entry){e="Network target current manager/class/UID differs";return false;}out=actual;e.clear();return true;
}
bool register_place(const std::string& id,uintptr_t object,uint64_t uid,std::string& e){
    return register_places(id,{{object,uid}},e);
}
bool register_places(const std::string& id,const std::vector<std::pair<uintptr_t,uint64_t>>& born,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);objects::Scope scope;objects::Entry entry;
    if(GetCurrentThreadId()!=s.main_thread||!objects::uuid(id)||born.empty()||born.size()>2){e="Network confirmed birth set/UUID/Mainthread invalid";return false;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=id){e="Network birth registration lacks its exact pending Controller action";return false;}
    auto next=object_registry;for(const auto& [object,uid]:born){if(!current_object(object,uid,scope,entry,e)||!next.append(scope,objects::birth(id,entry.kind),entry,e))return false;}
    object_registry=std::move(next);e.clear();return true;
}
bool retire_object(const std::string& key,uintptr_t object,uint64_t uid,std::string& e){
    auto& s=shared();std::lock_guard lock(s.mutex);Roots roots;objects::Scope scope;objects::Entry old;
    if(GetCurrentThreadId()!=s.main_thread||!roots_read(roots,e)||!object_scope(roots,scope,e)||!object_registry.lookup(scope,key,old,e))return false;
    void* current=nullptr;const auto seh=lookup_native(uid,current);if(seh||current){e="Network retirement native full UID remains registered SEH="+std::to_string(seh);return false;}
    std::map<uint64_t,Binding> nodes,pipes;if(!census(roots,nodes,e)||!pipe_census(roots,pipes,e))return false;
    if(nodes.contains(uid)||pipes.contains(uid)){e="Network retirement remains in current manager census";return false;}
    return object_registry.retire(scope,key,object,uid,e);
}
bool inspect_history_object(uintptr_t object,uint64_t uid,ObjectEntry& out,std::string& error){
    auto& state=shared();std::lock_guard lock(state.mutex);objects::Scope scope;
    if(GetCurrentThreadId()!=state.main_thread){error="Network history current object requires Mainthread";return false;}
    return current_object(object,uid,scope,out,error)&&object_registry.current(scope,error);
}
bool reconcile_history(const std::string& id,const std::map<std::string,ObjectEntry>& before,const std::map<std::string,ObjectEntry>& after,std::string& error){
    auto& state=shared();std::lock_guard lock(state.mutex);Roots roots;objects::Scope scope;
    const auto controller=typed_action_runtime::controller_status();
    if(GetCurrentThreadId()!=state.main_thread||!objects::uuid(id)||!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=id){error="Network history retirement lacks exact pending Controller/Mainthread";return false;}
    if(!roots_read(roots,error)||!object_scope(roots,scope,error))return false;
    std::vector<std::pair<std::string,ObjectEntry>> rows;std::set<std::string> tombstones;
    if(!object_registry.enumerate(scope,rows,tombstones,error))return false;
    const std::map<std::string,ObjectEntry> registered(rows.begin(),rows.end());
    if(registered!=before){error="Network history complete registered BEFORE key/UID/owner set changed";return false;}
    auto next=object_registry;
    for(const auto& [key,old]:before)if(!after.contains(key)){void* current=nullptr;const auto exception=lookup_native(old.uid,current);
        if(exception||current){error="Network history retired full UID remains registered SEH="+std::to_string(exception);return false;}
        if(!next.retire(scope,key,old.pointer,old.uid,error))return false;}
    for(const auto& [key,row]:after){objects::Scope actual_scope;ObjectEntry actual;if(!current_object(row.pointer,row.uid,actual_scope,actual,error))return false;
        if(actual_scope!=scope||actual!=row){error="Network history current AFTER native Origin/full UID/owner differs";return false;}
        const auto old=before.find(key);if(old!=before.end()){if(old->second!=actual){error="Network history surviving canonical full UID/pointer/owner changed";return false;}}
        else if(!next.restore_history(scope,key,actual,error))return false;}
    if(!next.enumerate(scope,rows,tombstones,error))return false;
    if(std::map<std::string,ObjectEntry>(rows.begin(),rows.end())!=after){error="Network history complete canonical AFTER set differs";return false;}
    object_registry=std::move(next);error.clear();return true;
}
}
