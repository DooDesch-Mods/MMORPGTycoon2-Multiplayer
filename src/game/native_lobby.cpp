#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "native_lobby.h"
#include "initial_release/release_owner.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"
#include "typed_action_runtime.h"
#include "prepare_flow.h"
#include "mt2mp/net/session_wire.h"
#include "vendor/minhook/include/MinHook.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <charconv>
#include <map>
#include <algorithm>
#include "world_selection.h"

namespace mt2mp::game::native_lobby {
namespace {
uintptr_t image=0;
DWORD thread=0;
void* title=nullptr;
void* ui_owner=nullptr;int ui_mode=-1;
void* panel=nullptr;void* browser_panel=nullptr;
bool list_pending=false,list_callback_completed=false,list_callback_ok=false,publication_failed=false;std::string publication_error;size_t list_page=0;uint64_t list_generation=0,selected_lobby=0;
std::vector<std::string> saved_worlds;
std::vector<net::LobbyInfo> discovered;std::string list_status="Öffentliche Lobbys",published_state;
using Command=void(*)(void*,const void*);
Command original_command=nullptr,original_ingame_command=nullptr;
bool release_fixture_active=false;void* release_fixture_mode=nullptr;uint64_t release_fixture_callbacks=0;std::string release_fixture_error;
std::string ui_status="Multiplayer",cause,last_ui_status,selected_world,selected_file;
bool local_admitted=false;
bool choosing=false,pending=false,load_attempted=false,append_attempted=false;
bool playing_close_attempted=false;
uint64_t owner=0,generation=0,last_leave_confirmed=0;
uint64_t command_count=0;std::string last_command;
uint64_t command_failures=0;
std::map<std::string,nlohmann::json> route_receipts;
struct alignas(8) String {unsigned char bytes[32];};
bool readable(const void* p,size_t n) {
    MEMORY_BASIC_INFORMATION m{};
    if(!p||!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto offset=reinterpret_cast<uintptr_t>(p)-reinterpret_cast<uintptr_t>(m.BaseAddress);
    return offset<=m.RegionSize&&n<=m.RegionSize-offset;
}
template<class T> bool read(const void* p,size_t off,T& out) {
    if(!p)return false;const auto* q=static_cast<const unsigned char*>(p)+off;
    if(!readable(q,sizeof(T)))return false;std::memcpy(&out,q,sizeof(T));return true;
}
bool text(const void* p,std::string& out,size_t limit=4096) {
    const char* data=nullptr;size_t size=0;
    if(!read(p,0,data)||!read(p,8,size)||size>limit||!readable(data,size+1)||data[size]!=0)return false;
    out.assign(data,size);return true;
}
bool fail(const std::string& detail) {
    ++command_failures;
    if(cause!=detail)LOG_ERROR("[NativeLobby] {}",detail);
    cause=detail;ui_status="Sitzung nicht verfügbar";return false;
}
bool metadata_failure(const std::string& detail){publication_failed=true;publication_error=detail;return fail(detail);}
// All string storage is constructed and disposed by the game's own GCC runtime.
DWORD string_call(uintptr_t rva,void* receiver,const char* value,void** result=nullptr) {
    String s{};bool built=false;DWORD code=0;
    __try {
        reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(&s,value);built=true;
        if(result)*result=reinterpret_cast<void*(*)(void*,const void*)>(image+rva)(receiver,&s);
        else reinterpret_cast<void(*)(void*,const void*)>(image+rva)(receiver,&s);
    } __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(built) {__try {reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(&s);} __except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}
DWORD find_window(const char* id,void** out) {
    String s{};bool built=false;DWORD code=0;
    __try {reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(&s,id);built=true;*out=reinterpret_cast<void*(*)(const void*)>(image+0x81aa90)(&s);}
    __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(built){__try {reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(&s);}__except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}
DWORD show(void* window,bool visible) {
    __try {reinterpret_cast<void(*)(void*,bool,bool)>(image+0x81ec20)(window,visible,false);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
DWORD set_disabled(void* p,bool disabled) {
    __try {reinterpret_cast<void(*)(void*,bool)>(image+0x670e00)(p,disabled);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
DWORD activate_native(void* p,bool entry) {
    __try {
        if(entry)reinterpret_cast<void(*)(void*,void*,const void*)>(image+0x6a6220)(p,nullptr,nullptr);
        else reinterpret_cast<void(*)(void*)>(image+0x674780)(p);
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
DWORD latest_save(void* mode,const char* world,String* output,bool* returned) {
    String input{};bool built=false;DWORD code=0;
    __try {
        reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(&input,world);built=true;
        reinterpret_cast<void*(*)(void*,void*,const void*)>(image+0x4b9910)(output,mode,&input);*returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(built){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(&input);}__except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}
DWORD dispose(String* value) {
    __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(value);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
void* pane(const char* id) {
    void* window=std::string_view(id).starts_with("list")?browser_panel:panel;
    void* out=nullptr;const auto exception=string_call(0x81d860,window,id,&out);
    if(exception||!out)fail("native pane lookup failed id="+std::string(id)+" exception="+std::to_string(exception));
    return out;
}
bool label(const char* id,const std::string& value) {
    auto* p=pane(id);if(!p)return false;
    const auto exception=string_call(0x7f1630,p,value.c_str());
    return !exception||fail("native text setter failed id="+std::string(id)+" exception="+std::to_string(exception));
}
bool disable(const char* id,bool disabled) {
    auto* p=pane(id);if(!p)return false;const auto code=set_disabled(p,disabled);
    return !code||fail("native button state failed id="+std::string(id)+" exception="+std::to_string(code));
}
bool close_playing_panel_once(bool ingame_active,const nlohmann::json& preparation,std::string& error){
    if(!ingame_active||!preparation.value("playing",false)||playing_close_attempted)return true;
    playing_close_attempted=true;
    const auto exception=show(panel,false);
    if(exception){error="native playing panel close failed exception="+std::to_string(exception);return fail(error);}
    return true;
}
// The game's Desktop owns the entry through its native content array.
// Mode windows keep the session panels; no entry is added to those arrays.
void* desktop_attempt_owner=nullptr;bool desktop_append_attempted=false;
DWORD queue_layout(void* desktop) {
    __try {reinterpret_cast<void(*)(void*)>(image+0x81a330)(desktop);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
bool desktop_entry(void* game,bool title_active,void** entry,bool* visible,std::string& error) {
    *entry=nullptr;*visible=false;void* desktop=nullptr;
    if(!read(game,0x1e8,desktop)||!desktop){error="native game Desktop owner absent";return false;}
    uintptr_t vtable=0;
    if(!read(desktop,0,vtable)||vtable!=image+0x16c02d0){error="native game Desktop class differs from exact executable";return false;}
    if(desktop_attempt_owner!=desktop){desktop_attempt_owner=desktop;desktop_append_attempted=false;}
    auto code=string_call(0x816150,desktop,"entry_button",entry);
    if(code){error="native Desktop entry lookup failed exception="+std::to_string(code);return false;}
    if(!*entry&&title_active) {
        if(desktop_append_attempted){error="native Desktop entry append already attempted; no replay";return false;}
        desktop_append_attempted=true;
        code=string_call(0xbb2e60,static_cast<unsigned char*>(desktop)+0x2c8,"mt2mp_ui/entry.win");
        if(code||string_call(0x816150,desktop,"entry_button",entry)||!*entry){error="native Desktop content append/ACK failed; no replay";return false;}
        code=queue_layout(desktop);
        if(code){error="native Desktop entry layout queue failed exception="+std::to_string(code);return false;}
        LOG_INFO("[NativeLobby] Multiplayer entry adopted by native Desktop content");
    }
    uint8_t shown=0;void* parent=nullptr;void* window=nullptr;
    if(!read(desktop,0x45f,shown)){error="native Desktop visibility unreadable";return false;}
    if(*entry) {
        if(!read(*entry,0x278,parent)||!read(*entry,0x280,window)){error="native Desktop entry ancestry unreadable";return false;}
        *visible=title_active&&shown&&parent==desktop&&window==desktop;
    }
    return true;
}
bool current_desktop_entry(void** entry,bool* visible,std::string& error) {
    native::WorldStatus world;if(!native::read_world_status(world,error))return false;
    void* game=nullptr;
    if(!read(reinterpret_cast<void*>(image),0x13df8f0,game)||!game){error="native Desktop game owner absent";return false;}
    return desktop_entry(game,world.active_mode_index==2&&!world.title_transition_pending,entry,visible,error);
}
bool refresh_saved_worlds(std::string& error) {
    using Enumerate=char**(*)(const char*);
    using FreeList=void(*)(void*);
    using IsDirectory=int(*)(const char*);
    const auto dll=GetModuleHandleW(L"libphysfs.dll");
    const auto enumerate=dll?reinterpret_cast<Enumerate>(GetProcAddress(dll,"PHYSFS_enumerateFiles")):nullptr;
    const auto release=dll?reinterpret_cast<FreeList>(GetProcAddress(dll,"PHYSFS_freeList")):nullptr;
    const auto is_directory=dll?reinterpret_cast<IsDirectory>(GetProcAddress(dll,"PHYSFS_isDirectory")):nullptr;
    if(!enumerate||!release||!is_directory){error="native save enumeration exports unavailable";return false;}
    saved_worlds.clear();
    if(!is_directory("user/save")){error.clear();return true;}
    auto** worlds=enumerate("user/save");
    if(!worlds){error="native save directory enumeration failed";return false;}
    for(auto** item=worlds;*item;++item){
        const std::string name=*item;
        if(!world_selection::valid_name(name))continue;
        const auto path="user/save/"+name;
        if(!is_directory(path.c_str()))continue;
        auto** saves=enumerate(path.c_str());
        if(!saves){release(worlds);error="native save enumeration failed world="+name;return false;}
        bool found=false;
        for(auto** save=saves;*save;++save)if(world_selection::save_file(*save)){found=true;break;}
        release(saves);
        if(found)saved_worlds.push_back(name);
    }
    release(worlds);
    std::sort(saved_worlds.begin(),saved_worlds.end());
    error.clear();return true;
}
bool select_world(void* mode,const std::string& command) {
    const std::string prefix="Connect ";
    if(!command.starts_with(prefix))return false;
    std::string world=command.substr(prefix.size());
    if(world.size()>=2&&world.front()=='"'&&world.back()=='"')world=world.substr(1,world.size()-2);
    if(!world_selection::valid_name(world)) {fail("native world selection name invalid");return true;}
    String output{};bool returned=false;DWORD code=latest_save(mode,world.c_str(),&output,&returned);
    std::string stem;const bool valid=!code&&returned&&text(&output,stem,200);
    if(returned){const auto cleanup=dispose(&output);if(!code)code=cleanup;}
    if(!valid||code||stem.empty()||stem.find_first_of("/\\\"\r\n\t")!=std::string::npos){fail("native latest world save unavailable exception="+std::to_string(code));return true;}
    selected_world=world;selected_file="save/"+world+"/"+stem+".vrz";choosing=false;
    std::string metadata_error;if(!Config::instance().local_test&&!net::SteamMatchmaking::instance().publish_lobby_data("game_name",world,metadata_error)){metadata_failure(metadata_error);return true;}
    const auto hidden=show(browser_panel,false);
    if(hidden){fail("native world selection window close failed exception="+std::to_string(hidden));return true;}
    const auto shown=show(panel,true);
    if(shown)fail("native lobby show after world selection failed exception="+std::to_string(shown));
    else {ui_status="Welt ausgewählt";LOG_INFO("[NativeLobby] World selection captured without load path={}",selected_file);}
    return true;
}
bool lobby_host(){return admitted()&&(Config::instance().local_test?Config::instance().peer_id==1:net::SteamMatchmaking::instance().is_host());}
bool admit_local(bool host){
    const auto peer=net::session_wire::local_peer();const auto mask=net::session_wire::peer_mask();
    if(!net::session_wire::connected()||peer<1||peer>4||(mask&3)!=3||!(mask&(1u<<(peer-1))))return fail("local lobby admission awaits confirmed launcher peer membership");
    if(host!=(peer==1))return fail("local lobby action differs from launcher-owned Host/Client role");
    local_admitted=true;Config::instance().is_host=peer==1;++generation;cause.clear();ui_status="Lokale Lobby offen";return true;
}
std::array<std::string,4> player_rows(){
    std::array<std::string,4> rows;rows.fill("Freier Platz");
    if(Config::instance().local_test){
        const auto mask=admitted()?net::session_wire::peer_mask():0;
        for(size_t i=0;i<4;++i)if(mask&(1u<<i))rows[i]="Lokaler Peer "+std::to_string(i+1)+(i==0?" - Host":" - Mitspieler");
    }else{
        const auto members=net::SteamMatchmaking::instance().get_lobby_members();
        for(size_t i=0;i<rows.size()&&i<members.size();++i)rows[i]=net::SteamNetworking::instance().get_persona_name(members[i])+(members[i]==owner?" - Host":" - Mitspieler");
    }
    return rows;
}
bool ready_for_start() {
    return lobby_prepare::status().value("ready",false);
}
void dispatch(void* mode,const void* value,Command original) {
    std::string cmd;
    if(!text(value,cmd)) {LOG_WARN("[NativeLobby] Native command unreadable; original continues");original(mode,value);return;}
    if(choosing&&ui_mode==2&&mode==title&&select_world(mode,cmd))return;
    if(!cmd.starts_with("MT2MP")){original(mode,value);return;}
    if(GetCurrentThreadId()!=thread||mode!=ui_owner){fail("native UI command outside bound active mode thread");return;}
    ++command_count;last_command=cmd;
    auto& mm=net::SteamMatchmaking::instance();
    if(publication_failed&&cmd!="MT2MPLeave"&&cmd!="MT2MPOpen"){fail("Host metadata unavailable: "+publication_error);return;}
    if(cmd=="MT2MPOpen") {void* entry=nullptr;bool visible=false;std::string error;if(!current_desktop_entry(&entry,&visible,error)||!visible){fail(error.empty()?"Multiplayer entry requires visible main-menu Desktop":error);return;}const auto e=show(panel,true);if(e)fail("native panel show failed exception="+std::to_string(e));return;}
    if(cmd=="MT2MPRefresh"&&choosing){
        std::string error;if(!refresh_saved_worlds(error)){fail(error);return;}
        list_page=0;list_status=saved_worlds.empty()?"Keine gespeicherten Welten":"Welt auswählen";return;
    }
    if(cmd=="MT2MPSearch"||cmd=="MT2MPRefresh"){
        if(ui_mode!=2||pending||admitted()||list_pending){fail("native discovery requires idle Title and no pending request");return;}
        if(!net::SteamNetworking::instance().init()||!mm.init()){fail("Steam discovery interface unavailable");return;}
        if(show(panel,false)||show(browser_panel,true)){fail("native discovery window visibility failed");return;}
        list_pending=true;list_callback_completed=list_callback_ok=false;cause.clear();list_page=0;selected_lobby=0;discovered.clear();list_status="Lobbys werden gesucht";const auto current=++list_generation;
        mm.request_lobby_list([current](bool okay,const std::vector<net::LobbyInfo>& rows){
            if(current!=list_generation){LOG_WARN("[NativeLobby] Stale discovery callback discarded generation={}",current);return;}
            list_pending=false;list_callback_completed=true;list_callback_ok=okay;discovered=rows;list_page=0;
            list_status=okay?(rows.empty()?"Keine öffentlichen Lobbys gefunden":"Öffentliche Lobbys: "+std::to_string(rows.size())):"Lobby-Suche fehlgeschlagen";
            if(!okay)LOG_ERROR("[NativeLobby] Steam discovery callback failed; see SDK callback/timeout cause");
            for(auto& [token,receipt]:route_receipts)if(receipt.value("discovery_generation",uint64_t{})==current){receipt["callback_pending"]=false;receipt["callback_completed"]=true;receipt["backend_confirmed"]=okay;receipt["lobby_count"]=rows.size();}
        });return;
    }
    if(cmd=="MT2MPListBack"){choosing=false;if(show(browser_panel,false)||show(panel,true))fail("native discovery Back visibility failed");return;}
    if(cmd=="MT2MPListPrevious"||cmd=="MT2MPListNext"){
        if(list_pending){fail("native discovery page awaits callback");return;}
        if(cmd=="MT2MPListPrevious"){if(list_page) --list_page;}
        else if((list_page+1)*4<(choosing?saved_worlds.size():discovered.size()))++list_page;return;
    }
    if(cmd.size()==10&&cmd.starts_with("MT2MPList")&&cmd.back()>='0'&&cmd.back()<='3'){
        const auto index=list_page*4+size_t(cmd.back()-'0');
        if(choosing){
            if(index>=saved_worlds.size()){fail("native selected world absent from current list");return;}
            select_world(mode,"Connect "+saved_worlds[index]);return;
        }
        if(list_pending||index>=discovered.size()){fail("native selected lobby is not in completed current discovery");return;}
        const auto& row=discovered[index];
        if(row.state!="open"||row.player_count<1||row.max_players<=row.player_count){fail("native selected lobby is not open or has no free slot");return;}
        selected_lobby=row.lobby_id;
        if(!label("lobby_id",std::to_string(selected_lobby))||show(browser_panel,false)||show(panel,true)){fail("native selected lobby field/visibility failed");return;}
        ui_status="Ausgewählt: "+row.host_name;return;
    }
    if(cmd=="MT2MPPickWorld") {
        if(ui_mode!=2||pending||load_attempted||!lobby_host()){fail("world selection rejected: Host with idle Title lobby required");return;}
        std::string error;
        if(!refresh_saved_worlds(error)){fail(error);return;}
        if(saved_worlds.empty()){ui_status="Keine gespeicherte Welt. Zuerst eine kleine Welt erstellen und speichern.";return;}
        choosing=true;list_page=0;list_status="Welt auswählen";
        if(show(panel,false)||show(browser_panel,true)){choosing=false;fail("native world selection window visibility failed");}
        return;
    }
    if(cmd=="MT2MPHost"||cmd=="MT2MPHostPublic"||cmd=="MT2MPJoin") {
        if(ui_mode!=2||pending||admitted()){fail("lobby admission rejected: idle Title and no active lobby required");return;}
        if(Config::instance().local_test){admit_local(cmd!="MT2MPJoin");return;}
        if(!net::SteamNetworking::instance().init()||!mm.init()){fail("Steam lobby interface unavailable");return;}
        pending=true;cause.clear();ui_status="Verbindung wird hergestellt";const auto current=++generation;
        const bool host=cmd!="MT2MPJoin";const bool public_lobby=cmd=="MT2MPHostPublic";
        const auto completion=[current,host,public_lobby](bool ok,uint64_t id){
            if(current!=generation){LOG_WARN("[NativeLobby] Cancelled admission callback discarded generation={}",current);return;}
            for(auto& [token,receipt]:route_receipts)if(receipt.value("admission_generation",uint64_t{})==current&&receipt.value("button",std::string{})==(host?(public_lobby?"host_public":"host"):"join")){
                receipt["callback_pending"]=false;receipt["callback_completed"]=true;receipt["backend_confirmed"]=ok&&id!=0;receipt["steam_lobby_id"]=id;
            }
            pending=false;if(!ok||!id){fail("Steam lobby admission failed");return;}
            Config::instance().is_host=host;owner=net::SteamMatchmaking::instance().host_steam_id();ui_status="Lobby offen";
            if(host){std::string error;auto& mm=net::SteamMatchmaking::instance();
                if(!mm.publish_lobby_data("mt2mp","1",error)||!mm.publish_lobby_data("host_name",net::SteamNetworking::instance().get_persona_name(owner),error)||
                   !mm.publish_lobby_data("mt2mp_version",MOD_VERSION,error)||!mm.publish_lobby_data("game_name","Welt wird ausgewählt",error)||
                   !mm.publish_lobby_data("lobby_state","open",error)||!mm.set_lobby_joinable(true,error)){metadata_failure(error);return;}
                published_state="open";
            }
        };
        if(host)mm.create_lobby(4,completion,public_lobby);
        else {
            std::string id;auto* field=pane("lobby_id");uint64_t number=0;
            if(!field||!text(static_cast<unsigned char*>(field)+0x2d0,id,20)){pending=false;fail("native lobby ID field unreadable");return;}
            auto parse=std::from_chars(id.data(),id.data()+id.size(),number);
            if(parse.ec!=std::errc{}||parse.ptr!=id.data()+id.size()||!number){pending=false;ui_status="Ungültige Lobby-ID";return;}
            mm.join_lobby(number,completion);
        }
        return;
    }
    if(cmd=="MT2MPPrepare") {
        if(!admitted()||!lobby_host()||selected_file.empty()||load_attempted){fail("world preparation rejected: selected Host world required and no prior attempt");return;}
        std::string error;
        if(!lobby_prepare::prepare(selected_file,error)){fail("native world preparation failed: "+error);return;}
        load_attempted=true;ui_status="Welt wird geladen";return;
    }
    if(cmd=="MT2MPStart") {
        if(!admitted()||!lobby_host()||selected_file.empty()){fail("start requires confirmed Host and selected world");return;}
        std::string error;
        if(!load_attempted){if(!lobby_prepare::prepare(selected_file,error)){fail(error);return;}load_attempted=true;}
        if(!lobby_prepare::start(error)){fail(error);return;}
        ui_status="Sitzung wird vorbereitet";return;
    }
    if(cmd=="MT2MPLeave") {
        if(pending){fail("native Leave awaits actual pending admission callback; no cancellation replay");return;}
        std::string error;if(!lobby_prepare::leave(error)){fail("native Leave failed: "+error);return;}
        ++generation;pending=false;choosing=false;ui_status="Sitzung wird beendet";return;
    }
    if(cmd=="MT2MPInvite") {
        const auto steam=GetModuleHandleA("steam_api64_original.dll");
        using Access=void*(*)();using Enabled=bool(*)(void*);using Invite=void(*)(void*,uint64_t);
        auto friends=steam?reinterpret_cast<Access>(GetProcAddress(steam,"SteamAPI_SteamFriends_v017")):nullptr;
        auto utils=steam?reinterpret_cast<Access>(GetProcAddress(steam,"SteamAPI_SteamUtils_v010")):nullptr;
        auto enabled=steam?reinterpret_cast<Enabled>(GetProcAddress(steam,"SteamAPI_ISteamUtils_IsOverlayEnabled")):nullptr;
        auto invite=steam?reinterpret_cast<Invite>(GetProcAddress(steam,"SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog")):nullptr;
        auto* f=friends?friends():nullptr;auto* u=utils?utils():nullptr;
        if(!mm.in_lobby()||!f||!u||!enabled||!enabled(u)||!invite){ui_status="Steam-Overlay nicht verfügbar";LOG_WARN("[NativeLobby] Invite dialog unavailable; no invitation request");return;}
        invite(f,mm.current_lobby_id());ui_status="Steam-Einladungsdialog angefordert";
        LOG_INFO("[NativeLobby] Steam invite dialog requested; recipient selection and delivery not confirmed");return;
    }
    fail("unknown native lobby command "+cmd);
}
void command(void* mode,const void* value){dispatch(mode,value,original_command);}
void ingame_command(void* mode,const void* value){
    if(release_fixture_active){std::string cmd;
        if(GetCurrentThreadId()!=thread||mode!=release_fixture_mode)release_fixture_error="release fixture actual Mode callback owner/thread differs";
        else if(!text(value,cmd))release_fixture_error="release fixture original Mode callback string unreadable";
        else if(cmd=="LevelUp")++release_fixture_callbacks;
        else release_fixture_error="release fixture original Mode callback is not LevelUp";
    }
    dispatch(mode,value,original_ingame_command);
}
}
bool initialize(const std::filesystem::path& assets,uintptr_t base,std::string& error) {
    if(image){error="native lobby already initialized";return false;}
    uintptr_t resolved=0;uint32_t game_thread=0;std::string digest;
    if(!native::read_resolver_receipt(resolved,game_thread,digest,error)||resolved!=base||
       digest!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6") {
        error="native lobby exact executable resolver mismatch: "+error;return false;
    }
    constexpr unsigned char command_prologue[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53};
    const auto* command_entry=reinterpret_cast<const void*>(base+0x4b9be0);
    if(!readable(command_entry,sizeof(command_prologue))||std::memcmp(command_entry,command_prologue,sizeof(command_prologue))) {
        error="native title Command entry differs from exact executable bytes";return false;
    }
    const auto* ingame_entry=reinterpret_cast<const void*>(base+0x4a2f90);
    if(!readable(ingame_entry,sizeof(command_prologue))||std::memcmp(ingame_entry,command_prologue,sizeof(command_prologue))){error="native in-game Command entry differs from exact executable bytes";return false;}
    if(!std::filesystem::is_regular_file(assets/"entry.win")||!std::filesystem::is_regular_file(assets/"lobby.win")||!std::filesystem::is_regular_file(assets/"browser.win")||!std::filesystem::is_regular_file(assets/"multiplayer.png")||!std::filesystem::is_regular_file(assets/"materials/mt2mp_desktop_multiplayer.mat")){error="native lobby authored .win assets missing";return false;}
    const auto module=GetModuleHandleW(L"libphysfs.dll");
    using Mount=int(*)(const char*,const char*,int);using RealDir=const char*(*)(const char*);
    auto mount=module?reinterpret_cast<Mount>(GetProcAddress(module,"PHYSFS_mount")):nullptr;
    auto real_dir=module?reinterpret_cast<RealDir>(GetProcAddress(module,"PHYSFS_getRealDir")):nullptr;
    if(!mount||!real_dir||!mount(assets.string().c_str(),"mt2mp_ui",1)||!real_dir("mt2mp_ui/entry.win")){error="native lobby private PhysFS mount/ack failed";return false;}
    if(!mount((assets/"materials").string().c_str(),"materials",1)||!real_dir("materials/mt2mp_desktop_multiplayer.mat")||!real_dir("mt2mp_ui/multiplayer.png")){error="native Desktop icon material/texture mount ACK failed";return false;}
    image=base;
    if(!hooks::create_hook(reinterpret_cast<void*>(image+0x4b9be0),reinterpret_cast<void*>(&command),reinterpret_cast<void**>(&original_command))) {
        image=0;error="native title Command hook creation failed";return false;
    }
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(image+0x4b9be0));
    if(enabled!=MH_OK){error="native title Command hook enable failed: "+std::string(MH_StatusToString(enabled));return false;}
    if(!hooks::create_hook(reinterpret_cast<void*>(image+0x4a2f90),reinterpret_cast<void*>(&ingame_command),reinterpret_cast<void**>(&original_ingame_command))){error="native in-game Command hook creation failed";return false;}
    const auto ingame_enabled=MH_EnableHook(reinterpret_cast<void*>(image+0x4a2f90));
    if(ingame_enabled!=MH_OK){error="native in-game Command hook enable failed: "+std::string(MH_StatusToString(ingame_enabled));return false;}
    error.clear();return true;
}
bool tick(std::string& error) {
    if(!image){error="native lobby not initialized";return false;}
    if(!thread)thread=GetCurrentThreadId();
    if(thread!=GetCurrentThreadId()){error="native lobby tick outside bound game thread";return false;}
    auto& mm=net::SteamMatchmaking::instance();
    mm.poll();
    if(!Config::instance().local_test&&mm.in_lobby()&&!owner){owner=mm.host_steam_id();Config::instance().is_host=owner!=0&&owner==net::SteamNetworking::instance().local_steam_id();}
    const auto preparation=lobby_prepare::status();
    const auto left=preparation.value("leave_confirmed",uint64_t{});
    if(left!=last_leave_confirmed){last_leave_confirmed=left;playing_close_attempted=false;local_admitted=false;owner=0;selected_world.clear();selected_file.clear();load_attempted=false;selected_lobby=0;publication_failed=false;publication_error.clear();published_state.clear();cause.clear();ui_status="Sitzung beendet";}
    if(preparation.value("state",0)!=0)ui_status=preparation.value("label",std::string("Sitzung nicht verfügbar"));
    native::WorldStatus world;
    if(!native::read_world_status(world,error))return false;
    const bool title_active=world.active_mode_index==2&&!world.title_transition_pending;
    const bool ingame_active=world.active_mode_index==3&&native::in_game();
    if(!title_active&&!ingame_active){ui_owner=nullptr;panel=nullptr;browser_panel=nullptr;ui_mode=-1;append_attempted=false;last_ui_status.clear();error.clear();return true;}
    void* game=nullptr;void* mode=nullptr;void* active=nullptr;
    const size_t mode_offset=title_active?0x350:0x358;
    if(!read(reinterpret_cast<void*>(image),0x13df8f0,game)||!read(game,mode_offset,mode)||!read(game,0x28,active)||mode!=active){error="native UI owner differs from actual active mode";return false;}
    if(ui_owner!=mode||ui_mode!=world.active_mode_index){ui_owner=mode;ui_mode=world.active_mode_index;panel=nullptr;browser_panel=nullptr;append_attempted=false;last_ui_status.clear();}
    if(title_active)title=mode;
    void* entry=nullptr;bool entry_visible=false;
    if(!desktop_entry(game,title_active,&entry,&entry_visible,error))return false;
    if(find_window("mt2mp_lobby",&panel)||find_window("mt2mp_browser",&browser_panel)){error="native lobby registry read failed";return false;}
    if(!panel||!browser_panel) {
        if(append_attempted){error="native lobby append already attempted in this mode epoch; no replay";return false;}
        append_attempted=true;
        if(panel||browser_panel){error="native lobby partial native window append; no replay";return false;}
        const auto p=string_call(0xbbe090,static_cast<unsigned char*>(ui_owner)+(title_active?0x40:0x98),"mt2mp_ui/lobby.win");
        const auto b=string_call(0xbbe090,static_cast<unsigned char*>(ui_owner)+(title_active?0x40:0x98),"mt2mp_ui/browser.win");
        if(p||b||find_window("mt2mp_lobby",&panel)||find_window("mt2mp_browser",&browser_panel)||!panel||!browser_panel){error="native lobby window append/registry ACK failed; no replay";return false;}
        if(show(panel,ingame_active&&admitted())||show(browser_panel,false)){error="native lobby initial visibility failed";return false;}
        LOG_INFO("[NativeLobby] Native panels registered on mode-owned window array mode={}",ui_mode);
    }
    if(!close_playing_panel_once(ingame_active,preparation,error))return false;
    if(!Config::instance().local_test&&owner&&mm.host_steam_id()!=owner&&!preparation.value("leaving",false)){std::string detail;if(!lobby_prepare::leave(detail))fail("Host connection lost; native retirement failed: "+detail);else ui_status="Hostverbindung verloren";}
    if(!Config::instance().local_test&&!publication_failed&&mm.in_lobby()&&mm.is_host()){
        const std::string state=preparation.value("leaving",false)?"leaving":preparation.value("playing",false)?"playing":
            (load_attempted||net::session_wire::connected())?"preparing":"open";
        if(state!=published_state){std::string detail;if(!mm.publish_lobby_data("lobby_state",state,detail)||!mm.set_lobby_joinable(state=="open",detail))metadata_failure(detail);else published_state=state;}
    }
    if(!label("list_status",list_status)){error=cause;return false;}
    for(size_t slot=0;slot<4;++slot){const auto index=list_page*4+slot;std::string value="-";bool disabled=true;
        if(choosing){
            if(index<saved_worlds.size()){value=saved_worlds[index];disabled=false;}
        }else if(index<discovered.size()){const auto& row=discovered[index];
            const std::string state=row.state=="open"?"Offen":row.state=="preparing"?"Wird vorbereitet":row.state=="playing"?"Im Spiel":row.state=="leaving"?"Wird beendet":"Nicht verfügbar";
            value=row.host_name+"\n"+row.save_name+" - "+(row.state=="unavailable"?state:std::to_string(row.player_count)+"/"+std::to_string(row.max_players)+" Spieler - "+state);
            disabled=list_pending||row.state!="open"||row.player_count<1||row.max_players<=row.player_count;
        }
        const auto id="list"+std::to_string(slot);if(!label(("list_text"+std::to_string(slot)).c_str(),value)||!disable(id.c_str(),disabled)){error=cause;return false;}
    }
    if(!disable("list_refresh",list_pending)||!disable("list_previous",list_pending||!list_page)||!disable("list_next",list_pending||(list_page+1)*4>=(choosing?saved_worlds.size():discovered.size()))){error=cause;return false;}
    if(publication_failed)ui_status="Host-Metadaten nicht verfügbar";
    if(last_ui_status!=ui_status){if(!label("status",ui_status)){error=cause;return false;}last_ui_status=ui_status;}
    const auto shared=net::snapshot_progress();
    const bool shared_client_world=!Config::instance().is_host&&ingame_active&&admitted()&&preparation.value("ready",false)&&
        shared.value("phase",std::string{})=="active"&&shared.value("world_authority_valid",false)&&
        !preparation.value("snapshot_id",std::string{}).empty()&&shared.value("id",std::string{})==preparation.value("snapshot_id",std::string{});
    const std::string world_label=selected_world.empty()?(shared_client_world?"Gemeinsame Welt geladen":"Keine Welt ausgewählt"):selected_world;
    if(!label("world",world_label)){error=cause;return false;}
    const auto rows=player_rows();
    for(size_t i=0;i<rows.size();++i)if(!label(("player"+std::to_string(i)).c_str(),rows[i])){error=cause;return false;}
    if(!disable("host_public",!title_active||pending||admitted())||!disable("search",!title_active||pending||admitted()||list_pending)||!disable("host",!title_active||pending||admitted())||!disable("join",!title_active||pending||admitted())||!disable("pick",!title_active||!lobby_host()||pending||load_attempted)||
       !disable("leave",pending||preparation.value("leaving",false))||!disable("prepare",publication_failed||!lobby_host()||selected_file.empty()||load_attempted)||
       !disable("start",publication_failed||!lobby_host()||selected_file.empty()||pending||preparation.value("start_requested",false)||!preparation.value("error",std::string{}).empty())){error=cause;return false;}
    error.clear();return true;
}
bool admitted(){return Config::instance().local_test?local_admitted&&net::session_wire::connected():net::SteamMatchmaking::instance().in_lobby();}
std::string status(){return cause.empty()?ui_status:cause;}
bool observe(nlohmann::json& result,std::string& error) {
    if(!image||!thread||GetCurrentThreadId()!=thread){error="native lobby observation requires initialized bound game thread";return false;}
    native::WorldStatus world;if(!native::read_world_status(world,error))return false;
    if(!tick(error))return false;
    result={{"title_active",world.active_mode_index==2&&!world.title_transition_pending},{"state",status()},
        {"transport",Config::instance().local_test?"local":"steam"},{"admission_source",Config::instance().local_test?"launcher_loopback":"steam_callback"},{"admitted",admitted()},{"host_permission",lobby_host()},
        {"local_admitted",Config::instance().local_test&&local_admitted},{"local_role",Config::instance().local_test?(net::session_wire::local_peer()==1?"host":"client"):"none"},{"local_session_id",Config::instance().local_test?net::session_wire::session_id():0},
        {"local_peer",Config::instance().local_test?net::session_wire::local_peer():0},{"local_peer_mask",Config::instance().local_test?net::session_wire::peer_mask():0},{"wire_connected",net::session_wire::connected()},{"player_rows",player_rows()},{"playing_close_attempted",playing_close_attempted},
        {"generation",generation},{"commands",command_count},{"last_command",last_command},{"command_failures",command_failures},
        {"metadata_failed",publication_failed},{"metadata_error",publication_error},{"discovery_callback_completed",list_callback_completed},{"discovery_callback_ok",list_callback_ok},{"discovery_pending",list_pending},{"discovery_generation",list_generation},{"discovery_state",list_status},{"discovery_page",list_page},{"selected_lobby",selected_lobby},{"lobbies",nlohmann::json::array()},{"callback_pending",pending},{"selected_world",selected_world},{"selected_file",selected_file},{"world_selection_open",choosing},{"saved_worlds",saved_worlds},{"prepare",lobby_prepare::status()},
        {"steam_lobby_id",net::SteamMatchmaking::instance().current_lobby_id()},{"steam_host_id",net::SteamMatchmaking::instance().host_steam_id()},
        {"steam_in_lobby",net::SteamMatchmaking::instance().in_lobby()},{"steam_members",net::SteamMatchmaking::instance().get_lobby_members()},
        {"native_panes",nlohmann::json::array()},{"route_receipts",nlohmann::json::array()}};
    for(const auto& row:discovered)result["lobbies"].push_back({{"lobby_id",row.lobby_id},{"host",row.host_name},{"world",row.save_name},{"players",row.player_count},{"slots",row.max_players},{"state",row.state}});
    for(const auto& [token,receipt]:route_receipts)result["route_receipts"].push_back(receipt);
    result["ui_mode"]=ui_mode;result["ui_owner_current"]=ui_owner!=nullptr;
    result["snapshot"]=net::snapshot_progress();result["controller"]=typed_action_runtime::status();
    result["physical_handshake"]=net::session_wire::connected();
    if(!ui_owner){error.clear();return true;}
    void* entry=nullptr;bool visible=false;if(!current_desktop_entry(&entry,&visible,error))return false;
    result["entry_registered"]=entry!=nullptr;result["entry_visible"]=visible;
    result["entry_owner"]="native_game_desktop";result["entry_in_mode_window_array"]=false;
    result["panel_registered"]=panel!=nullptr;
    void* legacy_entry=nullptr;if(find_window("mt2mp_entry",&legacy_entry)){error="native legacy entry registry read failed";return false;}
    result["legacy_entry_window_registered"]=legacy_entry!=nullptr;
    for(const char* id:{"host","host_public","search","join","pick","prepare","start","leave","list_refresh","list_previous","list_next","list_back","list0","list1","list2","list3"}){
        auto* p=pane(id);uint8_t disabled=1;std::string actual;
        if(!p||!read(p,0x46d,disabled)||!text(static_cast<unsigned char*>(p)+0x388,actual)){
            error="native lobby button field observation failed id="+std::string(id);return false;}
        result["native_panes"].push_back({{"id",id},{"disabled",disabled!=0},{"native_command",actual}});
    }
    for(const char* id:{"status","world","player0","player1","player2","player3","list_status","list_text0","list_text1","list_text2","list_text3"}){
        auto* p=pane(id);std::string actual;
        if(!p||!text(static_cast<unsigned char*>(p)+0x2d0,actual)){error="native lobby text readback failed id="+std::string(id);return false;}
        result["native_labels"][id]=actual;
    }
    error.clear();return true;
}
bool activate(const nlohmann::json& args,nlohmann::json& result,std::string& error) {
    if(!args.is_object()||args.size()<2||args.size()>3||!args.contains("acceptance_token")||!args.contains("button")||
       !args.at("acceptance_token").is_string()||!args.at("button").is_string()){
        error="native lobby activation requires exact token/button and optional lobby_id";return false;}
    const auto token=args.at("acceptance_token").get<std::string>();const auto button=args.at("button").get<std::string>();
    if(token.size()!=36||token.find_first_not_of("0123456789abcdef-")!=std::string::npos||token[8]!='-'||token[13]!='-'||token[18]!='-'||token[23]!='-'){
        error="native lobby acceptance token invalid";return false;}
    if(args.size()==3&&!((button=="join"&&args.contains("lobby_id")&&args.at("lobby_id").is_string())||
       (button=="pick"&&args.contains("world")&&args.at("world").is_string()))){
        error="native lobby optional field requires Join lobby_id or Pick world";return false;}
    std::string world_name;
    if(args.contains("world")){world_name=args.at("world").get<std::string>();
        if(world_name.empty()||world_name.size()>200||world_name.find_first_of("/\\\"\r\n\t")!=std::string::npos||world_name==".."){
            error="native world selection name invalid";return false;}}
    const std::map<std::string,std::string> admitted_buttons={{"open","MT2MPOpen"},{"host","MT2MPHost"},{"host_public","MT2MPHostPublic"},{"search","MT2MPSearch"},{"join","MT2MPJoin"},{"list_refresh","MT2MPRefresh"},{"list_previous","MT2MPListPrevious"},{"list_next","MT2MPListNext"},{"list_back","MT2MPListBack"},{"list0","MT2MPList0"},{"list1","MT2MPList1"},{"list2","MT2MPList2"},{"list3","MT2MPList3"},
        {"pick","MT2MPPickWorld"},{"prepare","MT2MPPrepare"},{"start","MT2MPStart"},{"leave","MT2MPLeave"}};
    const auto expected=admitted_buttons.find(button);if(expected==admitted_buttons.end()){error="native lobby button is not admitted; invitation delivery excluded";return false;}
    if(route_receipts.contains(token)){result=route_receipts.at(token);error="native lobby token has already been attempted; read receipt, no replay";return false;}
    if(route_receipts.size()>=64){error="native lobby receipt limit reached required<64";return false;}
    nlohmann::json current;if(!observe(current,error))return false;
    if(!current.value("ui_owner_current",false)||!current.value("panel_registered",false)){
        error="native lobby button requires actual current mode-owned windows";return false;}
    if(!Config::instance().local_test&&button=="join"&&args.contains("lobby_id")){
        const auto id=args.at("lobby_id").get<std::string>();uint64_t number=0;const auto parsed=std::from_chars(id.data(),id.data()+id.size(),number);
        if(id.empty()||id.size()>20||parsed.ec!=std::errc{}||parsed.ptr!=id.data()+id.size()||!number){error="native Join lobby ID invalid";return false;}
        if(!label("lobby_id",id)){error=cause;return false;}
    }
    void* target=nullptr;bool entry=button=="open";
    if(entry){bool visible=false;if(!current_desktop_entry(&target,&visible,error)||!target||!visible){if(error.empty())error="actual main-menu Desktop ClickView unavailable or hidden";return false;}}
    else target=pane(button.c_str());
    std::string native_command;uint8_t disabled=0;
    if(!target||(!entry&&!read(target,0x46d,disabled))||disabled||
       !text(static_cast<unsigned char*>(target)+(entry?0x318:0x388),native_command)||native_command!="MessageTo Mode "+expected->second){
        error="native lobby target disabled or command differs from authored button";return false;}
    const auto before=command_count,failed=command_failures;
    result={{"acceptance_token",token},{"button",button},{"native_attempted",true},{"native_returned",false},{"handler_reached",false},{"backend_confirmed",false}};
    route_receipts[token]=result;
    const auto code=activate_native(target,entry);
    result["native_returned"]=code==0;result["exception_code"]=code;
    result["handler_reached"]=command_count==before+1&&last_command==expected->second;
    DWORD selection_code=0;
    if(!code&&button=="pick"&&!world_name.empty()&&choosing){
        selection_code=string_call(0x4b9be0,title,("Connect \""+world_name+"\"").c_str());
        result["selection_route"]="native Title.Command and GetLatestSaveForGame";
        result["selected_world"]=selected_world;result["selected_file"]=selected_file;
        result["selection_confirmed"]=selection_code==0&&!choosing&&selected_world==world_name&&!selected_file.empty();
    }
    if(Config::instance().local_test&&(button=="host"||button=="host_public"||button=="join")){result["backend"]="launcher_loopback";result["backend_confirmed"]=admitted()&&command_failures==failed;result["callback_completed"]=false;}
    result["handler_failed"]=command_failures!=failed;result["callback_pending"]=pending;
    if(button=="host"||button=="host_public"||button=="join")result["admission_generation"]=generation;
    if(button=="search"||button=="list_refresh"){result["discovery_generation"]=list_generation;result["callback_pending"]=list_pending;result["callback_completed"]=list_callback_completed;result["backend_confirmed"]=list_callback_completed&&list_callback_ok;}
    result["state"]=status();result["steam_lobby_id"]=net::SteamMatchmaking::instance().current_lobby_id();
    route_receipts[token]=result;
    if(code||selection_code||!result.at("handler_reached").get<bool>()||result.at("handler_failed").get<bool>()||
       (!world_name.empty()&&!result.value("selection_confirmed",false))){
        error="native lobby activation failed button="+button+" exception="+std::to_string(code)+" selection_exception="+std::to_string(selection_code)+" cause="+cause;return false;}
    error.clear();return true;
}
}

namespace mt2mp::game::native_lobby {
namespace {
bool release_route(void* mode,std::string& error){
    if(!image||GetCurrentThreadId()!=thread||ui_mode!=3||!mode||mode!=ui_owner){error="release fixture actual active native Lobby Mode/thread differs";return false;}
    class Memory final:public initial_release::Memory {
        placement_ui_probe::ProcessMemory native_;
        bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return native_.copy(p,out,n,e);}
    } memory;
    return initial_release::owned_mode_route(memory,image,uintptr_t(&ingame_command),uintptr_t(original_ingame_command),error);
}
}
bool initial_release_window(void* mode,void** window,std::string& error){
    if(!window){error="release fixture window output absent";return false;}*window=nullptr;
    if(!release_route(mode,error))return false;
    const auto fault=find_window("release_hud",window);
    if(fault||!*window){error="release fixture original registered ReleaseHUD lookup failed SEH="+std::to_string(fault);return false;}
    error.clear();return true;
}
bool begin_initial_release_fixture(void* mode,std::string& error){
    if(release_fixture_active){error="release fixture original callback scope already active";return false;}
    if(!release_route(mode,error))return false;
    release_fixture_mode=mode;release_fixture_callbacks=0;release_fixture_error.clear();release_fixture_active=true;return true;
}
bool end_initial_release_fixture(uint64_t& callbacks,std::string& error){
    callbacks=release_fixture_callbacks;
    if(!release_fixture_active){error="release fixture original callback scope absent";return false;}
    void* mode=release_fixture_mode;release_fixture_active=false;release_fixture_mode=nullptr;
    if(!release_fixture_error.empty()){error=release_fixture_error;return false;}
    return release_route(mode,error);
}
}
