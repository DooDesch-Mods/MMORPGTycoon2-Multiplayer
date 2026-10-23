namespace character_actor_records {
namespace catalogue_detail {
using Sample=monster_action_runtime::character_membership::Sample;
namespace timing=character_catalogue_timing;
using Mode=character_catalogue_batch::Mode;
using CaptureResult=character_catalogue_batch::Result;
struct CaptureTiming {
    timing::Totals totals;timing::Binding binding{totals};timing::Clock::time_point started=timing::Clock::now();bool confirmed=false;
    ~CaptureTiming(){
        const auto elapsed=timing::elapsed(started);if(elapsed<100000000)return;
        static thread_local timing::Clock::time_point next_log{};static thread_local uint64_t suppressed=0;
        const auto now=timing::Clock::now();if(confirmed&&now<next_log){++suppressed;return;}next_log=now+std::chrono::seconds(1);
        using P=timing::Part;
        LOG_WARN("[CharacterCatalogue] Slow capture status={} total_ms={} code_ms={} prepare_ms_inclusive={} census_ms={} quests_ms={} references_ms={} external_owners_ms_inclusive={} external_props_ms_inclusive={} prop_read_ms_inclusive={} prop_catalog_code_ms={} prop_catalog_lookup_ms={} prop_catalog_text_ms={} lifetime_ms={} native_capture_ms_inclusive={} graph_read_ms={} canonical_records_ms={} final_hash_ms={} preparations={} records={} graph_nodes={} private_links={} suppressed_successful_slow_logs={}",
            confirmed?"confirmed":"failed",double(elapsed)/1000000.0,totals.ms(P::Code),totals.ms(P::Prepare),totals.ms(P::Census),totals.ms(P::Quests),
            totals.ms(P::References),totals.ms(P::ExternalOwners),totals.ms(P::ExternalProps),totals.ms(P::PropRead),totals.ms(P::PropCode),totals.ms(P::PropLookup),totals.ms(P::PropText),totals.ms(P::Lifetime),totals.ms(P::NativeCapture),totals.ms(P::GraphRead),
            totals.ms(P::CanonicalRecords),totals.ms(P::FinalHash),totals.preparations,totals.records,totals.graph_nodes,totals.private_links,suppressed);suppressed=0;
    }
};
bool prepare(Memory&,const action_session::Context&,const action_session::Binding&,Mode,Sample&,std::string&);
CaptureResult record(Memory&,const action_session::Context&,const action_session::Binding&,Mode,const Sample&,
                     const character_definition_identity::Definition&,std::string&,std::string&);
}
bool capture_catalogue(const action_session::Context& context,const action_session::Binding& binding,
                       CatalogueProof& out,std::string& error)try{
    catalogue_detail::CaptureTiming measured;
    out={};namespace member=monster_action_runtime::character_membership;
    Memory memory;{character_catalogue_timing::Scope code(character_catalogue_timing::Part::Code);if(!member::code(memory,error))return false;}
    auto prepare=[&](auto mode,auto& value,auto& cause){return catalogue_detail::prepare(memory,context,binding,mode,value,cause);};
    auto record=[&](auto mode,const auto& value,const auto& definition,auto& sha,auto& cause){return catalogue_detail::record(memory,context,binding,mode,value,definition,sha,cause);};
    auto stable=[](const auto& before,const auto& after){
        return character_catalogue_batch::same(before.definitions,after.definitions)&&before.users==after.users&&
               before.owners==after.owners&&action_runtime::same_world(before.world,after.world);
    };
    json first;if(!character_catalogue_batch::capture_manifest<catalogue_detail::Sample>(prepare,record,stable,first,error))return false;
    std::string sha;{character_catalogue_timing::Scope hash(character_catalogue_timing::Part::FinalHash);if(!quest_bridge::sha256(first.dump(),sha,error))return false;}
    out.sha256=std::move(sha);measured.confirmed=true;error.clear();return true;
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
    // SaveRecord has returned; this pure read ends before native record cleanup.
    live_host_state::apply_timing::PageReadScope record_pages;
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
namespace catalogue_detail {
bool prepare_references(Memory& memory,const action_session::Context& expected,const action_session::Binding& binding,
    Sample& value,std::string& error,Mode mode=Mode::Eager){
    namespace member=monster_action_runtime::character_membership;
    const auto& runtime=monster_action_runtime::state;
    value.manager=value.definitions.manager;
    host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,runtime.image,exe_sha,value.api,error))return false;
    host::Reader reader(memory.process,origin,runtime.image,runtime.thread,value.api);
    if(!reader.current(value.world,true,error)||!action_runtime::bound(value.world,binding,expected.snapshot_uuid,error))return false;
    // Without external manifest entries, unrelated props cannot affect canonical
    // record bytes. The eager fallback retains their complete admission checks.
    if(mode==Mode::LinkFree)return true;
    auto seed=value.quests;
    for(const auto& user:value.users.rows)if(user.family==Family::Npc&&!seed.owners.contains(user.object)){
        std::string key;if(!action_runtime::local_key(user.object,user.uid,key,error))return false;
        monster_quest_retirement::Owner row;row.pointer=user.object;row.uid=user.uid;row.key=key;
        seed.owners.emplace(user.object,std::move(row));
    }
    {timing::Scope measured(timing::Part::Quests);
        if(!monster_action_runtime::read_remove_quests(memory,binding,expected.snapshot_uuid,0,{},value.quests,error,&seed))return false;}
    {timing::Scope measured(timing::Part::References);
        if(!monster_action_runtime::quest_list::references(memory,binding,expected.snapshot_uuid,value.quests,value.references,error))return false;}
    {timing::Scope measured(timing::Part::ExternalOwners);
        if(!member::external_owners(memory,binding,expected.snapshot_uuid,value,error,nullptr))return false;}
    return true;
}
bool prepare(Memory& memory,const action_session::Context& expected,const action_session::Binding& binding,
    Mode mode,Sample& value,std::string& error){
    namespace member=monster_action_runtime::character_membership;
    timing::Scope measured(timing::Part::Prepare);if(timing::active)++timing::active->preparations;
    value={};if(!member::ready||member::fault)return fail(error,"Character authored definition requires its active original reader");
    {timing::Scope census(timing::Part::Census);if(!read_current(expected,binding,value.definitions,value.users,error))return false;}
    return prepare_references(memory,expected,binding,value,error,mode);
}
CaptureResult record(Memory& memory,const action_session::Context& expected,const action_session::Binding& binding,
    Mode mode,const Sample& value,const character_definition_identity::Definition& selected,std::string& original_record_sha,std::string& error){
    namespace member=monster_action_runtime::character_membership;
    const auto& runtime=monster_action_runtime::state;
    if(timing::active)++timing::active->records;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,runtime.image,runtime.thread,value.api);
    const auto lifetime=[&](){
        timing::Scope measured(timing::Part::Lifetime);
        live_host_state::apply_timing::PageReadScope pages;
        auto at=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){return field(memory,base,offset,out,cause);};
        for(const auto& array:value.definitions.arrays)if(!character_definition_identity::unchanged(at,array,error))return false;
        uintptr_t registered=0;uint32_t seh=0;
        {live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);
            seh=lookup(value.api.find,selected.uid,registered);}
        if(seh||registered!=selected.object)return fail(error,"Character authored original Record changed registered full UID SEH="+std::to_string(seh));
        uint64_t uid=0;host::WorldState current_world;action_session::Context current;
        if(!field(memory,selected.object,0x18,uid,error)||!reader.current(current_world,true,error)||
           !action_session::selected_session().current(current,error))return false;
        return (uid==selected.uid&&current==expected&&action_runtime::bound(current_world,binding,expected.snapshot_uuid,error)&&
                action_runtime::same_world(value.world,current_world))||
            fail(error,error.empty()?"Character authored original Record changed global owner/World/Session":error);
    };
    if(!lifetime())return CaptureResult::Failed;
    std::string captured;
    const auto result=member::complete_record_capture(memory,selected.object,0xf62880,0xf628e0,value.owners,mode,captured,error);
    if(result==CaptureResult::Failed||!lifetime())return CaptureResult::Failed;
    if(result==CaptureResult::NeedsExternalOwners)return result;
    original_record_sha=std::move(captured);error.clear();return CaptureResult::Complete;
}
}
bool read_definition(const action_session::Context& expected,const action_session::Binding& binding,
    int32_t kind,int32_t global_id,std::string& original_record_sha,std::string& error)try{
    original_record_sha.clear();
    namespace member=monster_action_runtime::character_membership;
    if(kind<0||kind>=5||global_id<0||!member::ready||member::fault)
        return fail(error,"Character authored definition requires its active original reader and global kind/ID");
    Memory memory;catalogue_detail::Sample value;
    if(!read_current(expected,binding,value.definitions,value.users,error))return false;
    const auto selected=std::find_if(value.definitions.definitions.begin(),value.definitions.definitions.end(),
        [&](const auto& row){return row.kind==kind&&row.id==global_id;});
    if(selected==value.definitions.definitions.end()||selected->deleted||!selected->uid)
        return fail(error,"Character authored definition global kind/ID is absent or deleted");
    if(!catalogue_detail::prepare_references(memory,expected,binding,value,error))return false;
    return catalogue_detail::record(memory,expected,binding,catalogue_detail::Mode::Eager,value,*selected,original_record_sha,error)==catalogue_detail::CaptureResult::Complete;
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
