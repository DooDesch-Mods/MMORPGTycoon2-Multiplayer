namespace {
namespace bt=buildings_tab;
using TabWorld=building_mixed_identity::opened_file::CurrentWorld;
struct BuildingsJob {
    Job base;
    TabWorld origin;
    action_session::Binding binding;
    action_session::Ledger ledger_before;
    bt::Pin pin;
    bt::Receipt receipt;
    std::string cause;
};
std::map<std::string,BuildingsJob> buildings_jobs;
BuildingsJob* active_buildings_job=nullptr;
bool buildings_unresolved()noexcept {
    for(const auto& [token,j]:buildings_jobs){(void)token;if(j.receipt.unknown||(j.receipt.attempted&&!j.receipt.state.ready))return true;}
    return false;
}
bool tab_same_world(const TabWorld& a,const TabWorld& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
bool tab_pin(BuildingsJob& j,bt::Pin& pin,std::string& e){
    if(!world(j.base,e))return false;
    TabWorld current;action_session::Binding binding;
    if(!native_open_runtime::read_current_world(current,e)||!tab_same_world(current,j.origin))return fail(e,e.empty()?"Buildings tab full NativeOpen owner changed":e);
    if(!typed_action_runtime::network_binding(j.base.context.snapshot,binding,e)||binding!=j.binding)
        return fail(e,e.empty()?"Buildings tab Controller binding changed":e);
    return bt::inspect(memory,j.base.image,reinterpret_cast<uintptr_t>(current.world),pin,e);
}
bool tab_unchanged(BuildingsJob& j,std::string& e){
    native::SimulationState state;native::WorldStatus world_state;
    if(!native::read_simulation_state(state,e)||!native::read_world_status(world_state,e))return false;
    if(state.clock.time_scale!=0||world_state.edit_operations||world_state.saves_in_progress)
        return fail(e,"Buildings tab requires actual paused Clock and no edit/save");
    action_session::Ledger after;
    return ledger(after,e)&&(after==j.ledger_before||fail(e,"Buildings tab full ledger changed"));
}
uint32_t tab_press(BuildingsJob& j,const bt::Pin& pin)noexcept {
    const std::array<float,2> position{};
    __try {
        reinterpret_cast<void(*)(void*,void*,const void*)>(j.base.image+0x57c950)(
            reinterpret_cast<void*>(pin.tab),reinterpret_cast<void*>(pin.cursor),&position);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
json tab_format(const BuildingsJob& j){
    const auto& q=j.receipt;
    return {{"acceptance_token",j.base.context.token},{"snapshot_uuid",j.base.context.snapshot},
        {"attempted",q.attempted},{"returned",q.returned},{"confirmed",q.confirmed},{"unknown",q.unknown},
        {"exception",j.base.exception},{"tab","Buildings"},{"tab_requested",q.returned},
        {"tab_ready",q.state.ready},{"tab_pending",q.confirmed&&!q.state.ready},{"tab_cause",q.state.cause},
        {"cause",j.cause},{"host_ordered_action",false}};
}
void tab_fault(BuildingsJob& j,const std::string& cause){
    j.cause=cause;j.receipt.confirmed=false;j.receipt.unknown=j.receipt.attempted;
    LOG_ERROR("[ToolPrepare] Buildings tab token={} cause={}; no replay",j.base.context.token,cause);
}
}
bool prepare_buildings_tab(const json& args,json& out,std::string& e)try {
    active_buildings_job=nullptr;out=json::object();Context q;
    if(!parse(args,q,e))return false;
    if(!Config::instance().local_test)return fail(e,"Buildings tab requires owned local test runtime");
    if(unresolved()||jobs.contains(q.token)||buildings_jobs.contains(q.token))return fail(e,"Buildings tab token used or previous native effect unresolved; no replay");
    if(buildings_jobs.size()>=32)return fail(e,"Buildings tab retained receipt capacity reached");
    BuildingsJob initial;initial.base.context=q;
    if(!world(initial.base,e)||!native_open_runtime::read_current_world(initial.origin,e)||
       !typed_action_runtime::network_binding(q.snapshot,initial.binding,e)||!ledger(initial.ledger_before,e)||
       !tab_unchanged(initial,e)||!bt::verify(memory,initial.base.image,e))return false;
    bt::Pin pin;
    if(!tab_pin(initial,pin,e))return false;
    uintptr_t current=0,desired=0,ap=0;
    if(!field(pin.cursor,0x190,current,e)||!field(pin.cursor,0x198,desired,e)||current!=desired||
       !field(desired,0,ap,e)||ap!=initial.base.image+0x17391a0)
        return fail(e,e.empty()?"Buildings tab preparation requires actual idle Default cursor":e);
    initial.base.cursor=pin.cursor;initial.base.old=current;initial.base.desired=desired;
    if(!observe(initial.base,e))return false;
    if(pin.active){
        if(!bt::status(memory,pin,initial.receipt.state,e)||!initial.receipt.state.ready)
            return fail(e,e.empty()?"Buildings tab already active but not ready; no Press replay":e);
        initial.receipt.confirmed=true;initial.pin=pin;
        auto& j=buildings_jobs.emplace(q.token,std::move(initial)).first->second;
        out=tab_format(j);return true;
    }
    auto& j=buildings_jobs.emplace(q.token,std::move(initial)).first->second;active_buildings_job=&j;
    struct Calls {
        BuildingsJob& j;
        bool pin(bt::Pin& p,std::string& e){return tab_pin(j,p,e);}
        bool reserve(const bt::Pin& p,std::string&){j.pin=p;return true;}
        bool press(const bt::Pin& p,std::string& e){j.base.exception=tab_press(j,p);return !j.base.exception||fail(e,"Buildings original Tab::Press exception="+std::to_string(j.base.exception));}
        bool status(const bt::Pin& p,bt::Status& state,std::string& e){return bt::status(memory,p,state,e);}
        bool unchanged(std::string& e){return tab_unchanged(j,e);}
    } calls{j};
    const bool okay=bt::prepare(calls,j.receipt,e);
    j.cause=e;out=tab_format(j);
    if(!okay)LOG_ERROR("[ToolPrepare] Buildings tab token={} cause={}; no replay",q.token,e);
    return okay;
}catch(const std::exception& ex){e=std::string("Buildings tab exception: ")+ex.what();if(active_buildings_job){tab_fault(*active_buildings_job,e);out=tab_format(*active_buildings_job);}return false;}
catch(...){e="Buildings tab non-standard exception";if(active_buildings_job){tab_fault(*active_buildings_job,e);out=tab_format(*active_buildings_job);}return false;}
bool read_buildings_tab(const json& args,json& out,std::string& e)try {
    active_buildings_job=nullptr;out=json::object();
    if(!args.is_object()||args.size()!=1||!args.contains("acceptance_token")||!args.at("acceptance_token").is_string())
        return fail(e,"Buildings tab read requires exact retained token");
    const auto found=buildings_jobs.find(args.at("acceptance_token").get<std::string>());
    if(found==buildings_jobs.end())return fail(e,"Buildings tab token unknown");
    auto& j=found->second;active_buildings_job=&j;
    if(j.receipt.unknown||!j.receipt.confirmed){out=tab_format(j);return fail(e,j.cause);}
    bt::Pin pin;
    if(!tab_pin(j,pin,e)||!bt::same_owner(j.pin,pin)||!bt::status(memory,pin,j.receipt.state,e)||!tab_unchanged(j,e)){
        if(e.empty())e="Buildings tab retained owner changed";tab_fault(j,e);out=tab_format(j);return false;
    }
    j.cause.clear();out=tab_format(j);out["read_only"]=true;return true;
}catch(const std::exception& ex){e=std::string("Buildings tab read exception: ")+ex.what();if(active_buildings_job){tab_fault(*active_buildings_job,e);out=tab_format(*active_buildings_job);}return false;}
catch(...){e="Buildings tab read non-standard exception";if(active_buildings_job){tab_fault(*active_buildings_job,e);out=tab_format(*active_buildings_job);}return false;}
