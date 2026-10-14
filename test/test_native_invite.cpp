#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool value,const char* detail){if(!value)throw std::runtime_error(detail);}
std::string cause,ui_status;
uint64_t command_failures=0;
std::vector<std::string> errors,information,lookups;
unsigned module_reads=0,friends_reads=0,utils_reads=0,overlay_reads=0,dialog_requests=0;
bool module_loaded=true,friends_present=true,utils_present=true,overlay_enabled=true,only_old_friends=false;
std::string missing_export;
int module_object=0,friends_object=0,utils_object=0;
constexpr uint64_t actual_lobby=109775244977043228ULL;

struct Matchmaking {
    bool member=true;uint64_t lobby=actual_lobby;
    bool in_lobby()const{return member;}
    uint64_t current_lobby_id()const{return lobby;}
} matchmaking;
void* fake_friends(){++friends_reads;return friends_present?&friends_object:nullptr;}
void* fake_utils(){++utils_reads;return utils_present?&utils_object:nullptr;}
bool fake_overlay(void* object){check(object==&utils_object,"wrong SteamUtils receiver");++overlay_reads;return overlay_enabled;}
void fake_invite(void* object,uint64_t lobby){check(object==&friends_object&&lobby==actual_lobby,"wrong invite receiver or lobby target");++dialog_requests;}
using Procedure=void(*)();
namespace mt2mp::net {
void* steam_module(){
    ++module_reads;return module_loaded?&module_object:nullptr;
}
}
Procedure GetProcAddress(void* module,const char* name){
    check(module==&module_object,"wrong selected Steam export owner");lookups.emplace_back(name);
    if(missing_export==name)return nullptr;
    if(std::string(name)=="SteamAPI_SteamFriends_v018")return only_old_friends?nullptr:reinterpret_cast<Procedure>(&fake_friends);
    if(std::string(name)=="SteamAPI_SteamUtils_v010")return reinterpret_cast<Procedure>(&fake_utils);
    if(std::string(name)=="SteamAPI_ISteamUtils_IsOverlayEnabled")return reinterpret_cast<Procedure>(&fake_overlay);
    if(std::string(name)=="SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog")return reinterpret_cast<Procedure>(&fake_invite);
    return nullptr;
}
#define LOG_ERROR(format,detail) errors.emplace_back(detail)
#define LOG_INFO(message) information.emplace_back(message)
#include "invite_failure_body.inc"
void dispatch_invite(const std::string& cmd){
    auto& mm=matchmaking;
#include "invite_command_body.inc"
}
void reset(){
    cause.clear();ui_status.clear();command_failures=0;errors.clear();information.clear();lookups.clear();
    module_reads=friends_reads=utils_reads=overlay_reads=dialog_requests=0;
    module_loaded=friends_present=utils_present=overlay_enabled=true;only_old_friends=false;missing_export.clear();matchmaking=Matchmaking{};
}
void failed(const char* reason,const char* status){
    check(cause==reason,"distinct failure cause missing");check(ui_status==status,"distinct German UI state missing");
    check(command_failures==1&&errors.size()==1&&errors[0]==reason,"failure not recorded/logged");
    check(dialog_requests==0&&information.empty(),"failure was presented as invite request");
}
int main(){
    reset();dispatch_invite("MT2MPInvite");
    check(dialog_requests==1&&friends_reads==1&&utils_reads==1&&overlay_reads==1,"v018-only success did not request dialog exactly once");
    check(std::find(lookups.begin(),lookups.end(),"SteamAPI_SteamFriends_v018")!=lookups.end()&&
          std::find(lookups.begin(),lookups.end(),"SteamAPI_SteamFriends_v017")==lookups.end(),"obsolete accessor requested");
    check(cause.empty()&&command_failures==0&&ui_status=="Steam-Einladungsdialog angefordert","success not limited to requested state");
    check(information.size()==1&&information[0].find("delivery not confirmed")!=std::string::npos,"delivery incorrectly claimed");

    reset();matchmaking.member=false;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog rejected: no active lobby","Keine Steam-Lobby");
    check(module_reads==0&&lookups.empty()&&friends_reads==0,"no-lobby request touched Steam boundaries");
    reset();matchmaking.lobby=0;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog rejected: active lobby ID is zero","Steam-Lobby-ID fehlt");
    check(module_reads==0,"zero lobby touched Steam module");
    reset();module_loaded=false;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog unavailable: selected Steam module unavailable","Steam-Bibliothek nicht verfügbar");
    check(lookups.empty()&&friends_reads==0,"missing module continued export lookup");

    const std::vector<std::pair<std::string,std::string>> absent={
        {"SteamAPI_SteamFriends_v018","Steam-Freundeschnittstelle fehlt"},
        {"SteamAPI_SteamUtils_v010","Steam-Dienstschnittstelle fehlt"},
        {"SteamAPI_ISteamUtils_IsOverlayEnabled","Steam-Overlayprüfung fehlt"},
        {"SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog","Steam-Einladungsfunktion fehlt"}};
    for(const auto& [symbol,status]:absent){
        reset();missing_export=symbol;dispatch_invite("MT2MPInvite");
        failed(("Steam invite dialog unavailable: missing export "+symbol).c_str(),status.c_str());
        check(friends_reads==0&&utils_reads==0&&overlay_reads==0,"missing export called another Steam interface");
    }
    reset();only_old_friends=true;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog unavailable: missing export SteamAPI_SteamFriends_v018","Steam-Freundeschnittstelle fehlt");
    check(friends_reads==0,"incompatible accessor fallback executed");
    reset();friends_present=false;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog unavailable: SteamFriends interface is null","Steam-Freundeschnittstelle nicht bereit");
    check(friends_reads==1&&utils_reads==0&&overlay_reads==0,"null friends continued");
    reset();utils_present=false;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog unavailable: SteamUtils interface is null","Steam-Dienstschnittstelle nicht bereit");
    check(utils_reads==1&&overlay_reads==0,"null utils continued overlay check");
    reset();overlay_enabled=false;dispatch_invite("MT2MPInvite");
    failed("Steam invite dialog unavailable: overlay disabled","Steam-Overlay deaktiviert");
    check(overlay_reads==1,"overlay availability was not measured");
    dispatch_invite("MT2MPInvite");
    check(errors.size()==1&&command_failures==2&&dialog_requests==0,"same reason logging repeated or automatically invited");
    overlay_enabled=true;dispatch_invite("MT2MPInvite");
    check(dialog_requests==1&&cause.empty()&&ui_status=="Steam-Einladungsdialog angefordert"&&command_failures==2,"manual recovery lost history or retained stale failure");
    reset();dispatch_invite("OtherCommand");
    check(module_reads==0&&dialog_requests==0&&cause.empty(),"foreign command entered Invite branch");
    std::cout<<"PASS actual Invite branch: v018, selected module/lobby, distinct visible failures, no false request or delivered claim, bounded logging and explicit recovery\n";
}
