#include "release_fixture.h"
#include "release_entry.h"
#include "native_open_runtime.h"
#include "native_lobby.h"
#include "typed_action_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cmath>
#include <map>
namespace mt2mp::game::initial_release {
namespace {
using json=nlohmann::json;
thread_local void* update_game=nullptr;
thread_local uint32_t update_depth=0;
bool locked=false,busy=false;std::string lock_cause;
std::map<std::string,json> retained;
class ProcessMemory final:public Memory {
    placement_ui_probe::ProcessMemory native_;
public:
    bool copy(uintptr_t p,void* out,size_t size,std::string& e)override{return native_.copy(p,out,size,e);}
} memory;
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){if(!p||offset>UINTPTR_MAX-p)return fail(e,"release fixture native address invalid");return memory.copy(p+offset,&out,sizeof out,e);}
json state_json(const State& s){return {{"version",{s.version.major,s.version.minor,s.version.patch}},{"planned",{s.planned.major,s.planned.minor,s.planned.patch}},
    {"release_type",s.release_type},{"changes",s.changes},{"changelogs",s.changelogs},{"last_changelog",{s.last_changelog.major,s.last_changelog.minor,s.last_changelog.patch}},
    {"last_changelog_type",s.last_changelog_type},{"server_current",s.server_current},{"server_target",s.server_target},{"map_state",s.map_state}};}
json ledger_json(const native::SimulationState& s){return {{"cash",s.cash},{"flat_day",s.clock.flat_day},{"record_present",s.record_present},{"record_day",s.record_day},
    {"record_cash",s.record_cash},{"record_income",s.record_income},{"record_expense",s.record_expense},{"line_counts",s.line_counts},{"line_income",s.line_income}};}
class FixtureRoot final:public Root {
public:
    building_mixed_identity::opened_file::CurrentWorld origin;Binding binding;Api api;State state;json ledger;float time_scale=0;
    bool read(std::string& e,bool requirePaused=true){
        uintptr_t image=0;uint32_t thread=0;std::string sha;
        if(!native::read_resolver_receipt(image,thread,sha,e)||!resolve_unpatched(memory,image,sha,api,e))return false;
        if(thread!=GetCurrentThreadId()||image!=uintptr_t(GetModuleHandleW(nullptr))||update_depth!=1||!update_game)
            return fail(e,"release fixture requires actual owned GameUpdate/Mainthread/process");
        if(!Config::instance().is_host)return fail(e,"release fixture requires actual Host process role");
        const auto c=typed_action_runtime::status();
        if(!c.contains("initialized")||!c.at("initialized").is_boolean()||c.at("initialized").get<bool>()||c.contains("error"))
            return fail(e,"release fixture requires actual uninitialized Controller");
        native::WorldStatus w;if(!native::read_world_status(w,e))return false;
        if(w.active_mode_index!=3||w.in_game_state!=4||!w.world_present||w.edit_operations!=0||w.saves_in_progress>0||w.quitting||w.title_transition_pending||w.map_task_pending)
            return fail(e,"release fixture requires actual idle InGame world without save/edit/transition");
        if(!native_open_runtime::read_current_world(origin,e)||!origin.world||!origin.state||!origin.map||!origin.mode||!origin.world_epoch||!origin.generation||origin.game!=update_game)
            return fail(e,e.empty()?"release fixture actual current native Origin/GameUpdate differs":e);
        native::ClockState clock;if(!native::read_clock(clock,e))return false;
        if(!std::isfinite(clock.time_scale)||!std::isfinite(clock.pause_scale)||clock.time_scale<0||clock.pause_scale<0)
            return fail(e,"release fixture actual Clock scales must be finite and nonnegative");
        if(requirePaused&&clock.time_scale!=0)
            return fail(e,"release fixture requires actual Clock selected scale0");
        time_scale=clock.time_scale;
        void* window=nullptr;if(!native_lobby::initial_release_window(origin.mode,&window,e))return false;
        binding={image,uintptr_t(window),0,0,0,0,uintptr_t(origin.map),origin.world_epoch,origin.generation,thread,origin.uuid};
        if(!at(image,0x13df4f0,binding.manager,e)||!at(binding.manager,0x38,binding.release,e)||!at(image,0x13df500,binding.software,e)||!at(image,0x13df430,binding.server,e))return false;
        if(!read_state(memory,binding,state,e))return false;
        native::SimulationState simulation;if(!native::read_simulation_state(simulation,e))return false;ledger=ledger_json(simulation);e.clear();return true;
    }
    bool authorize(const Request& r,Binding& b,Api& a,std::string& e)override{
        if(!read(e))return false;
        if(r.snapshot_uuid!=origin.uuid||r.before!=state)return fail(e,"release fixture actual Origin/expected state differs");
        if(!initial_before(state,e)||!functional_starting_region(memory,binding,e))return false;b=binding;a=api;return true;
    }
    bool current(const Binding& b,std::string& e)override{
        // CurrentRelease is replaced by the original method; world identity must remain.
        FixtureRoot now;if(!now.read(e))return false;
        if(now.origin.uuid!=origin.uuid||now.origin.sha256!=origin.sha256||now.origin.world_epoch!=b.epoch||now.origin.generation!=b.generation||
           now.origin.world!=origin.world||now.origin.mode!=origin.mode||now.binding.window!=b.window||now.binding.image!=b.image||now.binding.map!=b.map||
           now.binding.manager!=b.manager||now.binding.software!=b.software||now.binding.server!=b.server)return fail(e,"release fixture native world/owner changed");return true;
    }
    void failed(std::string_view stage,const std::string& e)noexcept override{LOG_ERROR("[InitialRelease] Fixture stage={} error={}",stage,e);}
};
class FixtureCalls final:public Calls {
    void* mode_;
public:
    uint64_t callbacks=0;
    explicit FixtureCalls(void* mode):mode_(mode){}
    bool command(const Api& api,uintptr_t window,CallReceipt& r,std::string& e)override{
        if(!native_lobby::begin_initial_release_fixture(mode_,e))return false;
        const bool ok=process_calls().command(api,window,r,e);
        std::string owner_error;const bool ended=native_lobby::end_initial_release_fixture(callbacks,owner_error);
        if(!ok){if(!ended)e+="; owned Mode callback: "+owner_error;return false;}
        if(!ended)return fail(e,owner_error);
        if(callbacks!=1)return fail(e,"release fixture native LevelUp callback count measured="+std::to_string(callbacks)+" required=1");return true;
    }
};
json public_root(const FixtureRoot& p){std::string cause;const bool settled=settled_open(p.state,cause);return {{"native_origin_uuid",p.origin.uuid},{"origin_sha256",p.origin.sha256},
    {"world_epoch",p.binding.epoch},{"generation",p.binding.generation},{"host",true},{"controller_initialized",false},{"time_scale",p.time_scale},
    {"state",state_json(p.state)},{"ledger",p.ledger},{"settled_open",settled},{"settled_cause",cause}};}
}
FixtureUpdateScope::FixtureUpdateScope(void* game)noexcept{outer_=update_depth==0;if(outer_)update_game=game;++update_depth;}
FixtureUpdateScope::~FixtureUpdateScope(){if(update_depth)--update_depth;if(outer_)update_game=nullptr;}
bool observe_fixture(json& out,std::string& e){
    out={{"observed",false},{"locked",locked},{"cause",lock_cause},{"receipts",json::array()}};for(const auto& [token,r]:retained){(void)token;out["receipts"].push_back(r);}
    FixtureRoot p;if(!p.read(e,false)){out["read_error"]=e;return false;}out.update(public_root(p));
    std::string functional_cause;const bool functional=functional_starting_region(memory,p.binding,functional_cause);
    out["functional_starting_region"]=functional;out["functional_starting_region_cause"]=functional_cause;
    out["observed"]=true;e.clear();return true;
}
bool run_fixture(const json& args,json& out,std::string& e){
    out={{"native_attempted",false},{"native_returned",false},{"effect_observed",false},{"fixture_only",true},{"peer_ack",false}};
    if(!validate_fixture_args(args,e))return false;
    const auto token=args.at("token").get<std::string>(),uuid=args.at("native_origin_uuid").get<std::string>();
    if(retained.contains(token)){out=retained.at(token);return fail(e,"release fixture token already admitted; read retained receipt, no replay");}
    if(locked||busy)return fail(e,locked?lock_cause:"release fixture is already executing");
    if(retained.size()>=64)return fail(e,"release fixture retained receipt limit measured=64 required<64");
    FixtureRoot root;if(!root.read(e))return false;
    // Compare exact public fields before constructing the genuine command record.
    const auto expected=state_json(root.state);
    if(uuid!=root.origin.uuid||args.at("expected")!=expected)return fail(e,"release fixture actual Origin/expected fields differ");
    if(!initial_before(root.state,e))return false;
    out["token"]=token;out["before"]=public_root(root);retained[token]=out;busy=true;
    struct BusyScope{~BusyScope(){busy=false;}} busy_scope;
    FixtureCalls calls(root.origin.mode);Receipt r;bool success=false;
    try {success=execute({uuid,token,root.state},root,memory,calls,r,e);}
    catch(const std::exception& ex){e=std::string("release fixture callback exception: ")+ex.what();locked=true;lock_cause=e;}
    catch(...){e="release fixture callback unknown exception";locked=true;lock_cause=e;}
    out["native_attempted"]=r.native.attempted;out["native_returned"]=r.native.returned;out["native_accepted"]=r.native.accepted;
    out["record_constructed"]=r.native.constructed;out["record_label_confirmed"]=r.native.label_confirmed;out["record_cleanup_attempted"]=r.native.cleanup_attempted;
    out["record_cleanup_confirmed"]=r.native.cleanup_confirmed;out["seh_code"]=r.native.seh_code;out["cleanup_exception"]=r.native.cleanup_exception;
    out["mode_levelup_callbacks"]=calls.callbacks;out["effect_observed"]=success&&r.effect_observed;out["cause"]=e;
    FixtureRoot after;std::string read_error;
    try {if(after.read(read_error))out["after"]=public_root(after);else out["after_read_error"]=read_error;}
    catch(const std::exception& ex){read_error=std::string("release fixture after-read exception: ")+ex.what();out["after_read_error"]=read_error;}
    catch(...){read_error="release fixture after-read unknown exception";out["after_read_error"]=read_error;}
    if(!success||!read_error.empty()){if(e.empty())e=read_error;if(r.native.attempted){locked=true;lock_cause=e;}out["cause"]=e;retained[token]=out;return false;}
    retained[token]=out;return true;
}
}
