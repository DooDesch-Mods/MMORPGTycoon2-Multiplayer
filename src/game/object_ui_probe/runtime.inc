namespace object_ui {
namespace {
namespace probe_contract=mt2mp::game::object_ui_probe;
namespace common=mt2mp::action_session::ui_diagnostic;
namespace capture=mt2mp::action_session::ui_capture;
struct ProbeMemory final:capture::Memory {
    Runtime& runtime;explicit ProbeMemory(Runtime& value):runtime(value){}
    bool copy(uintptr_t address,void* output,size_t size,std::string& error)override{return runtime.memory.read(address,output,size,error);}
    bool region(uintptr_t address,capture::Region& out,std::string& error)override{
        MEMORY_BASIC_INFORMATION page{};if(VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof page)!=sizeof page)return probe_contract::fail(error,"object_ui_native_page_query_failed Win32="+std::to_string(GetLastError()));
        const auto protect=page.Protect&0xff;out={uintptr_t(page.AllocationBase),page.State==MEM_COMMIT,!(page.Protect&PAGE_GUARD)&&(protect==PAGE_EXECUTE||protect==PAGE_EXECUTE_READ||protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY),page.Type==MEM_IMAGE,protect==PAGE_READWRITE||protect==PAGE_WRITECOPY||protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY};return true;
    }
};
struct Job {
    probe_contract::Input input;action::Binding binding;host::WorldState world;
    std::vector<duplicate_native::Handle> selected;std::set<std::string> prior_operations;action::Ledger money;
    uint64_t captures=0,submitted=0,rejected=0,revision=0,started_at=0;
    uintptr_t behaviour=0,cursor=0;std::string action_uuid,phase="preflight",cause;
    uint32_t attempted_mask=0,returned_mask=0,exception=0,cleanup_exception=0;
    bool command_accepted=false,preparation_reconciled=false,observation_reconciled=false;
    std::string reconciliation_cause,prior_observation_cause;json result=json::object();
};
std::map<std::string,Job> jobs;
bool terminal(const Job& job){return job.phase=="confirmed"||job.phase=="rejected";}
bool stopped(Job& job,const std::string& cause,std::string& error){job.cause=cause;job.phase=job.attempted_mask?"unknown":"rejected";LOG_ERROR("[ObjectUIProbe] token={} command={} action={} cause={}; no replay",job.input.token,job.input.command,job.action_uuid,cause);return probe_contract::fail(error,cause);}
json format(const Job& job){return {{"acceptance_token",job.input.token},{"snapshot_uuid",job.input.snapshot},{"command",job.input.command},{"record_keys",job.input.keys},{"phase",job.phase},{"confirmed",job.phase=="confirmed"},{"unknown",job.phase=="unknown"},{"attempted_mask",job.attempted_mask},{"returned_mask",job.returned_mask},{"exception",job.exception},{"cleanup_exception",job.cleanup_exception},{"command_attempted",bool(job.attempted_mask&16)},{"preparation_reconciled",job.preparation_reconciled},{"reconciliation_cause",job.reconciliation_cause},{"observation_reconciled",job.observation_reconciled},{"prior_observation_cause",job.prior_observation_cause},{"command_accepted",job.command_accepted},{"action_uuid",job.action_uuid},{"cause",job.cause},{"result",job.result}};}
bool current(Runtime& runtime,const Job& job,std::string& error){
    action::Binding binding;host::WorldState value;
    if(!runtime.ready||runtime.thread!=GetCurrentThreadId()||!net::session_wire::connected()||net::current_snapshot_id()!=job.input.snapshot||
       !typed_action_runtime::read_binding(job.input.snapshot,binding,error)||binding!=job.binding||!world(runtime,value,error)||!host::same_world(value.origin,job.world.origin)||value.selection!=job.world.selection||value.root!=job.world.root)
        return probe_contract::fail(error,error.empty()?"object_ui_current_session_world_binding_differs":error);
    return true;
}
bool selection_ready(Runtime& runtime,const Job& job,std::string& error){
    auto& memory=runtime.memory;const auto image=runtime.image;uintptr_t cursor=0,behaviour=0;uint8_t allowed=0,setting=0,queued=0;
    if(!group_reader::q::at(memory,image,0x13df9d0,cursor,error)||!group_reader::q::typed(memory,image,cursor,0x1786dd0,error)||
       !group_reader::q::at(memory,cursor,0x190,behaviour,error)||!group_reader::q::at(memory,behaviour,0x120,allowed,error)||
       !group_reader::q::at(memory,job.world.selection,0xd1,setting,error)||!group_reader::q::at(memory,job.world.selection,0xd3,queued,error))return false;
    return (allowed==1&&!setting&&!queued)||probe_contract::fail(error,"object_ui_original_selection_not_allowed_cancel_tool_first");
}
bool reconcile_preparation(Runtime& runtime,Job& job,std::string& error){
    if(!job.attempted_mask||(job.attempted_mask&~uint32_t(3))||job.returned_mask!=job.attempted_mask||
       job.exception||job.cleanup_exception||job.command_accepted||!job.action_uuid.empty())return false;
    action::Ledger actual;if(!current(runtime,job,error)||!ledger(actual,error))return false;
    const bool duplicate=job.input.command=="Duplicate";
    const uint64_t captures=duplicate?duplicate_edit::captures:group_edit::captures,submitted=duplicate?duplicate_edit::submitted:group_edit::submitted,rejected=duplicate?duplicate_edit::rejections:group_edit::rejections;
    std::set<std::string> operations;
    if(duplicate){for(const auto& [id,value]:duplicate_edit::operations){(void)value;operations.insert(id);}}
    else{for(const auto& [id,value]:group_edit::operations){(void)value;operations.insert(id);}}
    const auto controller=typed_action_runtime::controller_status();
    if(actual!=job.money||captures!=job.captures||submitted!=job.submitted||rejected!=job.rejected||operations!=job.prior_operations||
       controller.value("host_revision",uint64_t(0))!=job.revision||controller.value("pending",true)||controller.value("locked",true))
        return probe_contract::fail(error,"object_ui_preparation_only_readback_changed_no_replay");
    job.phase="rejected";job.preparation_reconciled=true;error.clear();return true;
}
bool reconcile_rejection(Runtime& runtime,Job& job,std::string& error){
    if(job.input.command!="Duplicate"||job.attempted_mask!=511||job.returned_mask!=511||
       job.exception||job.cleanup_exception||!job.command_accepted)return false;
    const auto source=duplicate_edit::status().at("last_submission");if(source.is_null())return false;
    if(job.captures==UINT64_MAX||job.submitted==UINT64_MAX||job.revision==UINT64_MAX)
        return probe_contract::fail(error,"object_ui_rejection_counter_exhausted");
    const auto id=source.at("action_uuid").get<std::string>();
    if(!probe_contract::uuid(id)||(!job.action_uuid.empty()&&job.action_uuid!=id)||job.prior_operations.contains(id)||
       !source.at("submitted").get<bool>()||source.at("snapshot_uuid").get<std::string>()!=job.input.snapshot||
       source.at("selected_keys").get<std::vector<std::string>>()!=job.input.keys||
       source.at("native_epoch").get<uint64_t>()!=job.binding.native_epoch||
       source.at("native_generation").get<uint64_t>()!=job.binding.native_generation||
       source.at("capture_count").get<uint64_t>()!=job.captures+1||source.at("submission_count").get<uint64_t>()!=job.submitted+1)
        return probe_contract::fail(error,"object_ui_rejection_source_identity_differs");
    std::set<std::string> operations;for(const auto& [operation_id,value]:duplicate_edit::operations){(void)value;operations.insert(operation_id);}
    if(duplicate_edit::captures!=job.captures+1||duplicate_edit::submitted!=job.submitted+1||duplicate_edit::rejections!=job.rejected||operations!=job.prior_operations)
        return probe_contract::fail(error,"object_ui_rejection_capture_or_native_operation_differs");
    auto controller=typed_action_runtime::controller_status();
    if(controller.value("pending",true)||controller.value("locked",true)||controller.value("outcome",0)!=int(action::Outcome::RejectedNoEffect))return false;
    if(controller.value("action_uuid",std::string{})!=id||controller.value("host_revision",uint64_t(0))!=job.revision+1||controller.value("cause",std::string{}).empty())
        return probe_contract::fail(error,"object_ui_rejection_authoritative_completion_differs");
    action::Ledger actual;if(!current(runtime,job,error)||!ledger(actual,error))return false;
    if(actual!=job.money)return probe_contract::fail(error,"object_ui_rejection_current_ledger_differs");
    const auto final=typed_action_runtime::controller_status();
    if(final.value("pending",true)||final.value("locked",true)||final.value("outcome",0)!=int(action::Outcome::RejectedNoEffect)||
       final.value("action_uuid",std::string{})!=id||final.value("host_revision",uint64_t(0))!=job.revision+1||
       final.value("cause",std::string{})!=controller.value("cause",std::string{}))
        return probe_contract::fail(error,"object_ui_rejection_completion_changed_during_readback");
    job.result={{"rejected_no_effect",true},{"source_submission",source},{"prior_observation_cause",job.cause}};
    job.action_uuid=id;job.cause=controller.at("cause").get<std::string>();job.phase="rejected";job.reconciliation_cause.clear();error.clear();return true;
}
uint32_t show(uintptr_t image,uintptr_t manager,const group_edit::Array* selected,const common::Api* api,uint32_t* attempted,uint32_t* returned)noexcept{
    __try{*attempted|=1;reinterpret_cast<void(*)(void*,const group_edit::Array*)>(image+0x2d5910)(reinterpret_cast<void*>(manager),selected);*returned|=1;
        *attempted|=2;api->info(reinterpret_cast<void*>(manager));*returned|=2;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t command(const common::Api* api,uintptr_t window,const char* label,Job* job)noexcept{
    common::NativeRecordStorage record{};bool constructed=false;uint32_t exception=0;
    __try{job->attempted_mask|=4;api->construct(&record,label);constructed=true;job->returned_mask|=4;
        job->attempted_mask|=8;const bool equal=api->label_equals(&record,label);job->returned_mask|=8;
        if(equal){job->attempted_mask|=16;job->command_accepted=api->command(reinterpret_cast<void*>(window),&record);job->returned_mask|=16;}}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(constructed){job->attempted_mask|=32;__try{api->destroy(&record);job->returned_mask|=32;}__except(EXCEPTION_EXECUTE_HANDLER){job->cleanup_exception=GetExceptionCode();}}
    return exception;
}
uint32_t cursor_call(uintptr_t image,uintptr_t behaviour,uintptr_t cursor,const std::array<float,3>* position,unsigned step,Job* job)noexcept{
    __try{job->attempted_mask|=uint32_t(1)<<step;const auto rva=step==6?0x5c0200:step==7?0x5c05c0:0x5c01e0;
        reinterpret_cast<void(*)(void*,void*,const float*)>(image+rva)(reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(cursor),position->data());job->returned_mask|=uint32_t(1)<<step;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool cursor(Runtime& runtime,Job& job,bool pending,std::string& error){
    auto& memory=runtime.memory;const auto image=runtime.image;uintptr_t ap=0,behaviour=0,data=0,target=0;int32_t count=0,capacity=0;uint8_t placing=0,queued=0,duplicate=0;
    if(!group_reader::q::at(memory,image,0x13df9d0,job.cursor,error)||!group_reader::q::typed(memory,image,job.cursor,0x1786dd0,error)||
       !group_reader::q::at(memory,job.cursor,0x190,behaviour,error)||!group_reader::q::at(memory,behaviour,0,ap,error)||ap!=image+0x172f1f0||
       !group_reader::q::at(memory,behaviour,0x460,data,error)||!group_reader::q::at(memory,behaviour,0x468,count,error)||!group_reader::q::at(memory,behaviour,0x46c,capacity,error)||count!=1||capacity<1||
       !group_reader::q::at(memory,data,16,target,error)||target!=job.selected.front().object||!group_reader::q::at(memory,behaviour,0x400,placing,error)||
       !group_reader::q::at(memory,behaviour,0x401,queued,error)||!group_reader::q::at(memory,behaviour,0x499,duplicate,error)||duplicate!=1||placing!=(pending?0:1)||queued!=(pending?1:0))
        return probe_contract::fail(error,error.empty()?"object_ui_duplicate_actual_cursor_source_or_flags_differ":error);
    if(job.behaviour&&job.behaviour!=behaviour)return probe_contract::fail(error,"object_ui_duplicate_cursor_changed");job.behaviour=behaviour;return true;
}
bool live_window(Runtime& runtime,const Job& job,common::Api& api,uintptr_t& window,std::string& error){
    const auto image=runtime.image,manager=job.world.selection;auto& memory=runtime.memory;std::vector<uintptr_t> raw,filtered,expected;
    for(const auto& target:job.selected)expected.push_back(target.object);
    if(!group_reader::selection(memory,image,manager,raw,filtered,error)||raw!=expected||filtered!=expected)return probe_contract::fail(error,error.empty()?"object_ui_exact_current_selection_differs":error);
    const bool group=job.input.keys.front().starts_with("group:");uintptr_t ap=0,entry=0,target=0,handler=0;uint8_t visible=0;
    if(job.input.command=="Group"||job.selected.size()>1){
        if(!group_reader::q::at(memory,manager,0xc0,window,error))return false;ap=image+0x171b260;entry=image+0x74bc00;
    }else if(group){
        if(!group_reader::q::at(memory,manager,0xb8,handler,error)||!group_reader::q::at(memory,handler,0x10,window,error)||
           !group_reader::q::at(memory,window,0x4e8,target,error)||target!=job.selected.front().object)return probe_contract::fail(error,error.empty()?"object_ui_Group_InfoWindow_target_differs":error);
        ap=image+0x1719fd0;entry=image+0x718c80;
    }else{
        remove_diagnostics::LocalTarget selected;if(!remove_diagnostics::resolve_record_target(job.input.snapshot,job.input.keys.front(),selected,error))return false;
        ProbeMemory adapter(runtime);common::Context context{image,manager,selected.object,selected.uid,job.input.epoch,job.input.generation,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",selected.normal_scenery?common::Family::Scenery:common::Family::Building};common::Window actual;
        if(!common::read_live_window(adapter,context,actual,error))return false;window=actual.window;return true;
    }
    uintptr_t actual=0,actual_entry=0;if(!group_reader::q::at(memory,window,0,actual,error)||actual!=ap||!group_reader::q::at(memory,ap,0x168,actual_entry,error)||actual_entry!=entry||
       !group_reader::q::at(memory,window,0x45f,visible,error)||visible!=1)return probe_contract::fail(error,error.empty()?"object_ui_visible_native_window_class_or_command_differs":error);
    api.command=reinterpret_cast<common::UiCommand>(entry);return true;
}
}
bool unresolved()noexcept{for(const auto& [token,job]:jobs){(void)token;if(!terminal(job))return true;}return false;}
bool probe(const json& args,json& out,std::string& error){Job* active=nullptr;try{
    out=json::object();probe_contract::Input input;if(!probe_contract::parse(args,input,error))return false;
    if(jobs.contains(input.token))return probe_contract::fail(error,"object_ui_token_already_attempted_no_replay");if(unresolved())return probe_contract::fail(error,"object_ui_previous_effect_unresolved");
    auto* runtime=published.load(std::memory_order_acquire);if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId())return probe_contract::fail(error,"object_ui_ready_Mainthread_required");
    auto& job=jobs.emplace(input.token,Job{}).first->second;active=&job;job.input=std::move(input);auto& r=*runtime;const auto controller=typed_action_runtime::controller_status();
    if(!net::session_wire::connected()||net::current_snapshot_id()!=job.input.snapshot||!controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)||
       status().value("pending_ui",true)||!typed_action_runtime::network_binding(job.input.snapshot,job.binding,error)||!world(r,job.world,error)||
       job.binding.native_epoch!=job.input.epoch||job.binding.native_generation!=job.input.generation)return stopped(job,error.empty()?"object_ui_idle_current_controller_required":error,error);
    duplicate_edit::Sample sample;
    const bool sampled=job.input.command=="Duplicate"?duplicate_edit::sample_duplicate(r,job.binding,nullptr,{},sample,error):
        (job.input.command=="Group"||job.input.command=="Ungroup")?duplicate_edit::sample_untouched_scenery(r,job.binding,nullptr,{},sample,error):
        duplicate_edit::sample(r,job.binding,nullptr,{},sample,error);
    if(!sampled)return stopped(job,error,error);
    common::Family family=common::Family::Building;
    for(const auto& key:job.input.keys){const auto found=sample.handles.find(key);
        if(found==sample.handles.end()||!sample.objects.at(key).parent.empty())return stopped(job,"object_ui_selected_canonical_top_level_target_absent",error);
        const auto kind=sample.objects.at(key).family;
        if(job.input.command=="Group"&&kind!=action::DuplicateFamily::Scenery&&kind!=action::DuplicateFamily::Group)return stopped(job,"object_ui_Group_requires_original_Scenery_or_Group_members",error);
        if(job.input.command=="Ungroup"&&kind!=action::DuplicateFamily::Group)return stopped(job,"object_ui_Ungroup_requires_current_Group",error);
        if(kind==action::DuplicateFamily::Scenery)family=common::Family::Scenery;job.selected.push_back(found->second);
    }
    if(job.input.command=="Group"){action::GroupIntent intent{action::GroupKind::Group,job.input.keys};group_transition::Plan plan;
        if(!group_transition::plan(sample.groups.state,intent,plan,error))return stopped(job,error,error);}
    ProbeMemory adapter(r);common::Api api;
    if(!common::resolve_api(adapter,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,error,family))return stopped(job,error,error);
    const auto check=[&](const auto& guards){for(const auto& guard:guards){std::vector<unsigned char> actual(guard.size);if(!r.memory.read(r.image+guard.rva,actual.data(),actual.size(),error))return false;if(std::memcmp(actual.data(),guard.bytes,guard.size))return probe_contract::fail(error,"object_ui_exact_native_body_differs rva="+std::to_string(guard.rva));}return true;};
    if(!check(mt2mp::game::object_ui_code::guards)||!check(mt2mp::game::move_ui_probe::code::guards))return stopped(job,error,error);
    const bool duplicate=job.input.command=="Duplicate";
    if(duplicate){job.captures=duplicate_edit::captures;job.submitted=duplicate_edit::submitted;job.rejected=duplicate_edit::rejections;for(const auto& [id,operation]:duplicate_edit::operations){(void)operation;job.prior_operations.insert(id);}}
    else{job.captures=group_edit::captures;job.submitted=group_edit::submitted;job.rejected=group_edit::rejections;for(const auto& [id,operation]:group_edit::operations){(void)operation;job.prior_operations.insert(id);}}
    job.money=sample.money;
    if(!selection_ready(r,job,error))return stopped(job,error,error);
    job.revision=controller.value("host_revision",uint64_t(0));job.started_at=GetTickCount64();job.phase="attempted";
    std::vector<uintptr_t> selected;for(const auto& handle:job.selected)selected.push_back(handle.object);const group_edit::Array array{r.image+0x1748c50,selected.data(),int32_t(selected.size()),int32_t(selected.size())};
    job.exception=show(r.image,job.world.selection,&array,&api,&job.attempted_mask,&job.returned_mask);uintptr_t window=0;
    if(job.exception||!current(r,job,error)||!live_window(r,job,api,window,error)){
        stopped(job,error.empty()?"object_ui_original_selection_or_current_window_failed":error,error);
        std::string reconciliation;if(!reconcile_preparation(r,job,reconciliation))job.reconciliation_cause=reconciliation;
        else LOG_INFO("[ObjectUIProbe] token={} preparation-only rejection verified without command/capture; cause={}",job.input.token,job.cause);
        out=format(job);return false;
    }
    job.exception=command(&api,window,job.input.command.c_str(),&job);
    if(job.exception||job.cleanup_exception||!job.command_accepted||job.returned_mask!=63){stopped(job,"object_ui_original_command_or_record_cleanup_failed",error);out=format(job);return false;}
    if(duplicate){
        if(!current(r,job,error)||!cursor(r,job,false,error)){stopped(job,error,error);out=format(job);return false;}
        for(unsigned step=6;step<=8;++step){if(!current(r,job,error)){stopped(job,error,error);out=format(job);return false;}job.exception=cursor_call(r.image,job.behaviour,job.cursor,&job.input.position,step,&job);if(job.exception){stopped(job,"object_ui_original_cursor_callback_failed",error);out=format(job);return false;}}
        if(!cursor(r,job,true,error)){stopped(job,error,error);out=format(job);return false;}
    }
    job.phase="waiting_for_capture_and_host";out=format(job);error.clear();return true;
 }catch(const std::exception& cause){const auto message=std::string("object_ui_probe_exception: ")+cause.what();if(active){stopped(*active,message,error);out=format(*active);return false;}return probe_contract::fail(error,message);}
catch(...){if(active){stopped(*active,"object_ui_probe_unknown_exception",error);out=format(*active);return false;}return probe_contract::fail(error,"object_ui_probe_unknown_exception");}}
bool read(const json& args,json& out,std::string& error)try{
    out=json::object();if(!args.is_object()||args.size()!=1||!args.at("acceptance_token").is_string())return probe_contract::fail(error,"object_ui_receipt_requires_one_token");
    const auto token=args.at("acceptance_token").get<std::string>();const auto found=jobs.find(token);if(found==jobs.end())return probe_contract::fail(error,"object_ui_token_unknown");auto& job=found->second;
    auto* r=published.load(std::memory_order_acquire);if(!r||r->thread!=GetCurrentThreadId())return probe_contract::fail(error,"object_ui_receipt_requires_bound_Mainthread");
    if(!terminal(job)){
        std::string reconciliation;if(reconcile_rejection(*r,job,reconciliation)){
            LOG_INFO("[ObjectUIProbe] token={} action={} exact Host no-effect rejection correlated; cause={}",job.input.token,job.action_uuid,job.cause);
            out=format(job);error.clear();return true;
        }
        if(!reconciliation.empty())job.reconciliation_cause=reconciliation;
    }
    if(job.phase=="unknown"){
        std::string reconciliation;if(!reconcile_preparation(*r,job,reconciliation)){if(!reconciliation.empty())job.reconciliation_cause=reconciliation;}
        else LOG_INFO("[ObjectUIProbe] token={} preparation-only rejection verified without command/capture; cause={}",job.input.token,job.cause);
    }
    const bool late_timeout=job.phase=="unknown"&&job.cause=="object_ui_observation_timeout_ms>30000_no_replay";
    if(late_timeout){
        job.prior_observation_cause=job.cause;
        const uint32_t required=job.input.command=="Duplicate"?511u:63u;
        if(job.attempted_mask!=required||job.returned_mask!=required||job.exception||job.cleanup_exception||!job.command_accepted){
            stopped(job,"object_ui_late_observation_original_command_incomplete",error);out=format(job);return true;
        }
    }
    // An observation deadline does not invalidate a later exact completion.
    // Re-read only the retained operation; every other Unknown stays blocked.
    if(!terminal(job)&&(job.phase!="unknown"||late_timeout)){
        if(!current(*r,job,error)){stopped(job,error,error);out=format(job);return true;}
        const bool duplicate=job.input.command=="Duplicate";const uint64_t captures=duplicate?duplicate_edit::captures:group_edit::captures,submitted=duplicate?duplicate_edit::submitted:group_edit::submitted,rejected=duplicate?duplicate_edit::rejections:group_edit::rejections;
        size_t created=0;bool confirmed=false,retired=false;std::string native_cause;
        if(duplicate){for(const auto& [id,operation]:duplicate_edit::operations)if(!job.prior_operations.contains(id)){
            if(late_timeout&&((!job.action_uuid.empty()&&job.action_uuid!=id)||operation.request.snapshot_uuid!=job.input.snapshot)){
                stopped(job,"object_ui_late_observation_action_or_snapshot_differs",error);out=format(job);return true;
            }
            ++created;job.action_uuid=id;std::vector<std::string> keys;for(const auto& row:operation.request.duplicate.rows)keys.push_back(row.key);
            if(keys!=job.input.keys||operation.binding!=job.binding){stopped(job,"object_ui_duplicate_capture_target_or_binding_differs",error);out=format(job);return true;}
            confirmed=operation.confirmed;retired=operation.retired;native_cause=operation.cause;json births=json::array();for(const auto& row:operation.after.state.created)births.push_back({{"key",row.key},{"family",int(row.family)},{"members",row.members}});
            job.result={{"created",births},{"native",duplicate_edit::status().at("operations").at(id)}};
        }}else{for(const auto& [id,operation]:group_edit::operations)if(!job.prior_operations.contains(id)){
            if(late_timeout&&((!job.action_uuid.empty()&&job.action_uuid!=id)||operation.request.snapshot_uuid!=job.input.snapshot)){
                stopped(job,"object_ui_late_observation_action_or_snapshot_differs",error);out=format(job);return true;
            }
            ++created;job.action_uuid=id;const auto kind=job.input.command=="Group"?action::GroupKind::Group:action::GroupKind::Ungroup;
            if(operation.request.group.kind!=kind||operation.request.group.selected!=job.input.keys||operation.binding!=job.binding){stopped(job,"object_ui_group_capture_target_or_binding_differs",error);out=format(job);return true;}
            confirmed=operation.confirmed;retired=operation.retired;native_cause=operation.cause;json groups=json::array();for(const auto& row:operation.after.local.state.groups){json members=json::array();for(const auto& member:row.members)members.push_back(member.key);groups.push_back({{"key",row.key},{"members",members}});}
            job.result={{"groups",groups},{"native",group_edit::status().at("operations").at(id)}};
        }}
        const auto controller=typed_action_runtime::controller_status();
        if(rejected!=job.rejected&&native_cause.empty())native_cause=duplicate?duplicate_edit::cause:group_edit::cause;
        const probe_contract::Counters before{job.captures,job.submitted,job.rejected,job.revision},after{captures,submitted,rejected,controller.value("host_revision",uint64_t(0))};
        const auto completion=probe_contract::completion(before,after,created,confirmed,retired,controller.value("pending",true),controller.value("locked",true),
            controller.value("action_uuid",std::string{})==job.action_uuid,controller.value("outcome",0),GetTickCount64()-job.started_at,native_cause,error);
        if(completion==probe_contract::Completion::Unknown){if(!late_timeout||error!=job.cause)stopped(job,error,error);}
        else if(completion==probe_contract::Completion::Confirmed){
            job.phase="confirmed";
            if(late_timeout){job.observation_reconciled=true;job.cause.clear();job.reconciliation_cause.clear();
                LOG_INFO("[ObjectUIProbe] token={} action={} exact late completion observed without replay; prior_cause={}",job.input.token,job.action_uuid,job.prior_observation_cause);}
        }
    }
    out=format(job);error.clear();return true;
}catch(const std::exception& cause){return probe_contract::fail(error,std::string("object_ui_read_exception: ")+cause.what());}
}
