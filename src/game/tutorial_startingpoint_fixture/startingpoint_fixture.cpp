#include "startingpoint_fixture.h"
#include "tutorial_development.h"
#include "tutorial_observer.h"
#include "native_open_runtime.h"
#include "typed_action_runtime.h"
#include "hud_time_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/game/native_engine.h"
#include "entry_bytes.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace mt2mp::game::tutorial_startingpoint_fixture {
namespace {
using json=nlohmann::json;
using World=building_mixed_identity::opened_file::CurrentWorld;
thread_local void* update_game=nullptr;
thread_local unsigned update_depth=0;
placement_ui_probe::ProcessMemory memory;
struct Progress {bool shown=false,completed=false,committed=false,locked=false;std::string cause;};
std::map<std::string,Progress> worlds;
std::map<std::string,json> receipts;
bool busy=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t off,T& out,std::string& e){
    if(!p||off>UINTPTR_MAX-p)return fail(e,"startingpoint fixture address overflow");return memory.copy(p+off,&out,sizeof(out),e);
}
bool text(uintptr_t p,size_t off,std::string& out,std::string& e){
    uintptr_t data=0;uint64_t size=0;if(!at(p,off,data,e)||!at(p,off+8,size,e))return false;
    if(size>200)return fail(e,"startingpoint fixture native text exceeds200");
    std::array<char,201> bytes{};if(!memory.copy(data,bytes.data(),size_t(size)+1,e))return false;
    if(bytes[size]||std::memchr(bytes.data(),0,size_t(size)))return fail(e,"startingpoint fixture native text terminator differs");
    out.assign(bytes.data(),size_t(size));return true;
}
bool type(uintptr_t p,uintptr_t expected,std::string& e){uintptr_t value=0;return at(p,0,value,e)&&(value==expected||fail(e,"startingpoint fixture native type differs"));}
bool same(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
struct Pin {
    World world;
    uintptr_t image=0,lounge=0,project=0,window=0,button=0,player=0,inventory=0,slots=0;
    std::array<uintptr_t,200> items{};
    json development,tutorial;
};
bool pin(const Request& request,Pin& p,std::string& e){
    uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(p.image,thread,sha,e))return false;
    if(sha!=exact_sha||p.image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId()||update_depth!=1||!update_game)
        return fail(e,"startingpoint fixture requires exact EXE/owned GameUpdate/Mainthread");
    if(!Config::instance().local_test||!Config::instance().is_host)return fail(e,"startingpoint fixture requires isolated local-test Host");
    const auto controller=typed_action_runtime::status();
    if(!controller.contains("initialized")||!controller.at("initialized").is_boolean()||controller.at("initialized").get<bool>()||controller.contains("error"))
        return fail(e,"startingpoint fixture requires uninitialized Controller");
    native::WorldStatus status;if(!native::read_world_status(status,e))return false;
    if(status.active_mode_index!=3||status.in_game_state!=4||!status.world_present||status.edit_operations||status.saves_in_progress||status.quitting||status.title_transition_pending||status.map_task_pending)
        return fail(e,"startingpoint fixture requires idle InGame without edit/save/transition");
    native::ClockState clock;if(!native::read_clock(clock,e))return false;
    if(clock.time_scale!=0||!std::isfinite(clock.pause_scale)||clock.pause_scale<0)return fail(e,"startingpoint fixture requires selected Clock0");
    if(!native_open_runtime::read_current_world(p.world,e)||!tutorial_development::read(p.development,e)||!tutorial_observer::read(p.tutorial,e))return false;
    if(p.world.game!=update_game||p.world.uuid!=request.origin_uuid||p.world.world_epoch!=request.epoch||p.world.generation!=request.generation)
        return fail(e,"startingpoint fixture expected local Origin differs");
    if(!hud_time_runtime::verify_window_show_hook(p.image+0x81ec20,e))return false;
    for(const auto& entry:entries){std::vector<unsigned char> actual(entry.size);
        if(!memory.copy(p.image+entry.rva,actual.data(),actual.size(),e))return false;
        if(mt2mp::sha256(actual)!=entry.sha256)return fail(e,"startingpoint fixture entry differs RVA="+std::to_string(entry.rva));}
    const auto state=uintptr_t(p.world.state);
    if(!at(state,0x180,p.lounge,e)||!at(p.lounge,0xa8,p.project,e))return false;
    int32_t count=0;uintptr_t global_player=0;
    if(!at(uintptr_t(p.world.mode),0x150,p.player,e)||!at(p.image,0x13df9f0,global_player,e)||
       !at(p.player,0x1d0,p.inventory,e)||!type(p.inventory,p.image+0x16df0f0,e)||
       !at(p.inventory,8,p.slots,e)||!at(p.inventory,0x10,count,e))return false;
    if(p.player!=global_player||count!=200)return fail(e,"startingpoint fixture current Player/inventory owner or slot count differs");
    if(!memory.copy(p.slots,p.items.data(),sizeof(p.items),e))return false;
    World after;if(!native_open_runtime::read_current_world(after,e))return false;
    return same(p.world,after)||fail(e,"startingpoint fixture Origin changed during pin");
}
bool before(const Pin& p,std::string& e){
    const auto& d=p.development.at("project");const auto& t=p.development.at("startingpoint");
    if(!d.at("present").get<bool>()||d.at("kind")!=0||d.at("tech_key")!="startingpoint"||!p.project)
        return fail(e,"startingpoint fixture requires current native startingpoint project");
    if(d.at("bugs_total")!=0||d.at("bugs_known")!=0||d.at("bugs_unknown")!=0)
        return fail(e,"startingpoint fixture requires all actual bug counters0; nonzero Map effects unsupported");
    if(!t.at("record_present").get<bool>()||!t.at("purchased").get<bool>()||t.at("deployed").get<bool>()||t.at("custom").get<bool>()||t.at("owned").get<bool>()||t.at("inventory_count")!=0)
        return fail(e,"startingpoint fixture requires purchased/unowned record and no StartingPoints item");
    if(std::find(p.items.begin(),p.items.end(),uintptr_t(0))==p.items.end())
        return fail(e,"startingpoint fixture requires a free native inventory slot");
    // The native reader has already checked reciprocal TechRecord and catalogue ownership.
    return true;
}
bool same_inventory_owner(const Pin& a,const Pin& b){return a.player==b.player&&a.inventory==b.inventory&&a.slots==b.slots;}
bool one_inventory_insert(const Pin& a,const Pin& b,std::string& e){
    if(!same_inventory_owner(a,b))return fail(e,"startingpoint fixture inventory owner changed");
    unsigned inserted=0;
    for(size_t i=0;i<a.items.size();++i){
        if(a.items[i]==b.items[i])continue;
        if(a.items[i]||!b.items[i])return fail(e,"startingpoint fixture Commit replaced or removed an inventory item");
        ++inserted;
    }
    return inserted==1||fail(e,"startingpoint fixture Commit must insert exactly one inventory item");
}
struct BorrowedString {const char* data;uint64_t length;unsigned char storage[16]{};};
static_assert(sizeof(BorrowedString)==32);
DWORD find_button(uintptr_t image,uintptr_t window,uintptr_t& button){
    const BorrowedString name{"commit",6,{}};
    __try{button=uintptr_t(reinterpret_cast<void*(*)(void*,const void*)>(image+0x81d860)(reinterpret_cast<void*>(window),&name));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool development_window(Pin& p,std::string& e){
    uintptr_t data=0;int32_t count=0,capacity=0;const auto mode=uintptr_t(p.world.mode);
    if(!at(mode,0xa0,data,e)||!at(mode,0xa8,count,e)||!at(mode,0xac,capacity,e))return false;
    if(count<1||capacity<count||capacity>512)return fail(e,"startingpoint fixture Mode window array bounds invalid");
    std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t window=0;std::string id;
        if(!at(data,size_t(i)*8,window,e)||!window||!seen.insert(window).second)return fail(e,e.empty()?"startingpoint fixture Mode window ownership duplicated":e);
        if(!text(window,0x1b0,id,e))return false;
        if(id=="development"){if(p.window)return fail(e,"startingpoint fixture duplicate owned DevelopmentWindow");p.window=window;}}
    if(!p.window||!type(p.window,p.image+0x1722e60,e))return fail(e,e.empty()?"startingpoint fixture owned DevelopmentWindow absent":e);
    uintptr_t logic=0;if(!at(p.window,0x318,logic,e))return false;
    if(logic)return fail(e,"startingpoint fixture DevelopmentWindow has custom logic");
    return true;
}
bool commit_button(Pin& p,std::string& e){
    if(!development_window(p,e))return false;
    uint8_t visible=0;if(!at(p.window,0x45f,visible,e))return false;
    if(visible!=1)return fail(e,"startingpoint fixture requires visible DevelopmentWindow");
    const auto exception=find_button(p.image,p.window,p.button);
    if(exception)return fail(e,"startingpoint fixture FindPane exception="+std::to_string(exception));
    if(!p.button||!type(p.button,p.image+0x16e3ec0,e))return fail(e,e.empty()?"startingpoint fixture original Commit button absent":e);
    uintptr_t owner=0;uint8_t disabled=0,toggle=0;std::string command;
    if(!at(p.button,0x280,owner,e)||!at(p.button,0x46d,disabled,e)||!at(p.button,0x46c,toggle,e)||!text(p.button,0x388,command,e))return false;
    if(owner!=p.window||disabled||toggle||command!="MessageTo Window CommitTech")
        return fail(e,"startingpoint fixture Commit button owner/enabled/command differs");
    return true;
}
DWORD complete(uintptr_t image,uintptr_t lounge){__try{reinterpret_cast<void(*)(void*)>(image+0x25ec50)(reinterpret_cast<void*>(lounge));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD activate(uintptr_t image,uintptr_t button){__try{reinterpret_cast<void(*)(void*)>(image+0x674780)(reinterpret_cast<void*>(button));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD show(uintptr_t image,uintptr_t window){__try{reinterpret_cast<void(*)(void*,bool,bool)>(image+0x81ec20)(reinterpret_cast<void*>(window),true,false);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD complete_state(uintptr_t image,uintptr_t lounge,bool& result){__try{result=reinterpret_cast<bool(*)(void*)>(image+0x25ed50)(reinterpret_cast<void*>(lounge));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct Busy {Busy(){busy=true;}~Busy(){busy=false;}};
}
UpdateScope::UpdateScope(void* game) noexcept:outer_(update_depth==0){if(outer_)update_game=game;++update_depth;}
UpdateScope::~UpdateScope(){if(update_depth)--update_depth;if(outer_)update_game=nullptr;}
bool run(const Request& request,json& result,std::string& e){
    result={{"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false}};e.clear();
    std::string key;Progress* phase=nullptr;
    try{
        if(request.token.empty()||request.token.size()>64||request.origin_uuid.empty()||!request.epoch||!request.generation||
           (request.operation!="show"&&request.operation!="complete"&&request.operation!="commit"))return fail(e,"startingpoint fixture fixed request invalid");
        // No shared receipt state is touched outside the bound update thread.
        uintptr_t image=0;uint32_t thread=0;std::string sha;
        if(!native::read_resolver_receipt(image,thread,sha,e))return false;
        if(thread!=GetCurrentThreadId()||update_depth!=1||!update_game)
            return fail(e,"startingpoint fixture requires owned GameUpdate/Mainthread before receipt access");
        if(receipts.contains(request.token)){result=receipts.at(request.token);result["replay_rejected"]=true;return fail(e,"startingpoint fixture token consumed; no replay");}
        if(busy||receipts.size()>=64||worlds.size()>=64)return fail(e,"startingpoint fixture busy or receipt bound reached");
        Busy scope;Pin p;if(!pin(request,p,e)||!before(p,e))return false;
        if(p.development!=request.expected_development)return fail(e,"startingpoint fixture expected development readback differs");
        key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation);
        phase=&worlds[key];if(phase->locked)return fail(e,phase->cause);
        const bool is_complete=request.operation=="complete";
        const bool is_show=request.operation=="show";
        if((is_show&&phase->shown)||(is_complete&&phase->completed)||(!is_show&&!is_complete&&(!phase->completed||phase->committed)))return fail(e,"startingpoint fixture phase already consumed or completion proof absent");
        DWORD exception=0;
        if(!is_show){
            bool native_complete=false;exception=complete_state(p.image,p.lounge,native_complete);
            if(exception)return fail(e,"startingpoint fixture completion getter exception="+std::to_string(exception));
            if(native_complete==is_complete)return fail(e,"startingpoint fixture native completion precondition differs");
        }
        if(is_show&&!development_window(p,e))return false;
        if(!is_show&&!is_complete&&!commit_button(p,e))return false;
        Pin again;if(!pin(request,again,e)||!same(p.world,again.world)||p.lounge!=again.lounge||p.project!=again.project||p.development!=again.development||p.tutorial!=again.tutorial||!same_inventory_owner(p,again)||p.items!=again.items)
            return fail(e,e.empty()?"startingpoint fixture owners changed before native call":e);
        if(is_show){if(!development_window(again,e)||p.window!=again.window)return fail(e,e.empty()?"startingpoint fixture Development owner changed":e);}
        else if(!is_complete){if(!commit_button(again,e))return false;if(p.window!=again.window||p.button!=again.button)return fail(e,"startingpoint fixture UI owner changed before native call");}
        result["operation"]=request.operation;result["token"]=request.token;result["before"]=p.development;result["tutorial_before"]=p.tutorial;
        result["attempted"]=true;result["unknown"]=true;phase->locked=true;phase->cause="startingpoint fixture native attempt outstanding; no replay";receipts[request.token]=result;
        exception=is_show?show(p.image,p.window):(is_complete?complete(p.image,p.lounge):activate(p.image,p.button));
        result["exception_code"]=exception;result["returned"]=exception==0;
        Pin after;
        if(exception)e="startingpoint fixture native operation exception="+std::to_string(exception);
        else if(!pin(request,after,e)){result["readback_failed"]=true;}
        else if(!same(p.world,after.world)||p.lounge!=after.lounge)e="startingpoint fixture Origin/DevLounge changed after native call";
        else {
            result["after"]=after.development;result["tutorial_after"]=after.tutorial;
            const auto& d=after.development.at("project");const auto& t=after.development.at("startingpoint");
            if(is_show){
                uint8_t visible=0;
                if(!development_window(after,e)||!at(after.window,0x45f,visible,e)){result["ui_readback_failed"]=true;}
                else if(visible!=1||p.window!=after.window||p.development!=after.development||p.tutorial!=after.tutorial||!same_inventory_owner(p,after)||p.items!=after.items)e="startingpoint fixture Development Show changed owners/gameplay or did not show";
                else phase->shown=true;
            }else if(is_complete){
                auto expected=p.development;expected["project"]["work"]=d.at("work");
                bool now_complete=false;const auto code=complete_state(after.image,after.lounge,now_complete);
                if(code)e="startingpoint fixture post completion getter exception="+std::to_string(code);
                else if(!now_complete||expected!=after.development||p.project!=after.project||p.tutorial!=after.tutorial||!same_inventory_owner(p,after)||p.items!=after.items)e="startingpoint fixture completion changed fields beyond original work effect";
                else phase->completed=true;
            }else if(d.at("present")!=false||d.at("work")!=0||d.at("lines")!=0||d.at("bugs_total")!=0||d.at("bugs_known")!=0||d.at("desired_workers")!=0||
                     t.at("purchased")!=true||t.at("deployed")!=true||t.at("custom")!=false||t.at("owned")!=true||t.at("inventory_count")!=1)
                e="startingpoint fixture original Commit development/inventory readback differs";
            else if(!one_inventory_insert(p,after,e)){result["inventory_effect_failed"]=true;}
            else phase->committed=true;
        }
        if(e.empty()){phase->locked=false;phase->cause.clear();result["confirmed"]=true;result["unknown"]=false;}
        else {phase->cause=e;result["cause"]=e;}
        receipts[request.token]=result;return e.empty();
    }catch(const std::exception& x){e=std::string("startingpoint fixture exception: ")+x.what();if(result.value("attempted",false)){if(phase){phase->locked=true;phase->cause=e;}result["cause"]=e;receipts[request.token]=result;}return false;}
}
}
