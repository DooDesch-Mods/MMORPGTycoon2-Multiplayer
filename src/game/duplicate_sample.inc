namespace duplicate_edit {
namespace {
namespace q=npc_quest_targets;
using Family=action::DuplicateFamily;
using Handle=duplicate_native::Handle;
struct Sample {
    host::WorldState world;duplicate_reader::Census census;group_reader::Local groups;
    std::map<std::string,Handle> handles;
    std::map<std::string,action::DuplicateObject> objects;
    json selected_records=json::object();
    std::map<std::string,duplicate_records::UptimeWitness> uptime;
    std::map<uintptr_t,action::DuplicateObject> untouched_scenery;
    std::map<std::string,action::BuildingState> props;
    std::map<uintptr_t,std::string> external_owners;
    action::DuplicateState state;action::Ledger money;witness::UndoWitness undo;
    uintptr_t business=0,release=0,random=0;
};
void append_untouched_scenery(const Sample& value,json& untouched){
    // Unselected generated Scenery has no shared action identity. Keep its
    // complete original content and multiplicity without exporting local UIDs.
    if(!value.untouched_scenery.empty()){
        std::vector<std::string> rows;rows.reserve(value.untouched_scenery.size());
        for(const auto& [pointer,row]:value.untouched_scenery){(void)pointer;
            rows.push_back(json::array({row.region,row.pose,row.record_sha}).dump());}
        std::sort(rows.begin(),rows.end());
        untouched=json::array({std::move(untouched),json{{"untouched_scenery",rows}}});
    }
}
bool external_owner(Runtime& r,const Sample& sample,uintptr_t pointer,std::string& key,std::string& e){
    if(pointer==sample.world.root){key="root_shard";return true;}
    for(const auto region:sample.census.regions)if(pointer==region){int32_t id=-1;
        if(!q::at(r.memory,region,0x834,id,e))return false;key="region:"+std::to_string(id);return true;}
    for(const auto& [candidate,handle]:sample.handles)if(handle.object==pointer){key=candidate;return true;}
    const auto found=sample.external_owners.find(pointer);
    if(found!=sample.external_owners.end()){key=found->second;return true;}
    uintptr_t ap=0;if(!q::at(r.memory,pointer,0,ap,e))return false;
    return q::fail(e,"Duplicate original record external owner has no current canonical binding addresspoint_rva="+
        std::to_string(ap>=r.image?ap-r.image:0));
}
namespace child_npc=building_npc_lifecycle;
child_npc::Lease child_lease(const action::Binding& binding,const action::Request& request,const std::string& duplicate){
    return {request.snapshot_uuid,request.action_uuid,duplicate,binding.native_epoch,binding.native_generation};
}
bool child_parent(Runtime& r,const Sample& sample,const std::string& key,child_npc::Parent& out,std::string& e){
    out={};const auto handle=sample.handles.find(key);const auto value=sample.objects.find(key);
    if(handle==sample.handles.end()||value==sample.objects.end()||value->second.family!=Family::Building)
        return q::fail(e,"Duplicate child parent lacks its current canonical Building census row");
    out.key=key;out.object=handle->second.object;out.uid=handle->second.uid;out.saved_region=value->second.region;
    uintptr_t layout=0,child=0;uint8_t art=0;
    if(!q::typed(r.memory,r.image,out.object,0x16d93c0,e)||!q::at(r.memory,out.object,0xcba,art,e)||art>1||
       !q::at(r.memory,out.object,0x7d0,layout,e))return q::fail(e,e.empty()?"Duplicate child parent original Make form differs":e);
    out.make_form=art?child_npc::MakeForm::Art:layout?child_npc::MakeForm::Modular:child_npc::MakeForm::Functional;
    if(out.make_form==child_npc::MakeForm::Functional){
        if(!q::at(r.memory,out.object,0xa30,child,e)||(child&&!q::at(r.memory,child,0x18,out.stored_child_uid,e)))return false;
    }
    return true;
}
bool child_alias(Sample& sample,const child_npc::Receipt& receipt,std::string& e){
    if(!receipt.present)return true;
    const auto pointer=receipt.child.row.local_pointer;const auto& key=receipt.child.key;
    if(!pointer||!receipt.child.row.local_uid||key.empty())return q::fail(e,"Duplicate child receipt lacks its complete canonical full UID");
    for(const auto& [other,candidate]:sample.external_owners)if(candidate==key&&other!=pointer)
        return q::fail(e,"Duplicate restored child canonical key aliases another current NPC");
    const auto [found,inserted]=sample.external_owners.emplace(pointer,key);
    return inserted||found->second==key||q::fail(e,"Duplicate child pointer aliases another current canonical NPC");
}
struct GraphMemory final:subscriber_lifecycle::Memory {
    move_kiss::Memory& memory;explicit GraphMemory(move_kiss::Memory& m):memory(m){}
    bool read(uintptr_t p,void* out,size_t size,std::string& e)override{return memory.read(p,out,size,e);}
};
struct RecordCapture {GraphMemory& memory;subscriber_lifecycle::Graph graph;};
bool inspect_record(void* user,const void* record,const void* context,std::string& e){
    auto& v=*static_cast<RecordCapture*>(user);
    return subscriber_lifecycle::read_graph(v.memory,uintptr_t(record),uintptr_t(context),{100000,1000000,32*1024*1024,100000},v.graph,e);
}
bool record_object(Runtime& r,const Sample& sample,const Handle& handle,action::DuplicateObject& row,std::string& e,json* diagnostic=nullptr,duplicate_records::UptimeWitness* uptime=nullptr){
    namespace capture=subscriber_lifecycle;
    uintptr_t prepare=0,save=0;
    if(row.family==Family::Building){prepare=0xe570d0;save=0xe57130;}
    else if(row.family==Family::Scenery){prepare=0xe4fc90;save=0xe4fcf0;}
    else{prepare=0x1101cf0;save=0x1101d50;}
    GraphMemory memory(r.memory);RecordCapture value{memory};capture::CaptureReceipt receipt;
    const capture::CaptureApi api{reinterpret_cast<capture::Construct>(r.image+0x9667c0),reinterpret_cast<capture::Construct>(r.image+0x9676a0),
        reinterpret_cast<capture::Construct>(r.image+0x89e580),reinterpret_cast<capture::Construct>(r.image+0x89e5f0),
        reinterpret_cast<capture::PrepareSave>(r.image+prepare),reinterpret_cast<capture::SaveRecord>(r.image+save)};
    if(!capture::capture_original(api,reinterpret_cast<void*>(handle.object),inspect_record,&value,receipt,e))return false;
    auto owner=[&](uintptr_t p,std::string& canonical,std::string& why){
        return external_owner(r,sample,p,canonical,why);
    };
    duplicate_records::Witness witness;if(!duplicate_records::summarize_source(value.graph,handle.object,owner,witness,e,diagnostic,uptime))return false;
    row.record_sha=witness.sha;return true;
}
bool record(Runtime& r,const Sample& sample,const std::string& key,action::DuplicateObject& row,std::string& e,json* diagnostic=nullptr,duplicate_records::UptimeWitness* uptime=nullptr){
    return record_object(r,sample,sample.handles.at(key),row,e,diagnostic,uptime);
}
bool sample(Runtime& r,const action::Binding& binding,const action::DuplicateIntent* intent,
            const std::map<uint64_t,std::string>& created,Sample& out,std::string& e,
            const std::function<bool(Sample&,std::string&)>& prepare_records={},bool untouched_scenery_scope=false,bool duplicate_scope=false){
    out={};if(!world(r,out.world,e)||out.world.origin.world_epoch!=binding.native_epoch||out.world.origin.generation!=binding.native_generation)
        return q::fail(e,e.empty()?"Duplicate current complete World binding differs":e);
    auto find=[&](uint64_t uid,uintptr_t& p,std::string& why){return group_edit::find(r,uid,p,why);};
    if(!duplicate_reader::census(r.memory,r.image,uintptr_t(out.world.origin.map),out.world.root,find,out.census,e))return false;
    // This census consumes canonical aliases; full prop and original Record reads follow.
    json bindings;if(!remove_diagnostics::identity_bindings(bindings,e))return false;std::map<uint64_t,std::string> keys;std::set<uint64_t> retired;
    for(const auto& row:bindings.at("bindings")){
        if(row.at("revoked").get<bool>()){retired.insert(row.at("local_uid").get<uint64_t>());continue;}
        if(row.at("unknown").get<bool>()||row.at("snapshot_id")!=net::current_snapshot_id())return q::fail(e,"Duplicate complete shared registry is unknown or foreign");
        if(!keys.emplace(row.at("local_uid").get<uint64_t>(),row.at("record_key").get<std::string>()).second)return q::fail(e,"Duplicate complete shared registry full UID repeats");
    }
    for(const auto& [uid,key]:created){const auto found=keys.find(uid);if(found!=keys.end()&&found->second!=key)return q::fail(e,"Duplicate created full UID aliases another canonical key");keys[uid]=key;}
    auto owner=[&](uintptr_t region,int32_t& id,std::string& why){uintptr_t actual=0;if(!q::at(r.memory,region,0x834,id,why))return false;
        auto at=[&](uintptr_t p,size_t off,auto& v,std::string& error){return q::at(r.memory,p,off,v,error);};
        return region_saved_identity::resolve(r.image,uintptr_t(out.world.origin.map),out.world.root,id,region,actual,at,why);};
    auto bind=[&](uintptr_t p,uint64_t uid,std::string& key,int32_t& id,std::string& why){const auto found=out.census.objects.find(p);const auto k=keys.find(uid);
        if(found==out.census.objects.end())return q::fail(why,"Duplicate Group member is absent from current native census");
        if(found->second.handle.uid!=uid)return q::fail(why,"Duplicate Group member current full UID differs");
        if(found->second.family!=Family::Scenery)return q::fail(why,"Duplicate Group member current family is not Scenery");
        if(k==keys.end())return q::fail(why,"Duplicate Group member lacks current canonical Scenery key");
        key=k->second;id=found->second.region_id;return true;};
    std::vector<uintptr_t> scenery;
    for(const auto& [p,row]:out.census.objects){
        if(untouched_scenery_scope&&!keys.contains(row.handle.uid)&&row.family==Family::Scenery){
            if(row.parent)return q::fail(e,"Duplicate unbound Scenery has a Group parent");
            if(retired.contains(row.handle.uid))return q::fail(e,"Duplicate unbound Scenery has a live retired binding");
            action::DuplicateObject value;value.family=row.family;value.region=row.region_id;value.pose=row.pose;
            out.untouched_scenery.emplace(p,std::move(value));
        }else if(row.family==Family::Scenery)scenery.push_back(p);
    }
    auto bind_group=[&](uintptr_t,uint64_t uid,const std::string& derived,std::string& key,std::string& why){
        return group_edit::canonical_group(binding,out.world,uid,derived,key,why);};
    if(!group_reader::read(r.memory,r.image,out.world.root,out.world.selection,scenery,bind,owner,find,bind_group,out.groups,e))return false;
    std::map<uintptr_t,std::string> group_keys;
    for(const auto& group:out.groups.state.groups){const auto handle=out.groups.handles.at(group.key);group_keys.emplace(handle.object,group.key);
        out.handles.emplace(group.key,Handle{handle.object,handle.uid});action::DuplicateObject row;row.key=group.key;row.family=Family::Group;row.region=group.region;row.pose=group.pose;
        for(const auto& member:group.members)row.members.push_back(member.key);out.objects.emplace(row.key,std::move(row));}
    for(const auto& [p,local]:out.census.objects){if(out.untouched_scenery.contains(p))continue;const auto key=keys.find(local.handle.uid);
        if(key==keys.end())return q::fail(e,"Duplicate complete native census contains unbound prop full_uid="+std::to_string(local.handle.uid));
        action::DuplicateObject row;row.key=key->second;row.family=local.family;row.region=local.region_id;row.pose=local.pose;
        if(local.parent){const auto parent=group_keys.find(local.parent);if(parent==group_keys.end())return q::fail(e,"Duplicate native prop has unbound Group parent");row.parent=parent->second;}
        if(!out.handles.emplace(row.key,local.handle).second||!out.objects.emplace(row.key,row).second)return q::fail(e,"Duplicate complete canonical object key repeats");
        native::BuildingState prop;if(!scenery_kiss::read_action_prop(local.handle.uid,prop,e)||prop.prop.uid!=local.handle.uid)return false;
        out.props.emplace(row.key,world_state::building_state(prop));
    }
    std::vector<npc_saved_identity_runtime::Current> npc_owners;
    if(!npc_saved_identity_runtime::read_current(npc_owners,e))return false;
    std::set<std::string> npc_keys;
    for(const auto& npc_owner:npc_owners){if(npc_owner.key.empty()||!npc_owner.row.local_pointer||!npc_owner.row.local_uid||
           !npc_keys.insert(npc_owner.key).second||!out.external_owners.emplace(npc_owner.row.local_pointer,npc_owner.key).second)
            return q::fail(e,"Duplicate current NPC canonical owner census repeats or is incomplete");}
    if(prepare_records&&!prepare_records(out,e))return false;
    std::set<std::string> diagnostic_keys;
    if(intent&&Config::instance().diagnostic_mailbox_enabled)for(const auto& target:intent->rows){
        if(diagnostic_keys.size()==duplicate_records::diagnostic_sources)break;
        diagnostic_keys.insert(target.key);
        // Reuse this capture's original Record for created targets as well.
        // Pair the first source/result before spending the remaining budget.
        if(!created.empty()){
            if(target.family==Family::Group&&!target.created_keys.empty()&&diagnostic_keys.size()<duplicate_records::diagnostic_sources)
                diagnostic_keys.insert("group:"+target.created_keys.front());
            for(const auto& key:target.created_keys){
                if(diagnostic_keys.size()==duplicate_records::diagnostic_sources)break;diagnostic_keys.insert(key);}
        }
    }
    for(auto& [key,row]:out.objects){json* diagnostic=diagnostic_keys.contains(key)?&out.selected_records[key]:nullptr;
        duplicate_records::UptimeWitness* uptime=nullptr;
        if(duplicate_scope&&row.family==Family::Building){
            if(!intent)uptime=&out.uptime[key];
            else for(const auto& target:intent->rows)if(target.key==key&&target.captured_uptime){
                uptime=&out.uptime[key];uptime->captured=target.captured_uptime;}}
        if(!record(r,out,key,row,e,diagnostic,uptime))return false;}
    for(auto& [pointer,row]:out.untouched_scenery)
        if(!record_object(r,out,out.census.objects.at(pointer).handle,row,e))return false;
    std::set<std::string> excluded;
    if(intent){for(const auto& target:intent->rows){const auto source=out.objects.find(target.key);if(source==out.objects.end())return q::fail(e,"Duplicate requested source absent key="+target.key);
            out.state.sources.push_back(source->second);excluded.insert(target.key);
            if(!created.empty()){
                const auto top=target.family==Family::Group?"group:"+target.created_keys.front():target.created_keys.front();
                const auto found=out.objects.find(top);if(found==out.objects.end())return q::fail(e,"Duplicate original created top absent key="+top);
                out.state.created.push_back(found->second);excluded.insert(top);
                if(target.family==Family::Group)for(const auto& child:target.created_keys){const auto c=out.objects.find(child);if(c==out.objects.end())return q::fail(e,"Duplicate original created child absent key="+child);
                    out.state.created.push_back(c->second);excluded.insert(child);}
            }
        }}
    json untouched=json::array();for(const auto& [key,row]:out.objects)if(!excluded.contains(key))untouched.push_back(json::array({key,row.record_sha}));
    append_untouched_scenery(out,untouched);
    const auto text=untouched.dump();out.state.untouched_sha=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));
    uintptr_t release_manager=0;
    if(!q::at(r.memory,r.image,0x164b250,out.random,e)||!q::at(r.memory,out.random,0,out.state.random,e)||
       !q::at(r.memory,r.image,0x13df4f0,release_manager,e)||!q::typed(r.memory,r.image,release_manager,0x1714ed0,e)||
       !q::at(r.memory,release_manager,0x38,out.release,e)||!q::typed(r.memory,r.image,out.release,0x16c12b0,e)||
       !q::at(r.memory,out.release,0x120,out.state.release,e)||!q::at(r.memory,r.image,0x13df220,out.business,e)||
       !ledger(out.money,e)||!witness::read_undo(r.memory.process,r.image,out.undo,e))return false;
    if(!out.untouched_scenery.empty()){
        duplicate_reader::Census checked;
        if(!duplicate_reader::census(r.memory,r.image,uintptr_t(out.world.origin.map),out.world.root,find,checked,e))return false;
        if(checked!=out.census)return q::fail(e,"Duplicate untouched native census changed during original records");
    }
    host::WorldState after;if(!world(r,after,e)||!host::same_world(out.world.origin,after.origin)||out.undo.active_graph)
        return q::fail(e,e.empty()?"Duplicate complete read World/idle graph changed":e);
    return true;
}
bool sample_untouched_scenery(Runtime& r,const action::Binding& binding,const action::DuplicateIntent* intent,
                      const std::map<uint64_t,std::string>& created,Sample& out,std::string& e,
                      const std::function<bool(Sample&,std::string&)>& prepare_records={}){
    return sample(r,binding,intent,created,out,e,prepare_records,true);
}
bool sample_duplicate(Runtime& r,const action::Binding& binding,const action::DuplicateIntent* intent,
                      const std::map<uint64_t,std::string>& created,Sample& out,std::string& e,
                      const std::function<bool(Sample&,std::string&)>& prepare_records={}){
    return sample(r,binding,intent,created,out,e,prepare_records,true,true);
}
}
}
