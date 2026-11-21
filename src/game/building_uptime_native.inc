namespace uptime_native {
using Add=void(*)(void*,int32_t);
Add original_add=nullptr;release_ui_owner::Owner add_owner;
std::string fault;uint64_t suppressed=0,forwarded=0,failed=0;
constexpr uintptr_t add_rva=0x862b50,building_return=0x410365;
void retain(const std::string& cause){
    ++failed;if(fault.empty()){fault=cause.empty()?"Building uptime owner proof failed without a cause":cause;
        LOG_ERROR("[BuildingUptime] Client history authority failed cause={}; no native Add replay",fault);}
}
bool current_owner(Runtime& r,uintptr_t history,bool& owned,std::string& e){
    owned=false;
    if(GetCurrentThreadId()!=r.thread||!r.gm_context||!r.gm_binding||history<0xc60)
        return fail(e,"Building uptime requires current Client Mainthread binding");
    a::Context context;building_mixed_identity::opened_file::CurrentWorld world;
    if(!a::selected_session().current(context,e)||context!=*r.gm_context||context.role!=a::Role::Client||
       !native_open_runtime::read_current_world(world,e)||world.world_epoch!=r.gm_binding->native_epoch||
       world.generation!=r.gm_binding->native_generation||uintptr_t(world.map)+0x1c0!=r.gm_root)
        return fail(e,e.empty()?"Building uptime current Session/NativeOrigin differs":e);
    const auto object=history-0xc60;uintptr_t root=0;
    if(!ap(r,object,0x16df760,e)||!at(r,object,0x170,root,e))return false;
    if(root!=r.gm_root)return true;
    remove_diagnostics::LocalTarget target;
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);
        if(!remove_diagnostics::resolve_identity_destination(object,target,e))return false;}
    if(target.normal_scenery||target.object!=object||!target.uid||
       target.snapshot_id!=context.snapshot_uuid||target.world_epoch!=world.world_epoch||target.world_generation!=world.generation)
        return fail(e,"Building uptime canonical current Building/UID differs");
    bool uptime_owner=false;if(!remove_diagnostics::building_uptime_target(target,uptime_owner,e))return false;
    if(!uptime_owner)return fail(e,"Building uptime canonical current Building family differs");
    building_uptime::Native value;if(!building_uptime::read(r.memory,r.image,object,value,e))return false;
    owned=true;return true;
}
void dispatch(Runtime* r,void* receiver,int32_t value,uintptr_t caller){
    if(!original_add)throw std::runtime_error("Building uptime original Add was not published");
    if(!r||!r->gm_client||caller!=r->image+building_return){++forwarded;original_add(receiver,value);return;}
    if(r->gm_suspended||!fault.empty()){++suppressed;return;}
    bool owned=false;std::string e;
    try{
        if(!current_owner(*r,reinterpret_cast<uintptr_t>(receiver),owned,e)){retain(e);++suppressed;return;}
    }catch(const std::exception& error){retain(std::string("Building uptime owner guard: ")+error.what());++suppressed;return;}
    if(!owned){++forwarded;original_add(receiver,value);return;}
    if(value!=0&&value!=1)retain("Building uptime original Add argument is not Boolean int32");
    ++suppressed;
}
void entry(void* receiver,int32_t value){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    dispatch(published.load(std::memory_order_acquire),receiver,value,caller);
}
bool enable(Runtime& r,std::string& e){
    struct Body{uintptr_t rva;size_t bytes;const char* sha;};
    constexpr Body bodies[]{
        {0x862b50,512,"d28d47ffb4c0b24ff0d7f1982f1be4075a0fa41f6dbd14fb18492ffbd09f3932"},
        {0x4102f0,240,"0a286e829b97e9e803e34800847807167604b27ff48e70efc89fa8e1573710ae"},
        {0xe90b0,288,"94e6ee002731ce9c82c745053887811cb1090991147f29690983d73721d45355"}};
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.bytes);
        if(!r.memory.copy(r.image+body.rva,bytes.data(),bytes.size(),e)||mt2mp::sha256(bytes)!=body.sha)
            return fail(e,e.empty()?"Building uptime original full body differs RVA="+std::to_string(body.rva):e);
        if(body.rva==add_rva)add_owner={r.image+body.rva,reinterpret_cast<uintptr_t>(&entry),0,1,5,std::move(bytes)};
    }
    const auto made=MH_CreateHook(reinterpret_cast<void*>(r.image+0x862b50),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original_add));
    if(made!=MH_OK||!original_add)return fail(e,"Building uptime Add hook create failed status="+std::to_string(made));
    add_owner.trampoline=reinterpret_cast<uintptr_t>(original_add);
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(add_owner.entry));
    if(enabled!=MH_OK)return fail(e,"Building uptime Add hook enable failed status="+std::to_string(enabled));
    return release_ui_owner::verify(add_owner,[&](auto address,auto out,auto bytes,auto& cause){return r.memory.copy(address,out,bytes,cause);},
        [](const auto& bytes){return mt2mp::sha256(bytes);},bodies[0].sha,e);
}
}
