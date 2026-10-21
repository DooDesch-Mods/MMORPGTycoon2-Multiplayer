namespace {
namespace nt=network_tab;
using NetworkWorld=building_mixed_identity::opened_file::CurrentWorld;
struct NetworkJob {
    Job base;
    NetworkWorld origin;
    action_session::Binding binding;
    action_session::Ledger ledger_before;
    nt::Pin pin;
    nt::Receipt receipt;
    std::string cause;
};
std::map<std::string,NetworkJob> network_jobs;
NetworkJob* active_network_job=nullptr;
bool network_unresolved()noexcept {
    for(const auto& [token,j]:network_jobs){(void)token;if(j.receipt.unknown||(j.receipt.attempted&&!j.receipt.state.ready))return true;}
    return false;
}
bool network_same_world(const NetworkWorld& a,const NetworkWorld& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
bool network_pin(NetworkJob& j,nt::Pin& pin,std::string& e){
    if(!world(j.base,e))return false;
    NetworkWorld current;action_session::Binding binding;
    if(!native_open_runtime::read_current_world(current,e)||!network_same_world(current,j.origin))return fail(e,e.empty()?"Network tab full NativeOpen owner changed":e);
    if(!typed_action_runtime::network_binding(j.base.context.snapshot,binding,e)||binding!=j.binding)
        return fail(e,e.empty()?"Network tab Controller binding changed":e);
    return nt::inspect(memory,j.base.image,reinterpret_cast<uintptr_t>(current.world),pin,e);
}
bool network_unchanged(NetworkJob& j,std::string& e){
    native::SimulationState state;native::WorldStatus world_state;
    if(!native::read_simulation_state(state,e)||!native::read_world_status(world_state,e))return false;
    if(state.clock.time_scale!=0||world_state.edit_operations||world_state.saves_in_progress)
        return fail(e,"Network tab requires actual paused Clock and no edit/save");
    action_session::Ledger after;
    return ledger(after,e)&&(after==j.ledger_before||fail(e,"Network tab full ledger changed"));
}
uint32_t network_press(NetworkJob& j,const nt::Pin& pin)noexcept {
    const std::array<float,2> position{};
    __try {
        reinterpret_cast<void(*)(void*,void*,const void*)>(j.base.image+0x57c950)(
            reinterpret_cast<void*>(pin.tab),reinterpret_cast<void*>(pin.cursor),&position);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
json network_format(const NetworkJob& j){
    const auto& q=j.receipt;
    return {{"acceptance_token",j.base.context.token},{"snapshot_uuid",j.base.context.snapshot},
        {"attempted",q.attempted},{"returned",q.returned},{"confirmed",q.confirmed},{"unknown",q.unknown},
        {"exception",j.base.exception},{"tab","Network"},{"tab_requested",q.returned},
        {"tab_ready",q.state.ready},{"tab_pending",q.confirmed&&!q.state.ready},{"tab_cause",q.state.cause},
        {"cause",j.cause},{"host_ordered_action",false}};
}
void network_fault(NetworkJob& j,const std::string& cause){
    j.cause=cause;j.receipt.confirmed=false;j.receipt.unknown=j.receipt.attempted;
    LOG_ERROR("[ToolPrepare] Network tab token={} cause={}; no replay",j.base.context.token,cause);
}
}
bool prepare_network_tab(const json& args,json& out,std::string& e)try {
    active_network_job=nullptr;out=json::object();Context q;
    if(!parse(args,q,e))return false;
    if(!Config::instance().local_test)return fail(e,"Network tab requires owned local test runtime");
    if(unresolved()||jobs.contains(q.token)||buildings_jobs.contains(q.token)||network_jobs.contains(q.token))return fail(e,"Network tab token used or previous native effect unresolved; no replay");
    if(network_jobs.size()>=32)return fail(e,"Network tab retained receipt capacity reached");
    NetworkJob initial;initial.base.context=q;
    if(!world(initial.base,e)||!native_open_runtime::read_current_world(initial.origin,e)||
       !typed_action_runtime::network_binding(q.snapshot,initial.binding,e)||!ledger(initial.ledger_before,e)||
       !network_unchanged(initial,e)||!nt::verify(memory,initial.base.image,e))return false;
    nt::Pin pin;
    if(!network_pin(initial,pin,e))return false;
    uintptr_t current=0,desired=0,ap=0;
    if(!field(pin.cursor,0x190,current,e)||!field(pin.cursor,0x198,desired,e)||current!=desired||
       !field(desired,0,ap,e)||ap!=initial.base.image+0x17391a0)
        return fail(e,e.empty()?"Network tab preparation requires actual idle Default cursor":e);
    initial.base.cursor=pin.cursor;initial.base.old=current;initial.base.desired=desired;
    if(!observe(initial.base,e))return false;
    if(pin.active){
        if(!nt::status(memory,pin,initial.receipt.state,e)||!initial.receipt.state.ready)
            return fail(e,e.empty()?"Network tab already active but not ready; no Press replay":e);
        initial.receipt.confirmed=true;initial.pin=pin;
        auto& j=network_jobs.emplace(q.token,std::move(initial)).first->second;
        out=network_format(j);return true;
    }
    auto& j=network_jobs.emplace(q.token,std::move(initial)).first->second;active_network_job=&j;
    struct Calls {
        NetworkJob& j;
        bool pin(nt::Pin& p,std::string& e){return network_pin(j,p,e);}
        bool reserve(const nt::Pin& p,std::string&){j.pin=p;return true;}
        bool press(const nt::Pin& p,std::string& e){j.base.exception=network_press(j,p);return !j.base.exception||fail(e,"Network original Tab::Press exception="+std::to_string(j.base.exception));}
        bool status(const nt::Pin& p,nt::Status& state,std::string& e){return nt::status(memory,p,state,e);}
        bool unchanged(std::string& e){return network_unchanged(j,e);}
    } calls{j};
    const bool okay=nt::prepare(calls,j.receipt,e);
    j.cause=e;out=network_format(j);
    if(!okay)LOG_ERROR("[ToolPrepare] Network tab token={} cause={}; no replay",q.token,e);
    return okay;
}catch(const std::exception& ex){e=std::string("Network tab exception: ")+ex.what();if(active_network_job){network_fault(*active_network_job,e);out=network_format(*active_network_job);}return false;}
catch(...){e="Network tab non-standard exception";if(active_network_job){network_fault(*active_network_job,e);out=network_format(*active_network_job);}return false;}
bool read_network_tab(const json& args,json& out,std::string& e)try {
    active_network_job=nullptr;out=json::object();
    if(!args.is_object()||args.size()!=1||!args.contains("acceptance_token")||!args.at("acceptance_token").is_string())
        return fail(e,"Network tab read requires exact retained token");
    const auto found=network_jobs.find(args.at("acceptance_token").get<std::string>());
    if(found==network_jobs.end())return fail(e,"Network tab token unknown");
    auto& j=found->second;active_network_job=&j;
    if(j.receipt.unknown||!j.receipt.confirmed){out=network_format(j);return fail(e,j.cause);}
    nt::Pin pin;
    if(!network_pin(j,pin,e)||!nt::same_owner(j.pin,pin)||!nt::status(memory,pin,j.receipt.state,e)||!network_unchanged(j,e)){
        if(e.empty())e="Network tab retained owner changed";network_fault(j,e);out=network_format(j);return false;
    }
    j.cause.clear();out=network_format(j);out["read_only"]=true;return true;
}catch(const std::exception& ex){e=std::string("Network tab read exception: ")+ex.what();if(active_network_job){network_fault(*active_network_job,e);out=network_format(*active_network_job);}return false;}
catch(...){e="Network tab read non-standard exception";if(active_network_job){network_fault(*active_network_job,e);out=network_format(*active_network_job);}return false;}
