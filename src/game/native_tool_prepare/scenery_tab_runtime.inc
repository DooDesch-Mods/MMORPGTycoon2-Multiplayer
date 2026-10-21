namespace {
namespace st=scenery_tab;
using SceneryWorld=building_mixed_identity::opened_file::CurrentWorld;
struct SceneryJob {
    Job base;
    SceneryWorld origin;
    action_session::Binding binding;
    action_session::Ledger ledger_before;
    st::Pin pin;
    st::Receipt receipt;
    std::string cause;
};
std::map<std::string,SceneryJob> scenery_jobs;
SceneryJob* active_scenery_job=nullptr;
bool scenery_unresolved()noexcept {
    for(const auto& [token,j]:scenery_jobs){(void)token;if(j.receipt.unknown||(j.receipt.attempted&&!j.receipt.state.ready))return true;}
    return false;
}
bool scenery_same_world(const SceneryWorld& a,const SceneryWorld& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
bool scenery_pin(SceneryJob& j,st::Pin& pin,std::string& e){
    if(!world(j.base,e))return false;
    SceneryWorld current;action_session::Binding binding;
    if(!native_open_runtime::read_current_world(current,e)||!scenery_same_world(current,j.origin))return fail(e,e.empty()?"Scenery tab full NativeOpen owner changed":e);
    if(!typed_action_runtime::network_binding(j.base.context.snapshot,binding,e)||binding!=j.binding)
        return fail(e,e.empty()?"Scenery tab Controller binding changed":e);
    return st::inspect(memory,j.base.image,reinterpret_cast<uintptr_t>(current.world),pin,e);
}
bool scenery_unchanged(SceneryJob& j,std::string& e){
    native::SimulationState state;native::WorldStatus world_state;
    if(!native::read_simulation_state(state,e)||!native::read_world_status(world_state,e))return false;
    if(state.clock.time_scale!=0||world_state.edit_operations||world_state.saves_in_progress)
        return fail(e,"Scenery tab requires actual paused Clock and no edit/save");
    action_session::Ledger after;
    return ledger(after,e)&&(after==j.ledger_before||fail(e,"Scenery tab full ledger changed"));
}
uint32_t scenery_press(SceneryJob& j,const st::Pin& pin)noexcept {
    const std::array<float,2> position{};
    __try {
        reinterpret_cast<void(*)(void*,void*,const void*)>(j.base.image+0x57c950)(
            reinterpret_cast<void*>(pin.tab),reinterpret_cast<void*>(pin.cursor),&position);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
json scenery_format(const SceneryJob& j){
    const auto& q=j.receipt;
    return {{"acceptance_token",j.base.context.token},{"snapshot_uuid",j.base.context.snapshot},
        {"attempted",q.attempted},{"returned",q.returned},{"confirmed",q.confirmed},{"unknown",q.unknown},
        {"exception",j.base.exception},{"tab","Scenery"},{"tab_requested",q.returned},
        {"tab_ready",q.state.ready},{"tab_pending",q.confirmed&&!q.state.ready},{"tab_cause",q.state.cause},
        {"cause",j.cause},{"host_ordered_action",false}};
}
void scenery_fault(SceneryJob& j,const std::string& cause){
    j.cause=cause;j.receipt.confirmed=false;j.receipt.unknown=j.receipt.attempted;
    LOG_ERROR("[ToolPrepare] Scenery tab token={} cause={}; no replay",j.base.context.token,cause);
}
}
bool prepare_scenery_tab(const json& args,json& out,std::string& e)try {
    active_scenery_job=nullptr;out=json::object();Context q;
    if(!parse(args,q,e))return false;
    if(!Config::instance().local_test)return fail(e,"Scenery tab requires owned local test runtime");
    if(unresolved()||jobs.contains(q.token)||buildings_jobs.contains(q.token)||network_jobs.contains(q.token)||scenery_jobs.contains(q.token))return fail(e,"Scenery tab token used or previous native effect unresolved; no replay");
    if(scenery_jobs.size()>=32)return fail(e,"Scenery tab retained receipt capacity reached");
    SceneryJob initial;initial.base.context=q;
    if(!world(initial.base,e)||!native_open_runtime::read_current_world(initial.origin,e)||
       !typed_action_runtime::network_binding(q.snapshot,initial.binding,e)||!ledger(initial.ledger_before,e)||
       !scenery_unchanged(initial,e)||!st::verify(memory,initial.base.image,e))return false;
    st::Pin pin;
    if(!scenery_pin(initial,pin,e))return false;
    uintptr_t current=0,desired=0,ap=0;
    if(!field(pin.cursor,0x190,current,e)||!field(pin.cursor,0x198,desired,e)||current!=desired||
       !field(desired,0,ap,e)||ap!=initial.base.image+0x17391a0)
        return fail(e,e.empty()?"Scenery tab preparation requires actual idle Default cursor":e);
    initial.base.cursor=pin.cursor;initial.base.old=current;initial.base.desired=desired;
    if(!observe(initial.base,e))return false;
    if(pin.active){
        if(!st::status(memory,pin,initial.receipt.state,e)||!initial.receipt.state.ready)
            return fail(e,e.empty()?"Scenery tab already active but not ready; no Press replay":e);
        initial.receipt.confirmed=true;initial.pin=pin;
        auto& j=scenery_jobs.emplace(q.token,std::move(initial)).first->second;
        out=scenery_format(j);return true;
    }
    auto& j=scenery_jobs.emplace(q.token,std::move(initial)).first->second;active_scenery_job=&j;
    struct Calls {
        SceneryJob& j;
        bool pin(st::Pin& p,std::string& e){return scenery_pin(j,p,e);}
        bool reserve(const st::Pin& p,std::string&){j.pin=p;return true;}
        bool press(const st::Pin& p,std::string& e){j.base.exception=scenery_press(j,p);return !j.base.exception||fail(e,"Scenery original Tab::Press exception="+std::to_string(j.base.exception));}
        bool status(const st::Pin& p,st::Status& state,std::string& e){return st::status(memory,p,state,e);}
        bool unchanged(std::string& e){return scenery_unchanged(j,e);}
    } calls{j};
    const bool okay=st::prepare(calls,j.receipt,e);
    j.cause=e;out=scenery_format(j);
    if(!okay)LOG_ERROR("[ToolPrepare] Scenery tab token={} cause={}; no replay",q.token,e);
    return okay;
}catch(const std::exception& ex){e=std::string("Scenery tab exception: ")+ex.what();if(active_scenery_job){scenery_fault(*active_scenery_job,e);out=scenery_format(*active_scenery_job);}return false;}
catch(...){e="Scenery tab non-standard exception";if(active_scenery_job){scenery_fault(*active_scenery_job,e);out=scenery_format(*active_scenery_job);}return false;}
bool read_scenery_tab(const json& args,json& out,std::string& e)try {
    active_scenery_job=nullptr;out=json::object();
    if(!args.is_object()||args.size()!=1||!args.contains("acceptance_token")||!args.at("acceptance_token").is_string())
        return fail(e,"Scenery tab read requires exact retained token");
    const auto found=scenery_jobs.find(args.at("acceptance_token").get<std::string>());
    if(found==scenery_jobs.end())return fail(e,"Scenery tab token unknown");
    auto& j=found->second;active_scenery_job=&j;
    if(j.receipt.unknown||!j.receipt.confirmed){out=scenery_format(j);return fail(e,j.cause);}
    st::Pin pin;
    if(!scenery_pin(j,pin,e)||!st::same_owner(j.pin,pin)||!st::status(memory,pin,j.receipt.state,e)||!scenery_unchanged(j,e)){
        if(e.empty())e="Scenery tab retained owner changed";scenery_fault(j,e);out=scenery_format(j);return false;
    }
    j.cause.clear();out=scenery_format(j);out["read_only"]=true;return true;
}catch(const std::exception& ex){e=std::string("Scenery tab read exception: ")+ex.what();if(active_scenery_job){scenery_fault(*active_scenery_job,e);out=scenery_format(*active_scenery_job);}return false;}
catch(...){e="Scenery tab read non-standard exception";if(active_scenery_job){scenery_fault(*active_scenery_job,e);out=scenery_format(*active_scenery_job);}return false;}
