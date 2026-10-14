#include "npc_remove_selection.h"
#include "npc_remove_replay_binding.h"
namespace {
bool selection_absent(Memory& memory,uintptr_t image,uintptr_t manager,uintptr_t target,
                      std::string& error) {
    uintptr_t ap=0,focused=0;
    if(!field(memory,manager,0,ap,error)||ap!=image+0x17205d0||
       !field(memory,manager,0x80,focused,error))
        return fail(error,error.empty()?"NPC selection manager class differs":error);
    if(focused==target)return fail(error,"NPC target remains focused after original DestroyProps");
    for(const size_t offset:{size_t(0x20),size_t(0x38)}) {
        uintptr_t array_ap=0,data=0;int32_t count=0,capacity=0;
        if(!field(memory,manager,offset,array_ap,error)||
           !field(memory,manager,offset+8,data,error)||
           !field(memory,manager,offset+16,count,error)||
           !field(memory,manager,offset+20,capacity,error))return false;
        if(array_ap!=image+0x1748c50||count<0||count>100000||capacity<count||capacity>100000||
           (count&&!data)||data>UINTPTR_MAX-size_t(count)*sizeof(uintptr_t))
            return fail(error,"NPC selection array class or span differs");
        for(int32_t index=0;index<count;++index){uintptr_t selected=0;
            if(!field(memory,data,size_t(index)*8,selected,error))return false;
            if(selected==target)return fail(error,"NPC target remains in selection after DestroyProps");}
    }
    error.clear();return true;
}
bool placed_target(const std::string& target,const action::Binding& binding,
                   const std::string& snapshot,Operation*& place,std::string& error) {
    place=nullptr;const auto found=state.operations.find(target);
    if(found==state.operations.end()||found->second.request.action!=action::Action::PlaceNPC||
       !found->second.confirmed||!found->second.retired||found->second.target_absent||
       found->second.request.snapshot_uuid!=snapshot||found->second.binding!=binding||
       !found->second.local_uid||!found->second.local_pointer)
        return fail(error,"NPC Remove target lacks confirmed retired Place action in current World");
    place=&found->second;return true;
}
bool target_binding(const std::string& target,const action::Binding& binding,
                    const std::string& snapshot,ui::LocalBinding& out,uintptr_t& region,
                    npc_census::State& census,std::string& error) {
    out={};region=0;
    if(!npc_target_key::input(target))return fail(error,"NPC target key invalid");
    uint64_t uid=0;uintptr_t pointer=0;int32_t saved_region=-1,type=-1;
    if(snapshot_npc_key::valid(target)){
        npc_saved_identity_runtime::Current saved;
        if(!npc_saved_identity_runtime::resolve(target,saved,error))return false;
        uid=saved.row.local_uid;pointer=saved.row.local_pointer;saved_region=saved.saved_region;type=saved.row.type;
    }else{
        Operation* place=nullptr;if(!placed_target(target,binding,snapshot,place,error))return false;
        uid=place->local_uid;pointer=place->local_pointer;saved_region=place->request.npc.region;type=place->request.npc.type;
    }
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world;uintptr_t selection=0,ap=0,registered=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    if(!reader.current(world,true,error)||!bound(world,binding,snapshot,error)||
       !region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
           saved_region,0,region,read_field,error)||
       !field(memory,state.image,0x13df630,selection,error)||!selection||
       !field(memory,selection,0,ap,error)||!npc_census::read(memory,state.image,region,census,error))return false;
    const auto seh=lookup(api.find,uid,registered);
    if(seh||ap!=state.image+0x17205d0||registered!=pointer)
        return fail(error,"NPC target UID table or SelectionManager differs SEH="+std::to_string(seh));
    const auto row=std::find_if(census.rows.begin(),census.rows.end(),[&](const auto& item){return item.local_uid==uid;});
    if(row==census.rows.end()||row->local_pointer!=registered||row->type!=type)
        return fail(error,"NPC target is absent from current owner Manager");
    out={selection,registered,uid,binding.native_epoch,binding.native_generation,
         snapshot,npc_target_key::canonical(target),false,false,true};
    return true;
}
bool local_target_key(uintptr_t object,uint64_t uid,std::string& target,std::string& error) {
    target.clear();
    for(const auto& [id,op]:state.operations)if(op.request.action==action::Action::PlaceNPC&&
        op.request.snapshot_uuid==net::current_snapshot_id()&&op.local_pointer==object&&op.local_uid==uid&&
        op.confirmed&&op.retired&&!op.target_absent){
        if(!target.empty())return fail(error,"NPC target has multiple current Place action bindings");
        target=id;
    }
    if(!target.empty())return true;
    return npc_saved_identity_runtime::bind_local(object,uid,target,error);
}

bool quest_target_before(Memory& memory,uintptr_t image,uintptr_t region,
                         const npc_census::State& census,uint64_t uid,
                         quest_bridge::quest::State& quests,std::string& digest,std::string& error) {
    quests={};digest.clear();
    const auto row=std::find_if(census.rows.begin(),census.rows.end(),
        [uid](const auto& item){return item.local_uid==uid;});
    if(row==census.rows.end())return fail(error,"NPC Remove Quest target absent from fresh Manager census");
    if(row->type!=0)return true;
    int32_t saved_region=-1;
    if(!field(memory,region,0x834,saved_region,error)||saved_region<0)
        return fail(error,error.empty()?"NPC Remove Quest saved Region ID invalid":error);
    return quest_bridge::records(memory,image,saved_region,*row,quests,error)&&
        quest_bridge::pool(saved_region,digest,error);
}
bool state_owners_impl(const action::Context& context,const action::Binding& binding,
                  std::vector<StateOwner>& output,std::string& error) {
    if(!state.ready||GetCurrentThreadId()!=state.thread||!state.image||
       !binding.native_epoch||!binding.native_generation||context.snapshot_uuid.empty())
        return fail(error,"NPC Hoststate binder requires ready Mainthread, Context and World binding");
    action::Context actual;
    if(!action::selected_session().current(actual,error))return false;
    if(actual!=context||context.snapshot_uuid!=net::current_snapshot_id())
        return fail(error,"NPC Hoststate current Session or snapshot differs");
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world,after_world;
    if(!reader.current(world,false,error)||!bound(world,binding,context.snapshot_uuid,error))return false;
    using Region=std::pair<uintptr_t,npc_census::State>;
    std::map<int32_t,Region> regions;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){
        return field(memory,base,offset,value,cause);};
    const auto fresh=[&](const std::string& id,const Operation& op,StateOwner& owner,
                         std::string& cause) {
        auto region=regions.find(op.request.npc.region);
        if(region==regions.end()){
            uintptr_t pointer=0;npc_census::State census;
            if(!region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
                    op.request.npc.region,0,pointer,read_field,cause)||
               !npc_census::read(memory,state.image,pointer,census,cause))return false;
            region=regions.emplace(op.request.npc.region,Region{pointer,std::move(census)}).first;
        }
        uintptr_t registered=0;
        const auto seh=lookup(api.find,op.local_uid,registered);
        if(seh||registered!=op.local_pointer)
            return fail(cause,"NPC Hoststate Place UID lookup differs SEH="+std::to_string(seh));
        const auto& census=region->second.second;
        const auto row=std::find_if(census.rows.begin(),census.rows.end(),[&](const auto& value){
            return value.local_uid==op.local_uid;});
        if(row==census.rows.end()||row->local_pointer!=op.local_pointer||
           row->type!=op.request.npc.type||
           row->character_slot!=op.request.npc.character_slot||
           row->character_type!=op.request.npc.character_type)
            return fail(cause,"NPC Hoststate Place Manager row, class, type or CharacterTypeID differs");
        owner.key="action:"+id;owner.npc=row->local_pointer;
        if(row->type==0){
            if(!host_binding::quest_owners(memory,state.image,row->local_pointer,
                    row->quest_count,owner.quests,cause))return false;
        }else if(row->quest_count)
            return fail(cause,"NPC Hoststate non-QuestGiver has Quest records");
        cause.clear();return true;
    };
    std::vector<StateOwner> owners;
    if(!host_binding::collect(state.operations,binding,context.snapshot_uuid,fresh,owners,error))return false;
    std::vector<npc_saved_identity_runtime::Current> saved;
    if(!npc_saved_identity_runtime::read_current(saved,error))return false;
    for(const auto& item:saved){
        if(std::any_of(owners.begin(),owners.end(),[&](const auto& owner){return owner.key==item.key||owner.npc==item.row.local_pointer;}))
            return fail(error,"NPC Hoststate saved/action identity overlaps");
        StateOwner owner;owner.key=item.key;owner.npc=item.row.local_pointer;
        if(item.row.type==0){
            if(!host_binding::quest_owners(memory,state.image,owner.npc,item.row.quest_count,owner.quests,error))return false;
        }else if(item.row.quest_count!=0)return fail(error,"NPC Hoststate saved non-QuestGiver owns Quests");
        owners.push_back(std::move(owner));
    }
    std::sort(owners.begin(),owners.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    if(!reader.current(after_world,false,error))return false;
    if(!host::same_world(world.origin,after_world.origin)||world.root!=after_world.root||
       !bound(after_world,binding,context.snapshot_uuid,error))
        return fail(error,error.empty()?"NPC Hoststate World changed during owner read":error);
    output=std::move(owners);error.clear();return true;
}
uint32_t destroy_prop(uintptr_t image,uintptr_t manager,uintptr_t object)noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x2d5520)(
        reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t select_default_tool(uintptr_t image,uintptr_t cursor)noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x5854e0)(
        reinterpret_cast<void*>(cursor),nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool prepare_info_selection(Memory& memory,uintptr_t image,json& probe,std::string& error) {
    return npc_remove_selection::prepare_info_selection_body(image,probe,error,
        [&](uintptr_t base,size_t offset,auto& value,std::string& cause){
            return field(memory,base,offset,value,cause);},
        [&](uintptr_t current,std::string& cause){
            std::map<std::string,Tool> inventory;
            if(!tools(memory,image,inventory,cause))return false;
            const auto active=std::find_if(inventory.begin(),inventory.end(),
                [current](const auto& item){return item.second.count==1&&item.second.behaviour==current&&
                    quest_bridge::tool_type(item.first)>=0;});
            if(active!=inventory.end())return true;
            return fail(cause,"NPC Remove current cursor is not one original NPC tool");},
        [](uintptr_t module,uintptr_t cursor){return select_default_tool(module,cursor);});
}
bool live_npc_window(Memory& memory,uintptr_t image,const ui::LocalBinding& binding,
                     action::ui_diagnostic::Window& window,json& probe,std::string& error) {
    window={};probe=json::object();uintptr_t focused=0,ap=0,command=0,weak_target=0;
    uintptr_t secondary_handler=0,expected_proxy=0;int32_t selected_count=0;uint8_t visible=0;
    const auto reject=[&](const char* stage,const char* cause){probe["stage"]=stage;return fail(error,cause);};
    if(!field(memory,binding.manager,0x30,selected_count,error))return false;
    probe["selected_count"]=selected_count;
    if(!field(memory,binding.manager,0x80,focused,error))return false;
    probe["focused_matches_target"]=focused==binding.object;
    if(selected_count!=1||focused!=binding.object)
        return reject("selection","NPC original single selection or focus differs");
    if(!field(memory,binding.manager,0xb8,window.handler,error)||
       !field(memory,binding.manager,0xc0,secondary_handler,error))return false;
    probe["selected_handler_present"]=window.handler!=0;
    probe["other_handler_present"]=secondary_handler!=0;
    if(!window.handler)return reject("selected_handler","NPC original single-selection InfoWindow handler absent");
    if(!field(memory,window.handler,0,ap,error))return false;
    probe["selected_handler_class_matches"]=ap==image+0x1731820;
    if(ap!=image+0x1731820)return reject("selected_handler_class","NPC original InfoWindow handler class differs");
    if(!field(memory,window.handler,0x10,window.window,error))return false;
    probe["window_present"]=window.window!=0;
    if(!window.window)return reject("window","NPC original InfoWindow absent after SetInfoWindow");
    if(!field(memory,window.window,0,ap,error))return false;
    probe["window_class_matches"]=ap==image+0x1707d20;
    if(ap!=image+0x1707d20)return reject("window_class","NPC original InfoWindow class differs");
    if(!field(memory,ap,0x168,command,error))return false;
    probe["destroy_command_matches"]=command==image+0x759ad0;
    if(command!=image+0x759ad0)return reject("command","NPC original DestroyNPC command slot differs");
    if(!field(memory,window.window,0x45f,visible,error))return false;
    probe["visible"]=visible;
    if(visible!=1)return reject("visible","NPC original InfoWindow is not visible");
    if(!field(memory,window.window,0x550,window.target,error))return false;
    probe["window_target_matches"]=window.target==binding.object;
    if(window.target!=binding.object)return reject("window_target","NPC original InfoWindow target differs");
    if(!field(memory,binding.object,0x28,expected_proxy,error)||
       !field(memory,window.window,0x558,window.proxy,error))return false;
    probe["npc_weak_present"]=expected_proxy!=0;
    probe["window_weak_matches_npc"]=window.proxy==expected_proxy;
    if(!expected_proxy||window.proxy!=expected_proxy)
        return reject("weak_binding","NPC original InfoWindow weak binding differs from NPC+0x28");
    if(!field(memory,window.proxy,0,weak_target,error))return false;
    probe["weak_target_matches_npc"]=weak_target==binding.object+0x20;
    if(weak_target!=binding.object+0x20)
        return reject("weak_target","NPC original InfoWindow weak target differs from NPC+0x20");
    probe["stage"]="confirmed";error.clear();return true;
}
} // namespace

bool local_key(uintptr_t object,uint64_t uid,std::string& key,std::string& error) {
    key.clear();if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"NPC identity query requires ready Mainthread");
    std::string target;if(!local_target_key(object,uid,target,error))return false;
    key=npc_target_key::canonical(target);return true;
}
bool state_owners(const action::Context& context,const action::Binding& binding,
                  std::vector<StateOwner>& output,std::string& error) {
    return state_owners_impl(context,binding,output,error);
}
bool resolve_state_link(const std::vector<StateOwner>& owners,const std::string& key,
                        int32_t sequence_id,uintptr_t& npc,uintptr_t& quest,std::string& error) {
    return host_binding::resolve(owners,key,sequence_id,npc,quest,error);
}

bool authorize_remove_replay(const ui::Replay& replay,std::string& error);
bool remove_local_binding(uintptr_t manager,uintptr_t object,ui::LocalBinding& out,std::string& error) {
    out={};if(!state.ready||GetCurrentThreadId()!=state.thread||!manager||!object)
        return fail(error,"NPC Remove capture requires ready Mainthread target");
    if(state.remove_replay)return npc_remove_replay_binding::resolve_private(
        *state.remove_replay,manager,object,out,error,
        [&](const ui::Replay& replay,std::string& cause){return authorize_remove_replay(replay,cause);},
        [&](const std::string& id)->const Operation* {
            const auto found=state.operations.find(id);
            return found==state.operations.end()?nullptr:&found->second;},
        [&](const Operation& operation,ui::LocalBinding& fresh,std::string& cause){
            uintptr_t region=0;npc_census::State census;
            return target_binding(npc_target_key::argument(operation.request.canonical_record_key),operation.binding,
                operation.request.snapshot_uuid,fresh,region,census,cause);});
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    Memory target_memory;uint64_t uid=0;std::string target;
    if(!field(target_memory,object,0x18,uid,error)||!local_target_key(object,uid,target,error))return false;
    uintptr_t region=0;npc_census::State census;
    if(!target_binding(target,binding,snapshot,out,region,census,error))return false;
    if(out.manager!=manager||out.object!=object)
        return fail(error,"NPC Remove original SelectionManager or object differs");
    if(std::any_of(census.rows.begin(),census.rows.end(),[&](const auto& row){return row.local_uid==out.uid&&row.type==0;})){
        Memory memory;quest_bridge::quest::State quests;std::string digest;
        if(!quest_target_before(memory,state.image,region,census,out.uid,quests,digest,error))return false;
    }
    return true;
}
bool authorize_remove_replay(const ui::Replay& replay,std::string& error) {
    if(!state.remove_replay||state.remove_replay->purpose!=replay.purpose||
       state.remove_replay->intent.action_uuid!=replay.intent.action_uuid||
       state.remove_replay->intent.snapshot_uuid!=replay.intent.snapshot_uuid||
       state.remove_replay->intent.canonical_record_key!=replay.intent.canonical_record_key||
       state.remove_replay->binding!=replay.binding||!replay.binding.supported_npc||
       !state.ready||GetCurrentThreadId()!=state.thread||
       (replay.purpose!=ui::ReplayPurpose::HostExecution&&
        replay.purpose!=ui::ReplayPurpose::ConfirmedHostApplication))
        return fail(error,"NPC Remove lacks exact private replay scope");
    const auto controller=typed_action_runtime::controller_status();
    const auto operation=state.operations.find(replay.intent.action_uuid);
    if(operation==state.operations.end()||
       operation->second.request.action!=action::Action::RemoveNPC||
       operation->second.request.snapshot_uuid!=replay.intent.snapshot_uuid||
       operation->second.request.canonical_record_key!=replay.intent.canonical_record_key||
       !npc_target_key::request(replay.intent.canonical_record_key,operation->second.request.target_action_uuid)||
       operation->second.binding.native_epoch!=replay.binding.native_epoch||
       operation->second.binding.native_generation!=replay.binding.native_generation||
       operation->second.local_uid!=replay.binding.uid||
       operation->second.local_pointer!=replay.binding.object)
        return fail(error,"NPC Remove replay action, Place key or native target differs");
    if(!controller.value("started",false)||controller.value("locked",true)||
       !controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=replay.intent.action_uuid||
       replay.intent.snapshot_uuid!=net::current_snapshot_id()||
       (replay.purpose==ui::ReplayPurpose::HostExecution)!=Config::instance().is_host)
        return fail(error,"NPC Remove replay Controller, World or role differs");
    return true;
}
void remove_submitted(const std::string& id,const std::string& target) {
    ++state.captures;++state.submits;state.last_action=id;
    if(state.diagnostic_token.empty())return;
    const auto job=state.ui_jobs.find(state.diagnostic_token);
    if(job==state.ui_jobs.end()||job->second.value("canonical_record_key",std::string{})!=npc_target_key::canonical(target)){
        rejected("NPC Remove diagnostic token target differs after Controller submit");return;}
    job->second["captured"]=true;job->second["submitted"]=true;job->second["action_uuid"]=id;
}

bool native_remove_ui(const json& args,json& result,std::string& error) {
    result=json::object();
    if(!args.is_object()||args.size()!=2||!args.contains("token")||
       args.contains("target_action_uuid")==args.contains("record_key")||!args.at("token").is_string()||
       !args.at(args.contains("record_key")?"record_key":"target_action_uuid").is_string())
        return fail(error,"NPC Remove UI requires exact token and target_action_uuid");
    const auto token=args.at("token").get<std::string>();
    const auto target=args.at(args.contains("record_key")?"record_key":"target_action_uuid").get<std::string>();
    if(!token_valid(token)||!npc_target_key::input(target)||
       (args.contains("record_key")&&!snapshot_npc_key::valid(target))||token==target||
       state.ui_jobs.contains(token)||state.ui_jobs.size()>=64)
        return fail(error,"NPC Remove UI token or target invalid/used");
    result={{"operation","remove"},{"token",token},{"target_action_uuid",snapshot_npc_key::valid(target)?std::string{}:target},
        {"canonical_record_key",npc_target_key::canonical(target)},
        {"attempted",false},{"returned",false},{"captured",false},
        {"submitted",false},{"terminal",false},{"confirmed",false}};
    if(!state.ready||GetCurrentThreadId()!=state.thread||!ui_remove_runtime::enabled()||
       !net::session_wire::connected()||state.pending||state.capture_failed)
        return fail(error,"NPC Remove UI requires ready connected idle Mainthread capture");
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"NPC Remove UI requires idle bound Controller");
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    if(!target_binding(target,binding,snapshot,local,region,census,error))return false;
    Memory memory;action::NpcState before;action::Ledger money;undo::UndoWitness undo_before;
    const bool quest_target=census.rows.end()!=std::find_if(census.rows.begin(),census.rows.end(),
        [&](const auto& row){return row.local_uid==local.uid&&row.type==0;});
    json before_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,before,money,undo_before,census,error,
                    quest_target,&before_tasks)||
       undo_before.active_graph)return fail(error,error.empty()?"NPC Remove UI requires idle Undo":error);
    action::ui_diagnostic::Api api;
    if(!action::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    std::array<uint8_t,8> prologue{};
    constexpr std::array<uint8_t,8> expected{0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53};
    uintptr_t command=0;
    if(!memory.copy(state.image+0x759ad0,prologue.data(),prologue.size(),error)||prologue!=expected||
       !field(memory,state.image+0x1707d20,0x168,command,error)||command!=state.image+0x759ad0)
        return fail(error,error.empty()?"NPC exact InfoWindow UICommand bytes or vtable differ":error);
    api.command=reinterpret_cast<action::ui_diagnostic::UiCommand>(state.image+0x759ad0);
    api.destroy_label="DestroyNPC";
    action::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,
        binding.native_epoch,binding.native_generation,exe_sha,action::ui_diagnostic::Family::Building};
    action::ui_diagnostic::Receipt receipt;
    state.ui_jobs.emplace(token,result);auto& job=state.ui_jobs.at(token);
    job["attempted"]=true;result=job;
    const auto before_capture=ui_remove_runtime::status().value("captured",uint64_t{});
    auto& calls=action::ui_diagnostic::process_calls();
    json tool_probe;
    if(!prepare_info_selection(memory,state.image,tool_probe,error)){
        job["tool_probe"]=tool_probe;job["cause"]=error;
        job["unknown"]=tool_probe.value("tool_switch_attempted",false);result=job;return false;}
    job["tool_probe"]=tool_probe;
    if(!calls.select(api,context,receipt,error)){
        job["cause"]=error;job["unknown"]=receipt.selection_attempted;result=job;return false;}
    action::ui_diagnostic::Window window;
    npc_census::State selected_census;action::NpcState selected;
    action::Ledger selected_money;undo::UndoWitness selected_undo;
    json window_probe,selected_tasks=json::array();
    const bool window_live=live_npc_window(memory,state.image,local,window,window_probe,error);
    job["window_probe"]=window_probe;
    if(!window_live||
       !read_effect(memory,state.image,region,local.uid,selected,selected_money,selected_undo,selected_census,error,
                    quest_target,&selected_tasks)||
       selected!=before||selected_money!=money||selected_undo!=undo_before||selected_tasks!=before_tasks){
        job["cause"]=error.empty()?"NPC original Select/Info changed actor, RNG, Undo or Ledger":error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    job["window_confirmed"]=true;
    state.diagnostic_token=token;
    const bool returned=calls.command(api,window,receipt,error);
    state.diagnostic_token.clear();
    job["returned"]=receipt.ui_command_returned;
    job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;
    const auto after_capture=ui_remove_runtime::status();
    if(!returned||after_capture.value("captured",uint64_t{})!=before_capture+1||
       after_capture.value("last_origin",uint8_t{})!=uint8_t(ui::Origin::NpcWindow)||
       !job.value("captured",false)||!job.value("submitted",false)){
        job["cause"]=error.empty()?"NPC original DestroyNPC command lacks exact typed capture":error;
        job["unknown"]=receipt.ui_command_attempted;result=job;
        return fail(error,job.at("cause").get<std::string>());}
    npc_census::State after_census;action::NpcState after;action::Ledger after_money;undo::UndoWitness after_undo;
    json after_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,after,after_money,after_undo,after_census,error,
                    quest_target,&after_tasks)||
       after!=before||after_money!=money||after_undo!=undo_before||after_tasks!=before_tasks){
        job["cause"]=error.empty()?"NPC Client UI changed world before Host authority":error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    result=job;error.clear();return true;
}

action::Outcome remove_perform(const action::Binding& binding,const action::Request& request,
                               const action::Evidence* authority,std::string& error) {
    if(!state.ready||GetCurrentThreadId()!=state.thread||request.action!=action::Action::RemoveNPC||
       !ui_remove_runtime::enabled()){
        error="NPC Remove native hook, Mainthread or typed request invalid";return action::Outcome::RejectedNoEffect;}
    if(state.operations.contains(request.action_uuid)){
        error="NPC Remove action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||
       !npc_target_key::request(request.canonical_record_key,request.target_action_uuid)){
        error="NPC Remove lacks exact pending Controller or Place key";return action::Outcome::RejectedNoEffect;}
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    if(!target_binding(npc_target_key::argument(request.canonical_record_key),binding,request.snapshot_uuid,local,region,census,error))
        return action::Outcome::RejectedNoEffect;
    Memory memory;action::NpcState before;action::Ledger money;undo::UndoWitness undo_before;
    int32_t target_type=-1;
    {
        const auto target_row=std::find_if(census.rows.begin(),census.rows.end(),[&](const auto& row){return row.local_uid==local.uid;});
        if(target_row==census.rows.end()){
            error="NPC Remove target absent from fresh before census";return action::Outcome::RejectedNoEffect;}
        target_type=target_row->type;
    }
    const bool quest_target=target_type==0;
    json before_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,before,money,undo_before,census,error,
                    quest_target,&before_tasks)||
       undo_before.active_graph)return action::Outcome::RejectedNoEffect;
    if(authority&&(before.count!=authority->npc_before.count||
        before.roster_sha256!=authority->npc_before.roster_sha256||
        before.undo_count!=authority->npc_before.undo_count||
        before.created!=authority->npc_before.created||
        before.quest_targets_sha256!=authority->npc_before.quest_targets_sha256||money!=authority->before)){
        error="NPC Remove Client before actor/roster/Undo/Ledger differs from Host";
        return action::Outcome::RejectedNoEffect;}
    auto& op=state.operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    op.target_action_uuid=request.target_action_uuid;op.local_uid=local.uid;op.local_pointer=local.object;
    op.target_type=target_type;
    if(!field(memory,region,0x834,op.target_region,error))return action::Outcome::RejectedNoEffect;
    auto unknown=[&](const std::string& cause){error=op.cause=cause;return action::Outcome::Unknown;};
    if(authority){const auto seh=seed(state.image,authority->npc_before.global_random);
        if(seh)return unknown("NPC Remove Client Host RNG adoption SEH="+std::to_string(seh));
        if(!read_effect(memory,state.image,region,local.uid,before,money,undo_before,census,error,
                        quest_target,&before_tasks)||
           before!=authority->npc_before||money!=authority->before)
            return unknown(error.empty()?"NPC Remove Client Host before RNG/actor differs":error);}
    op.evidence.before=money;op.evidence.npc_before=before;op.quest_tasks_before=before_tasks;
    action::ui_diagnostic::Api api;
    if(!action::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);
    action::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,
        binding.native_epoch,binding.native_generation,exe_sha,action::ui_diagnostic::Family::Building};
    action::ui_diagnostic::Receipt receipt;
    op.attempted|=1;
    json tool_probe;
    if(!prepare_info_selection(memory,state.image,tool_probe,error))return unknown(error);
    if(!action::ui_diagnostic::process_calls().select(api,context,receipt,error))return unknown(error);
    op.returned|=1;
    action::NpcState selected;action::Ledger selected_money;undo::UndoWitness selected_undo;
    npc_census::State selected_census;json selected_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,selected,selected_money,selected_undo,selected_census,error,
                    quest_target,&selected_tasks)||
       selected!=before||selected_money!=money||selected_undo!=undo_before||selected_tasks!=before_tasks)
        return unknown(error.empty()?"NPC Remove original Select/Info changed actor, Undo or Ledger":error);
    if(quest_target){
        quest_bridge::quest::State original_quests;std::string pool_digest;
        if(!quest_target_before(memory,state.image,region,selected_census,local.uid,
                                original_quests,pool_digest,error)||
           original_quests.records!=selected.created.quests||
           pool_digest!=selected.quest_targets_sha256)
            return unknown(error.empty()?"NPC Remove original Quest records or pool changed before DestroyProps":error);
        for(size_t index=0;index<original_quests.local.size();++index){
            op.remove_quest_pointers.push_back(original_quests.local[index].quest);
            op.remove_quest_sequence_ids.push_back(original_quests.records[index].sequence_id);
        }
    }
    ui::Replay replay{authority?ui::ReplayPurpose::ConfirmedHostApplication:ui::ReplayPurpose::HostExecution,
                      {request.action_uuid,request.snapshot_uuid,request.canonical_record_key},local};
    const auto old_replays=ui_remove_runtime::status().value("replay_originals",uint64_t{});
    if(state.remove_replay)return unknown("NPC Remove private replay is reentrant");
    op.attempted|=2;bool performed=false;
    {
        state.remove_replay=&replay;
        struct Reset {~Reset(){state.remove_replay=nullptr;}} reset;
        performed=ui_remove_runtime::with_replay(replay,[&](std::string& cause){
            const auto seh=destroy_prop(state.image,local.manager,local.object);
            if(seh)return fail(cause,"NPC original DestroyProp SEH="+std::to_string(seh));
            return true;},error);
    }
    const auto new_replays=ui_remove_runtime::status().value("replay_originals",uint64_t{});
    op.destroyprops_original_calls=new_replays>=old_replays?uint32_t(new_replays-old_replays):0;
    if(!performed)return unknown(error.empty()?"NPC original DestroyProps replay did not confirm exactly once":error);
    op.returned|=2;
    if(op.destroyprops_original_calls!=1)return unknown("NPC original DestroyProps call count differs");
    action::NpcState after;action::Ledger after_money;undo::UndoWitness undo_after;
    npc_census::State after_census;
    if(!read_effect(memory,state.image,region,0,after,after_money,undo_after,after_census,error,
                    quest_target,&op.quest_tasks_after)||
       !remove_effect::removed_one(selected_census,after_census,local.uid,error))
        return unknown(error.empty()?"NPC original DestroyProps Manager readback failed":error);
    if(quest_target&&
       !quest_bridge::removed_quests(memory,state.image,op.remove_quest_pointers,
                                    op.remove_quest_sequence_ids,op.quest_retirement,error))
        return unknown(error);
    op.target_absent=true;
    if(!selection_absent(memory,state.image,local.manager,local.object,error))return unknown(error);
    op.selection_cleared=true;
    if(undo_after.undo.empty())return unknown("NPC original DestroyProps committed Undo graph absent");
    undo::GraphWitness graph;
    if(!undo::read_graph(memory.process,state.image,undo_after.undo.back(),graph,error)||
       !undo::validate_undo_commit(selected_undo,undo_after,graph,error)||graph.operations<1)
        return unknown(error.empty()?"NPC original DestroyProps Undo graph invalid":error);
    op.undo_confirmed=true;
    op.evidence.after=after_money;op.evidence.npc_after=after;
    op.evidence.undo_confirmed=true;op.evidence.target_retired=true;op.evidence.selection_confirmed=true;
    if(op.attempted!=op.returned||after_money!=money||
       !action::valid_npc_effect(request,op.evidence)||
       (authority&&(op.evidence.before!=authority->before||op.evidence.after!=authority->after||
                    op.evidence.npc_before!=authority->npc_before||op.evidence.npc_after!=authority->npc_after)))
        return unknown("NPC original Remove actor, Host state or full Ledger differs");
    if(!request.target_action_uuid.empty())state.operations.at(request.target_action_uuid).target_absent=true;op.confirmed=true;error.clear();return action::Outcome::Confirmed;
}
bool remove_observe(const action::Binding& binding,const action::Request& request,
                    action::Evidence& out,std::string& error) {
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||found->second.request!=request||
       found->second.binding!=binding||!found->second.confirmed||
       !found->second.target_absent||!found->second.selection_cleared||
       found->second.destroyprops_original_calls!=1)
        return fail(error,"NPC Remove observation lacks exact confirmed OriginalOnce operation");
    const auto& op=found->second;
    if(!request.target_action_uuid.empty()){
        const auto target=state.operations.find(request.target_action_uuid);
        if(target==state.operations.end()||!target->second.target_absent)
            return fail(error,"NPC Remove Place binding was not retired");
    }
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world;uintptr_t region=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    if(!reader.current(world,true,error)||!bound(world,binding,request.snapshot_uuid,error)||
       !region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
           op.target_region,0,region,read_field,error))return false;
    action::NpcState current;action::Ledger money;undo::UndoWitness witness;npc_census::State census;
    uintptr_t manager=0;
    const bool quest_target=op.target_type==0;
    json current_tasks=json::array();
    if(!field(memory,state.image,0x13df630,manager,error)||!manager||
       !read_effect(memory,state.image,region,0,current,money,witness,census,error,
                    quest_target,&current_tasks)||
       current!=op.evidence.npc_after||money!=op.evidence.after||
       current_tasks!=op.quest_tasks_after||
       !selection_absent(memory,state.image,manager,op.local_pointer,error))
        return fail(error,error.empty()?"NPC Remove current Manager, Undo, selection or Ledger differs":error);
    if(quest_target){json retirement;
        if(!quest_bridge::removed_quests(memory,state.image,op.remove_quest_pointers,
                                        op.remove_quest_sequence_ids,retirement,error)||
           retirement!=op.quest_retirement)
            return fail(error,error.empty()?"NPC Remove Quest retirement readback differs":error);}
    out=op.evidence;out.retirement_confirmed=op.retired;error.clear();return true;
}
bool remove_retire(const action::Binding& binding,const action::Request& request,std::string& error) {
    action::Evidence observed;if(!remove_observe(binding,request,observed,error))return false;
    auto& op=state.operations.at(request.action_uuid);
    if(op.retired)return fail(error,"NPC Remove already retired");
    op.retired=true;error.clear();return true;
}
