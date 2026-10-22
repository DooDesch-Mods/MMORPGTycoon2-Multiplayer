class GraphMemory final:public subscriber_lifecycle::Memory {
    Runtime& runtime_;
public:
    explicit GraphMemory(Runtime& runtime):runtime_(runtime){}
    bool read(uintptr_t address,void* out,size_t bytes,std::string& e)override{return runtime_.memory.copy(address,out,bytes,e);}
};
uint32_t reference_uid(uintptr_t image,uint64_t uid,uintptr_t& found)noexcept{
    __try{found=uintptr_t(reinterpret_cast<void*(*)(uint64_t)>(image+0x47ce20)(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool reference_keys(Runtime& r,const a::Context& c,const a::Binding& b,std::map<uintptr_t,std::string>& out,std::string& e){
    live::apply_timing::PageReadScope census_pages(!live::apply_timing::PageReadScope::reader_active());
    out.clear();live::Pin pin;if(!context(r,c,b,pin,e))return false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e)||world.world_epoch!=b.native_epoch||world.generation!=b.native_generation)return fail(e,e.empty()?"Release reference current NativeOrigin differs from saved binding":e);
    if(!([&]{live::apply_timing::PageReadScope original_read(live::apply_timing::PageBoundary::OriginalCall);return general_project_runtime::release_reference_keys(c,b,out,e);}()))return false;
    const auto add=[&](uintptr_t pointer,const char* family,const std::string& key){return pointer&&out.emplace(pointer,release_references::object_key(family,key)).second||fail(e,"Release canonical reference pointer aliases another owner");};
    auto read=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return at(r,address,offset,value,cause);};
    auto text=[&](uintptr_t address,std::string& value,std::string& cause){return string(r,address,value,cause);};
    release_references::Types types,checked;
    {live::apply_timing::PageReadScope base_type_pages(!live::apply_timing::PageReadScope::reader_active());
    if(!release_references::base_types(read,text,r.image,uintptr_t(world.state),types,e)||!release_references::base_types(read,text,r.image,uintptr_t(world.state),checked,e)||!release_references::add_base_keys(types,checked,out,e))return false;
    }
    std::vector<remove_diagnostics::LocalTarget> props;if(!([&]{live::apply_timing::PageReadScope original_read(live::apply_timing::PageBoundary::OriginalCall);return remove_diagnostics::identity_targets(props,e);}()))return false;
    for(const auto& prop:props){
        if(prop.world_epoch!=b.native_epoch||prop.world_generation!=b.native_generation||prop.snapshot_id!=c.snapshot_uuid||!prop.uid)
            return fail(e,"Release reference full-UID Building/Scenery current NativeOrigin differs: session_snapshot="+c.snapshot_uuid+" native_snapshot="+world.uuid+
                " alias_snapshot="+prop.snapshot_id+" prop_epoch="+std::to_string(prop.world_epoch)+" binding_epoch="+std::to_string(b.native_epoch)+
                " prop_generation="+std::to_string(prop.world_generation)+" binding_generation="+std::to_string(b.native_generation)+
                " uid="+std::to_string(prop.uid)+" key="+prop.record_key);
        const char* family=nullptr;
        auto modular=[](const auto& target,std::string& cause){live::apply_timing::PageReadScope original_read(live::apply_timing::PageBoundary::OriginalCall);return remove_diagnostics::is_modular_target(target,cause);};
        if(!release_references::prop_family(prop,modular,family,e))
            return fail(e,"Release reference Building/Scenery type proof failed: uid="+std::to_string(prop.uid)+" key="+prop.record_key+
                " type="+prop.type+" variant="+prop.variant+" normal_building="+std::to_string(prop.normal_building)+
                " normal_scenery="+std::to_string(prop.normal_scenery)+" classification="+prop.kind_cause+" cause="+e);
        if(!add(prop.object,family,prop.record_key))return false;
    }
    std::vector<npc_probe::action_runtime::StateOwner> npcs;if(!npc_probe::action_runtime::state_owners(c,b,npcs,e))return false;
    for(const auto& row:npcs){if(!add(row.npc,"npc",row.key))return false;
        for(const auto& [sequence,quest]:row.quests)if(!add(quest,"quest",nlohmann::json::array({row.key,sequence}).dump()))return false;
    }
    const auto root=uintptr_t(world.map)+0x1c0;
    region_saved_identity::Index region_index;
    if(!region_saved_identity::capture(r.image,uintptr_t(world.map),root,region_index,read,e))return false;
    for(const auto& [id,region]:region_index.saved){
        if(!add(region,"region",std::to_string(id)))return false;
    }
    if(!region_saved_identity::verify(r.image,uintptr_t(world.map),root,region_index,read,e))return false;
    if(!add(root,"root",""))return false;
    std::vector<std::pair<std::string,direct_monster_identity::Entry>> monsters;
    if(!direct_monster_identity::registry.enumerate(world.uuid,b.native_epoch,b.native_generation,root,monsters,e))return false;
    for(const auto& [birth,entry]:monsters){if(entry.retired)continue;
        uint64_t uid=0;uintptr_t ap=0,region=0,manager=0,owner=0,zone=0,proxy=0,data=0;int32_t count=0,capacity=0;
        if(!at(r,entry.pointer,0,ap,e)||ap!=r.image+0x16c0770||!at(r,entry.pointer,0x18,uid,e)||uid!=entry.uid||!at(r,entry.pointer,0x178,region,e)||region!=entry.scope.region||!at(r,region,0x988,manager,e)||manager!=entry.scope.manager||
           !at(r,manager,0,ap,e)||ap!=r.image+0x1714100||!at(r,manager,0x80,owner,e)||owner!=region||!at(r,entry.pointer,0x8d0,zone,e)||zone||!at(r,entry.pointer,0x8d8,proxy,e)||proxy||
           !at(r,manager,0x40,data,e)||!at(r,manager,0x48,count,e)||!at(r,manager,0x4c,capacity,e)||count<0||count>4096||capacity<count||(!data&&capacity))return fail(e,e.empty()?"Release Monster reference full UID/class/current manager/null-Zone witness differs":e);
        uintptr_t registered=0;const auto seh=reference_uid(r.image,uid,registered);
        if(seh||registered!=entry.pointer)return fail(e,"Release Monster reference actual full UID lookup differs SEH="+std::to_string(seh));
        unsigned matches=0;for(int32_t index=0;index<count;++index){uintptr_t object=0;if(!at(r,data,size_t(index)*8,object,e))return false;matches+=object==entry.pointer;}
        if(matches!=1||!add(entry.pointer,"monster",monster_target_key::canonical(birth)))return fail(e,e.empty()?"Release Monster reference absent/duplicated in current owning array":e);
    }
    a::Context current;building_mixed_identity::opened_file::CurrentWorld final;
    if(!a::selected_session().current(current,e)||current!=c||!native_open_runtime::read_current_world(final,e)||final.world_epoch!=world.world_epoch||final.generation!=world.generation||final.uuid!=world.uuid||final.state!=world.state||final.map!=world.map)return fail(e,e.empty()?"Release reference current Session/NativeOrigin changed during census":e);
    e.clear();return true;
}
bool reference_key(Runtime& r,const a::Context& c,const a::Binding& b,uintptr_t object,std::string& out,std::string& e){
    out.clear();std::map<uintptr_t,std::string> keys;if(!reference_keys(r,c,b,keys,e))return false;
    if(const auto found=keys.find(object);found!=keys.end()){out=found->second;return true;}
    uintptr_t ap=0;uint64_t uid=0;if(!at(r,object,0,ap,e)||!at(r,object,0x18,uid,e))return false;
    if(ap==r.image+0x16fc700||ap==r.image+0x16fca60){std::string key;network_identity::ObjectEntry entry;if(!network_identity::object_key(object,uid,key,entry,e))return false;out=release_references::object_key("network",key);return true;}
    return fail(e,"Release external owner lacks current canonical family binding AP-RVA="+std::to_string(ap-r.image));
}
bool reference_resolve(Runtime& r,const a::Context& c,const a::Binding& b,const std::string& key,uintptr_t& out,std::string& e){
    out=0;std::map<uintptr_t,std::string> keys;if(!reference_keys(r,c,b,keys,e))return false;
    for(const auto& [object,canonical]:keys)if(canonical==key){if(out)return fail(e,"Release canonical owner resolves to two current native pointers");out=object;}
    if(out)return true;
    try{const auto value=nlohmann::json::parse(key);if(value.is_array()&&value.size()==2&&value[0]=="network"&&value[1].is_string()){
        network_identity::ObjectEntry entry;if(!network_identity::object_target(value[1].get<std::string>(),entry,e))return false;
        std::string checked;if(!reference_key(r,c,b,entry.pointer,checked,e)||checked!=key)return fail(e,e.empty()?"Release Network canonical key/current full UID changed":e);out=entry.pointer;return true;
    }}catch(const std::exception& ex){return fail(e,"Release external canonical key parse: "+std::string(ex.what()));}
    return fail(e,"Release external canonical key is absent from current complete family census");
}
bool graph_sample(Runtime& r,const a::Context& c,const a::Binding& b,uintptr_t manager,release_graph::Value& value,std::string& e){
    GraphMemory memory(r);release_graph_native::Receipt receipt;
    auto key=[&](uintptr_t object,std::string& canonical,std::string& cause){return reference_key(r,c,b,object,canonical,cause);};
    return release_graph_native::capture(r.image,memory,manager,key,value,receipt,e);
}
