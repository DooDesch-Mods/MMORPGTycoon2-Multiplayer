#include <charconv>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"

using Json=nlohmann::json;
struct Config {bool local_test=false,is_host=false;static Config& instance(){static Config value;return value;}};
namespace net {
namespace chat {struct Runtime {
    bool admitted=false;unsigned admissions=0;
    static Runtime& instance(){static Runtime value;return value;}
    void set_lobby_admitted(bool value){admitted=value;++admissions;}
};}
struct LobbyInfo {std::string state="open",host_name="Host";int player_count=1,max_players=4;uint64_t lobby_id=42;};
struct SteamNetworking {static SteamNetworking& instance(){static SteamNetworking value;return value;}bool init(){return true;}std::string get_persona_name(uint64_t){return "Host";}};
struct SteamMatchmaking {
    std::function<void(bool,uint64_t)> completion;unsigned creates=0,joins=0;uint64_t requested=0;std::vector<uint64_t> members={1};
    static SteamMatchmaking& instance(){static SteamMatchmaking value;return value;}
    bool init(){return true;}
    uint64_t host_steam_id(){return 1;}
    bool publish_lobby_data(const char*,const std::string&,std::string&){return true;}
    bool set_lobby_joinable(bool,std::string&){return true;}
    void create_lobby(int,std::function<void(bool,uint64_t)> callback,bool){++creates;completion=callback;}
    void join_lobby(uint64_t id,std::function<void(bool,uint64_t)> callback){++joins;requested=id;completion=callback;}
    std::vector<uint64_t> get_lobby_members(){return members;}
};
namespace session_wire {bool physical=false;bool connected(){return physical;}}
}
bool pending=false,member=false,list_pending=false,choosing=false,load_attempted=false;
bool host_world_pick_requested=false,current_host_world=false;
std::string selected_world;
int ui_mode=2;size_t list_page=0;uint64_t generation=0,selected_lobby=0,owner=0;
std::vector<net::LobbyInfo> discovered;
std::vector<std::string> saved_worlds;
std::string chat_error;uint64_t chat_last_sequence=0;bool chat_draft_clear_failed=false;
std::string cause,ui_status,field_text,published_state;bool label_ok=true;
std::map<std::string,Json> route_receipts;
unsigned failures=0,world_selections=0;
unsigned char field_storage[0x300]{};void* panel=nullptr;void* browser_panel=nullptr;
bool admitted(){return member;}
bool fail(const std::string& detail){cause=detail;++failures;return false;}
bool metadata_failure(const std::string& detail){return fail(detail);}
bool label(const char*,const std::string& value){field_text=value;return label_ok;}
int show(void*,bool){return 0;}
bool select_world(void*,const std::string&){++world_selections;return true;}
bool admit_local(bool){throw std::runtime_error("unexpected local admission");}
void* pane(const char*){return field_storage;}
bool text(const void*,std::string& out,size_t){out=field_text;return true;}
#define LOG_WARN(...) ((void)0)
#define MOD_VERSION "test"
void dispatch(std::string cmd){
    void* mode=nullptr;auto& mm=net::SteamMatchmaking::instance();
#include "lobby_row_body.inc"
#include "lobby_admission_body.inc"
}
std::string publication_state(const Json& preparation){
    auto& mm=net::SteamMatchmaking::instance();
#include "lobby_publication_state.inc"
    return state;
}
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void reset(){
    net::chat::Runtime::instance()={};chat_error.clear();chat_last_sequence=0;
    Config::instance()=Config{};net::SteamMatchmaking::instance()=net::SteamMatchmaking{};
    pending=member=list_pending=choosing=load_attempted=false;net::session_wire::physical=false;
    host_world_pick_requested=current_host_world=false;selected_world.clear();
    ui_mode=2;list_page=0;generation=selected_lobby=owner=0;failures=world_selections=0;
    discovered={net::LobbyInfo{}};saved_worlds={"Saved"};cause.clear();ui_status.clear();field_text.clear();label_ok=true;route_receipts.clear();
}
int main(){
    auto& mm=net::SteamMatchmaking::instance();
    reset();dispatch("MT2MPHost");
    check(mm.creates==1&&pending&&!host_world_pick_requested,"unconfirmed Host opens world picker");
    member=true;mm.completion(true,42);check(host_world_pick_requested,"confirmed new Title host does not queue explicit picker");
    reset();selected_world="Already Selected";dispatch("MT2MPHost");member=true;mm.completion(true,42);
    check(!host_world_pick_requested,"Host callback replaces selected world");
    reset();current_host_world=true;dispatch("MT2MPHost");member=true;mm.completion(true,42);
    check(!host_world_pick_requested,"Host callback replaces current world");
    reset();dispatch("MT2MPHost");mm.completion(false,0);check(!host_world_pick_requested,"failed Host opens world picker");
    reset();dispatch("MT2MPList0");
    check(mm.joins==1&&mm.requested==42&&selected_lobby==42&&pending&&!admitted(),"row did not request one callback-confirmed Join");
    route_receipts["row"]={{"button","list0"},{"admission_generation",generation},{"backend_confirmed",false}};
    dispatch("MT2MPList0");check(mm.joins==1&&failures==1,"pending row replayed Join");
    check(!net::chat::Runtime::instance().admitted,"pending callback must not admit chat");
    member=true;mm.completion(true,42);
    check(net::chat::Runtime::instance().admitted&&net::chat::Runtime::instance().admissions==1,"confirmed callback admits chat once");
    check(!pending&&owner==1&&route_receipts["row"]["callback_completed"]==true&&route_receipts["row"]["backend_confirmed"]==true,"row callback did not update actual admission receipt");
    reset();dispatch("MT2MPList0");route_receipts["row"]={{"button","list0"},{"admission_generation",generation}};mm.completion(false,0);
    check(!pending&&!admitted()&&cause=="Steam lobby admission failed"&&route_receipts["row"]["backend_confirmed"]==false,"failed Join was presented as admitted");
    check(!net::chat::Runtime::instance().admitted,"failed callback must not admit chat");
    reset();dispatch("MT2MPList0");++generation;mm.completion(true,42);check(owner==0&&pending,"stale callback admitted");
    reset();discovered[0].state="playing";dispatch("MT2MPList0");check(mm.joins==0&&failures==1,"closed row joined");
    reset();discovered[0].player_count=4;dispatch("MT2MPList0");check(mm.joins==0&&failures==1,"full row joined");
    reset();member=true;dispatch("MT2MPList0");check(mm.joins==0&&failures==1,"existing membership joined again");
    reset();label_ok=false;dispatch("MT2MPList0");check(mm.joins==0&&failures==1,"failed row ID write attempted Join");
    reset();choosing=true;dispatch("MT2MPList0");check(world_selections==1&&mm.joins==0,"saved-world row entered Steam Join");
    reset();field_text="99";dispatch("MT2MPJoin");check(mm.joins==1&&mm.requested==99,"manual ID Join lost");
    reset();load_attempted=true;check(publication_state(Json::object())=="open","waiting first peer closed public lobby");
    mm.members={1,2};check(publication_state(Json::object())=="preparing","confirmed peers did not close preparation membership");
    check(publication_state({{"playing",true}})=="playing"&&publication_state({{"leaving",true}})=="leaving","session publication precedence changed");
    std::cout<<"PASS actual row-to-Join and admission callback bodies, one attempt, invalid rows, manual Join, first-peer visibility\n";
}
