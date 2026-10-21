namespace character_actor_records {
bool capture_catalogue(const action_session::Context& context,const action_session::Binding& binding,
                       CatalogueProof& out,std::string& error)try{
    out={};namespace member=monster_action_runtime::character_membership;
    Memory memory;if(!member::code(memory,error))return false;
    character_definition_identity::Census before,after;character_membership_reader::Users users,after_users;
    if(!read_current(context,binding,before,users,error))return false;
    const auto capture=[&](const character_definition_identity::Census& census,json& rows){
        std::map<std::string,std::string> records;
        for(const auto& definition:census.definitions)if(!definition.deleted){std::string record;
            if(!read_definition(context,binding,definition.kind,definition.id,record,error))return false;
            if(!records.emplace(definition.key,std::move(record)).second)
                return fail(error,"Character catalogue repeats an original global key");
        }
        return character_catalogue_proof::rows(census,records,rows,error);
    };
    json first,second;
    if(!capture(before,first)||!read_current(context,binding,after,after_users,error)||!capture(after,second))return false;
    if(first!=second||before.state!=after.state||before.manager!=after.manager||users!=after_users)
        return fail(error,"Character catalogue original records/manager/users changed during complete capture");
    for(size_t kind=0;kind<before.arrays.size();++kind)
        if(before.arrays[kind].object!=after.arrays[kind].object||before.arrays[kind].members!=after.arrays[kind].members)
            return fail(error,"Character catalogue original global member lifetimes changed during capture");
    std::string sha;if(!quest_bridge::sha256(first.dump(),sha,error))return false;
    out.sha256=std::move(sha);error.clear();return true;
}catch(const std::exception& exception){return fail(error,std::string("Character catalogue original capture: ")+exception.what());}

namespace {
using NativeMemory=Memory;
struct RecordMemory final:subscriber_lifecycle::Memory {
    NativeMemory& source;
    explicit RecordMemory(NativeMemory& value):source(value){}
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return source.copy(p,out,bytes,error);}
};
struct Inspect {RecordMemory& memory;subscriber_lifecycle::Graph graph;};
bool inspect(void* user,const void* record,const void* context,std::string& error){auto& value=*static_cast<Inspect*>(user);
    return subscriber_lifecycle::read_graph(value.memory,uintptr_t(record),uintptr_t(context),{100000,1000000,32*1024*1024,100000},value.graph,error);
}
}
bool read_current(const action_session::Context& expected,const action_session::Binding& binding,
    character_definition_identity::Census& definitions,character_membership_reader::Users& users,std::string& error){
    live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    definitions={};users={};const auto& owner=monster_action_runtime::state;
    if(!owner.ready||GetCurrentThreadId()!=owner.thread)return fail(error,"Character current complete census requires its installed bound Mainthread");
    action_session::Context context;if(!action_session::selected_session().current(context,error))return false;
    if(context!=expected)return fail(error,"Character current complete census actual Session differs from caller");
    Memory memory;host::LookupApi api;host::NativeOrigin origin;host::WorldState before,after;
    if(!host::resolve_lookup(memory.process,owner.image,exe_sha,api,error))return false;
    host::Reader reader(memory.process,origin,owner.image,owner.thread,api);
    if(!reader.current(before,true,error)||!action_runtime::bound(before,binding,context.snapshot_uuid,error))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(memory,base,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);const auto seh=lookup(api.find,uid,pointer);
        return !seh||fail(cause,"Character complete census current FindUID failed SEH="+std::to_string(seh));};
    uintptr_t manager=0;character_definition_identity::Census current;character_membership_reader::Users current_users;
    if(!at(uintptr_t(before.origin.state),0x168,manager,error)||
       !([&]{live_host_state::apply_timing::PageReadScope definition_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_definition_identity::read(at,owner.image,uintptr_t(before.origin.state),manager,current,error);}())||
       !([&]{live_host_state::apply_timing::PageReadScope user_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_membership_reader::users(at,find,owner.image,current,current_users,error);}())||
       !reader.current(after,true,error)||!action_runtime::same_world(before,after))
        return fail(error,error.empty()?"Character current complete census changed NativeOpen world lifetime":error);
    definitions=std::move(current);users=std::move(current_users);error.clear();return true;
}
bool read_definition(const action_session::Context& expected,const action_session::Binding& binding,
    int32_t kind,int32_t global_id,std::string& original_record_sha,std::string& error)try{
    original_record_sha.clear();
    namespace member=monster_action_runtime::character_membership;
    const auto& runtime=monster_action_runtime::state;
    if(kind<0||kind>=5||global_id<0||!member::ready||member::fault)
        return fail(error,"Character authored definition requires its active original reader and global kind/ID");
    Memory memory;member::Sample value;
    if(!read_current(expected,binding,value.definitions,value.users,error))return false;
    const auto selected=std::find_if(value.definitions.definitions.begin(),value.definitions.definitions.end(),
        [&](const auto& row){return row.kind==kind&&row.id==global_id;});
    if(selected==value.definitions.definitions.end()||selected->deleted||!selected->uid)
        return fail(error,"Character authored definition global kind/ID is absent or deleted");
    value.manager=value.definitions.manager;value.selected=selected->object;value.uid=selected->uid;
    host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,runtime.image,exe_sha,value.api,error))return false;
    host::Reader reader(memory.process,origin,runtime.image,runtime.thread,value.api);
    if(!reader.current(value.world,true,error)||!action_runtime::bound(value.world,binding,expected.snapshot_uuid,error))return false;
    auto seed=value.quests;
    for(const auto& user:value.users.rows)if(user.family==Family::Npc&&!seed.owners.contains(user.object)){
        std::string key;if(!action_runtime::local_key(user.object,user.uid,key,error))return false;
        monster_quest_retirement::Owner row;row.pointer=user.object;row.uid=user.uid;row.key=key;
        seed.owners.emplace(user.object,std::move(row));
    }
    if(!monster_action_runtime::read_remove_quests(memory,binding,expected.snapshot_uuid,0,{},value.quests,error,&seed)||
       !monster_action_runtime::quest_list::references(memory,binding,expected.snapshot_uuid,value.quests,value.references,error)||
       !member::external_owners(memory,binding,expected.snapshot_uuid,value,error,nullptr))return false;
    std::string captured;
    if(!member::complete_record(memory,value.selected,0xf62880,0xf628e0,value.owners,captured,error))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){return field(memory,base,offset,out,cause);};
    for(const auto& array:value.definitions.arrays)if(!character_definition_identity::unchanged(at,array,error))return false;
    uintptr_t registered=0;const auto seh=lookup(value.api.find,value.uid,registered);
    uint64_t uid=0;host::WorldState after;action_session::Context current;
    if(!field(memory,value.selected,0x18,uid,error)||!reader.current(after,true,error)||
       !action_session::selected_session().current(current,error))return false;
    if(seh||registered!=value.selected||uid!=value.uid||current!=expected||!action_runtime::same_world(value.world,after))
        return fail(error,"Character authored original Record changed global owner/World/Session SEH="+std::to_string(seh));
    original_record_sha=std::move(captured);error.clear();return true;
}catch(const std::exception& exception){original_record_sha.clear();return fail(error,std::string("Character authored definition Record boundary: ")+exception.what());}
bool read(const action_session::Binding& binding,const std::string& snapshot,Family family,uintptr_t actor,const External& external,Result& out,std::string& error)try{
    out={};const auto& owner=monster_action_runtime::state;
    if(!owner.ready||GetCurrentThreadId()!=owner.thread||uint8_t(family)>uint8_t(Family::Toon)||!actor||!external)
        return fail(error,"Character complete original Actor Record requires its actual bound Mainthread/family/owner");
    Memory memory;host::LookupApi api;host::NativeOrigin origin;host::WorldState before,after;
    if(!host::resolve_lookup(memory.process,owner.image,exe_sha,api,error))return false;
    host::Reader reader(memory.process,origin,owner.image,owner.thread,api);
    if(!reader.current(before,true,error)||!action_runtime::bound(before,binding,snapshot,error))return false;
    uintptr_t ap=0,callback=0,registered=0;uint64_t uid=0;
    if(!field(memory,actor,0,ap,error)||!field(memory,actor,0x18,uid,error)||!field(memory,ap,0x438,callback,error))return false;
    if(!uid||callback!=owner.image+character_membership_reader::callbacks[size_t(family)])return fail(error,"Character complete Actor Record concrete callback/full UID differs");
    const auto seh=lookup(api.find,uid,registered);
    if(seh||registered!=actor)return fail(error,"Character complete Actor Record current FindUID differs SEH="+std::to_string(seh));
    const auto& selected=methods[size_t(family)];
    for(const auto& pin:character_membership_codes::helpers){
        if(pin.rva!=selected[0]&&pin.rva!=selected[1]&&pin.rva!=0x9667c0&&pin.rva!=0x9676a0&&pin.rva!=0x89e580&&pin.rva!=0x89e5f0)continue;
        std::vector<unsigned char> bytes(pin.bytes);if(!memory.copy(owner.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(error,"Character complete Actor original serializer body differs RVA="+std::to_string(pin.rva));
    }
    RecordMemory graph_memory(memory);Inspect value{graph_memory};
    const subscriber_lifecycle::CaptureApi calls{
        reinterpret_cast<subscriber_lifecycle::Construct>(owner.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(owner.image+0x9676a0),
        reinterpret_cast<subscriber_lifecycle::Construct>(owner.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(owner.image+0x89e5f0),
        reinterpret_cast<subscriber_lifecycle::PrepareSave>(owner.image+selected[0]),reinterpret_cast<subscriber_lifecycle::SaveRecord>(owner.image+selected[1])};
    {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!subscriber_lifecycle::capture_original(calls,reinterpret_cast<const void*>(actor),inspect,&value,out.cleanup,error))return false;}
    if(!duplicate_records::summarize(value.graph,actor,external,out.portable,error))return false;
    if(!reader.current(after,true,error)||!action_runtime::same_world(before,after))return fail(error,error.empty()?"Character complete original Actor Record changed current World lifetime":error);
    const auto final_seh=lookup(api.find,uid,registered);
    uint64_t final_uid=0;if(!field(memory,actor,0x18,final_uid,error))return false;
    if(final_seh||registered!=actor||final_uid!=uid)return fail(error,"Character complete Actor Record changed native identity SEH="+std::to_string(final_seh));
    out.original=std::move(value.graph);error.clear();return true;
}catch(const std::exception& exception){return fail(error,std::string("Character complete Actor Record boundary: ")+exception.what());}
}
