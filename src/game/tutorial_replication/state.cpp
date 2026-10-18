#include "state.h"
#include <array>
#include <set>
#include <cmath>
#include <algorithm>
namespace mt2mp::game::tutorial_replication {
namespace {
bool text(const std::string& value){if(value.size()>200)return false;for(unsigned char c:value)if(c<32||c==127)return false;try{static_cast<void>(nlohmann::json(value).dump());return true;}catch(const nlohmann::json::exception&){return false;}}
void fields(const nlohmann::json& j,std::initializer_list<const char*> names){if(!j.is_object()||j.size()!=names.size())throw std::runtime_error("tutorial exact fields required");for(auto n:names)if(!j.contains(n))throw std::runtime_error("tutorial field absent");}
int32_t integer(const nlohmann::json& j){if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>2048:j.get<int64_t>()<0||j.get<int64_t>()>2048))throw std::runtime_error("tutorial cursor integer outside0..2048");return j.get<int32_t>();}
bool cursor_valid(const Cursor& c,std::string& e){
    if((c.scenario!="initial_tutorial"&&c.scenario!="leveltwo_tutorial")||c.count<1||c.count>2048||c.next<0||c.next>c.count||(c.complete&&c.next!=c.count)||c.history.size()>512||!text(c.last)){
        e="tutorial scenario/cursor/history bounds invalid";return false;}
    if(c.message_progress){if(!c.next){e="tutorial message progress requires an active indexed step";return false;}if(!valid(*c.message_progress,e))return false;}
    for(const auto& id:c.history)if(id.empty()||!text(id)){e="tutorial history ID invalid";return false;}
    return true;
}
nlohmann::json cursor_encode(const Cursor& c){return {{"scenario",c.scenario},{"count",c.count},{"next",c.next},{"complete",c.complete},{"history",c.history},{"last",c.last},{"message_progress",c.message_progress?nlohmann::json{{"shown",c.message_progress->shown},{"value",c.message_progress->value},{"minimum",c.message_progress->minimum},{"maximum",c.message_progress->maximum}}:nlohmann::json(nullptr)}};}
bool cursor_decode(const nlohmann::json& row,Cursor& c,std::string& e){
    if(row.contains("message_progress"))fields(row,{"scenario","count","next","complete","history","last","message_progress"});else fields(row,{"scenario","count","next","complete","history","last"});
    if(!row.at("complete").is_boolean()||!row.at("history").is_array()||row.at("history").size()>512)throw std::runtime_error("tutorial cursor types/history bound differ");
    c.scenario=row.at("scenario").get<std::string>();c.count=integer(row.at("count"));c.next=integer(row.at("next"));c.complete=row.at("complete").get<bool>();c.history=row.at("history").get<std::vector<std::string>>();c.last=row.at("last").get<std::string>();
    if(row.contains("message_progress")&&!row.at("message_progress").is_null()){
        const auto& p=row.at("message_progress");fields(p,{"shown","value","minimum","maximum"});
        if(!p.at("shown").is_boolean()||!p.at("value").is_number_float()||!p.at("minimum").is_number_float()||!p.at("maximum").is_number_float())throw std::runtime_error("tutorial message progress exact boolean/float types required");
        c.message_progress=MessageProgress{p.at("shown").get<bool>(),p.at("value").get<float>(),p.at("minimum").get<float>(),p.at("maximum").get<float>()};
    }return cursor_valid(c,e);
}
void version_decode(const nlohmann::json& j,std::optional<std::array<int32_t,3>>& out){
    if(j.is_null())return;
    if(!j.is_array()||j.size()!=3)throw std::runtime_error("tutorial version requires exact3 integers");
    std::array<int32_t,3> v{};for(size_t i=0;i<3;++i){const auto& n=j.at(i);if(!n.is_number_integer()||(n.is_number_unsigned()?n.get<uint64_t>()>INT32_MAX:n.get<int64_t>()<0||n.get<int64_t>()>INT32_MAX))throw std::runtime_error("tutorial version integer outside0..INT32_MAX");v[i]=n.get<int32_t>();}out=v;
}
}
bool valid(const MessageProgress& p,std::string& e){
    if(!std::isfinite(p.value)||!std::isfinite(p.minimum)||!std::isfinite(p.maximum)||p.minimum>p.maximum){e="tutorial message progress requires finite native value/min/max";return false;}
    e.clear();return true;
}
bool valid(const State& s,std::string& e){
    if(s.startingpoint&&!startingpoint::valid(*s.startingpoint,e))return false;
    if(s.research&&!research::valid(*s.research,e))return false;
    if(s.startingpoint&&s.research){
        const auto& a=*s.startingpoint;const auto& b=s.research->value;
        if(a.release_change_count!=b.release_change_count||a.release_counter!=b.release_counter){e="Research and Startingpoint shared Release counters differ";return false;}
        if(a.project_present&&b.project_present){e="Research and Startingpoint cannot both be current projects";return false;}
    }
    if(s.terminal_startingpoint&&!startingpoint::valid(*s.terminal_startingpoint,e))return false;
    if(s.staff_display){if(!(s.startingpoint&&s.startingpoint->project_present&&!s.startingpoint->deployed)&&!(s.research&&s.research->value.project_present&&!s.research->value.deployed)){e="Staff display requires current Startingpoint or Research row";return false;}if(!staff_display::valid(*s.staff_display,e))return false;}
    if(s.active.size()>2||s.terminal.size()>2){e="tutorial active/terminal state exceeds2";return false;}
    if(s.terminal.size()==2&&s.terminal.front().scenario!="initial_tutorial"){e="tutorial terminal receipts require fixed Initial/LevelTwo order";return false;}
    std::set<std::string> ids;bool initial_active=false,initial_terminal=false;
    for(const auto& c:s.active){
        if(!cursor_valid(c,e))return false;
        if(!ids.insert(c.scenario).second){e="tutorial duplicate active scenario";return false;}
        if((c.scenario=="initial_tutorial"&&s.initial_complete)||(c.scenario=="leveltwo_tutorial"&&s.leveltwo_complete)){e="tutorial active and completed states conflict";return false;}
        initial_active|=c.scenario=="initial_tutorial";
    }
    for(const auto& c:s.terminal){
        if(!cursor_valid(c,e))return false;
        const bool initial=c.scenario=="initial_tutorial";const size_t slot=initial?0:1;
        if(!ids.insert(c.scenario).second||!c.complete||c.count!=(initial?107:17)||c.next!=c.count||c.message_progress||!(initial?s.initial_complete:s.leveltwo_complete)||!s.lifecycle_order[slot][1]){e="tutorial terminal cursor/completion/order differs";return false;}
        initial_terminal|=initial;
    }
    std::set<uint32_t> serials;
    for(size_t i=0;i<2;++i){const auto& order=s.lifecycle_order[i];for(const auto n:order)if(n&&(n>4||!serials.insert(n).second)){e="tutorial lifecycle serial duplicate/outside1..4";return false;}
        const char* name=i==0?"initial_tutorial":"leveltwo_tutorial";
        if((order[0]&&!ids.contains(name))||(order[1]&&std::none_of(s.terminal.begin(),s.terminal.end(),[&](const Cursor& c){return c.scenario==name;}))||(order[0]&&order[1]&&order[0]>=order[1])){e="tutorial lifecycle serial has no matching birth/terminal";return false;}}
    if((s.version&&!initial_active)||(s.terminal_version&&!initial_terminal)||(s.terminal_startingpoint&&!initial_terminal)){e="tutorial version/Startingpoint receipt has no matching Initial cursor";return false;}
    if(initial_terminal&&(!s.terminal_version||!s.terminal_startingpoint||!s.terminal_startingpoint->deployed||s.terminal_startingpoint->project_present)){e="tutorial Initial terminal receipt requires native deployed Startingpoint and version";return false;}
    for(const auto* v:{&s.version,&s.terminal_version})if(*v)for(const auto n:**v)if(n<0){e="tutorial version contains negative native component";return false;}
    if(s.leveltwo_initial_next < -1 || s.leveltwo_initial_next > 107 || (s.leveltwo_initial_next >= 0 && !s.lifecycle_order[1][0])){e="tutorial LevelTwo birth checkpoint invalid";return false;}
    e.clear();return true;
}
nlohmann::json encode(const State& s){
    auto rows=nlohmann::json::array(),terminal=nlohmann::json::array();for(const auto& c:s.active)rows.push_back(cursor_encode(c));for(const auto& c:s.terminal)terminal.push_back(cursor_encode(c));
    nlohmann::json result={{"active",rows},{"initial_complete",s.initial_complete},{"leveltwo_complete",s.leveltwo_complete},{"startingpoint",s.startingpoint?startingpoint::encode(*s.startingpoint):nlohmann::json(nullptr)},{"staff_display",s.staff_display?staff_display::encode(*s.staff_display):nlohmann::json(nullptr)},
        {"terminal",terminal},{"terminal_startingpoint",s.terminal_startingpoint?startingpoint::encode(*s.terminal_startingpoint):nlohmann::json(nullptr)},
        {"version",s.version?nlohmann::json(*s.version):nlohmann::json(nullptr)},{"terminal_version",s.terminal_version?nlohmann::json(*s.terminal_version):nlohmann::json(nullptr)},{"lifecycle_order",s.lifecycle_order},{"leveltwo_initial_next",s.leveltwo_initial_next}};
    if(s.research)result["research"]=research::encode(*s.research);
    return result;
}
bool decode(const nlohmann::json& j,State& out,std::string& e){out={};try{
    if(j.contains("research"))fields(j,{"active","initial_complete","leveltwo_complete","startingpoint","research","staff_display","terminal","terminal_startingpoint","version","terminal_version","lifecycle_order","leveltwo_initial_next"});
    else if(j.contains("terminal"))fields(j,{"active","initial_complete","leveltwo_complete","startingpoint","staff_display","terminal","terminal_startingpoint","version","terminal_version","lifecycle_order","leveltwo_initial_next"});
    else if(j.contains("staff_display"))fields(j,{"active","initial_complete","leveltwo_complete","startingpoint","staff_display"});else if(j.contains("startingpoint"))fields(j,{"active","initial_complete","leveltwo_complete","startingpoint"});else fields(j,{"active","initial_complete","leveltwo_complete"});
    if(!j.at("active").is_array()||j.at("active").size()>2||!j.at("initial_complete").is_boolean()||!j.at("leveltwo_complete").is_boolean())throw std::runtime_error("tutorial state types differ");State s;
    if(j.contains("startingpoint")&&!j.at("startingpoint").is_null()){startingpoint::Row r;if(!startingpoint::decode(j.at("startingpoint"),r,e))return false;s.startingpoint=r;}
    if(j.contains("research")&&!j.at("research").is_null()){research::Row r;if(!research::decode(j.at("research"),r,e))return false;s.research=std::move(r);}
    if(j.contains("staff_display")&&!j.at("staff_display").is_null()){staff_display::State value;if(!staff_display::decode(j.at("staff_display"),value,e))return false;s.staff_display=std::move(value);}
    s.initial_complete=j.at("initial_complete").get<bool>();s.leveltwo_complete=j.at("leveltwo_complete").get<bool>();
    for(const auto& row:j.at("active")){Cursor c;if(!cursor_decode(row,c,e))return false;s.active.push_back(std::move(c));}
    if(j.contains("terminal")){
        if(!j.at("terminal").is_array()||j.at("terminal").size()>2)throw std::runtime_error("tutorial terminal array exceeds2");
        for(const auto& row:j.at("terminal")){Cursor c;if(!cursor_decode(row,c,e))return false;s.terminal.push_back(std::move(c));}
        if(!j.at("terminal_startingpoint").is_null()){startingpoint::Row r;if(!startingpoint::decode(j.at("terminal_startingpoint"),r,e))return false;s.terminal_startingpoint=r;}
        version_decode(j.at("version"),s.version);version_decode(j.at("terminal_version"),s.terminal_version);
        const auto& checkpoint=j.at("leveltwo_initial_next");if(!checkpoint.is_number_integer()||(checkpoint.is_number_unsigned()?checkpoint.get<uint64_t>()>107:checkpoint.get<int64_t>()< -1||checkpoint.get<int64_t>()>107))throw std::runtime_error("tutorial birth checkpoint outside-1..107");s.leveltwo_initial_next=checkpoint.get<int32_t>();
        const auto& order=j.at("lifecycle_order");if(!order.is_array()||order.size()!=2)throw std::runtime_error("tutorial lifecycle order requires2 rows");
        for(size_t i=0;i<2;++i){if(!order.at(i).is_array()||order.at(i).size()!=2)throw std::runtime_error("tutorial lifecycle order requires2 serials");for(size_t k=0;k<2;++k)s.lifecycle_order[i][k]=uint32_t(integer(order.at(i).at(k)));}
    }
    if(!valid(s,e))return false;out=std::move(s);return true;
}catch(const std::exception& x){e=std::string("tutorial decode: ")+x.what();return false;}}
Start command_policy(const std::string& command,bool restoring){
    if(command=="MessageTo Game TutorialComplete"||command=="MessageTo Game LevelTwoTutorialComplete")return Start::Skip;
    if(command=="MessageTo SoftwareStatus TutorialLevelUp")return restoring?Start::Skip:Start::Reject;
    constexpr std::array<const char*,11> allowed={
        "MessageTo SoftwareStatus LockLevelUp","MessageTo SoftwareStatus UnlockLevelUp",
        "MessageTo Hud LockMode Off","MessageTo Hud Tutorial 1","MessageTo Hud Tutorial 0",
        "MessageTo Hud LockMode \"Tutorial_01\"","MessageTo Hud LockMode \"Tutorial_02\"","MessageTo Hud LockMode \"Tutorial_03\"",
        "MessageTo Hud UnlockMode Overview","MessageTo WindowName \"toprighttabs_hud\" TutorialLock","MessageTo WindowName \"toprighttabs_hud\" TutorialUnlock"};
    for(auto value:allowed)if(command==value)return Start::Native;return Start::Reject;
}
}
