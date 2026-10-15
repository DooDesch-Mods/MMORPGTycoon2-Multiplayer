#include "state.h"
#include <array>
#include <set>
#include <cmath>
namespace mt2mp::game::tutorial_replication {
namespace {
bool text(const std::string& value){if(value.size()>200)return false;for(unsigned char c:value)if(c<32||c==127)return false;try{static_cast<void>(nlohmann::json(value).dump());return true;}catch(const nlohmann::json::exception&){return false;}}
void fields(const nlohmann::json& j,std::initializer_list<const char*> names){if(!j.is_object()||j.size()!=names.size())throw std::runtime_error("tutorial exact fields required");for(auto n:names)if(!j.contains(n))throw std::runtime_error("tutorial field absent");}
int32_t integer(const nlohmann::json& j){if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>2048:j.get<int64_t>()<0||j.get<int64_t>()>2048))throw std::runtime_error("tutorial cursor integer outside0..2048");return j.get<int32_t>();}
}
bool valid(const MessageProgress& p,std::string& e){
    if(!std::isfinite(p.value)||!std::isfinite(p.minimum)||!std::isfinite(p.maximum)||p.minimum>p.maximum){e="tutorial message progress requires finite native value/min/max";return false;}
    e.clear();return true;
}
bool valid(const State& s,std::string& e){
    if(s.startingpoint&&!startingpoint::valid(*s.startingpoint,e))return false;
    if(s.active.size()>2){e="tutorial active state exceeds2";return false;}
    std::set<std::string> ids;
    for(const auto& c:s.active){
        if((c.scenario!="initial_tutorial"&&c.scenario!="leveltwo_tutorial")||!ids.insert(c.scenario).second||c.count<1||c.count>2048||c.next<0||c.next>c.count||(c.complete&&c.next!=c.count)||c.history.size()>512||!text(c.last)){
            e="tutorial scenario/cursor/history bounds invalid";return false;
        }
        if((c.scenario=="initial_tutorial"&&s.initial_complete)||(c.scenario=="leveltwo_tutorial"&&s.leveltwo_complete)){e="tutorial active and completed states conflict";return false;}
        if(c.message_progress){if(!c.next){e="tutorial message progress requires an active indexed step";return false;}if(!valid(*c.message_progress,e))return false;}
        for(const auto& id:c.history)if(id.empty()||!text(id)){e="tutorial history ID invalid";return false;}
    }
    e.clear();return true;
}
nlohmann::json encode(const State& s){nlohmann::json rows=nlohmann::json::array();for(const auto& c:s.active)rows.push_back({{"scenario",c.scenario},{"count",c.count},{"next",c.next},{"complete",c.complete},{"history",c.history},{"last",c.last},{"message_progress",c.message_progress?nlohmann::json{{"shown",c.message_progress->shown},{"value",c.message_progress->value},{"minimum",c.message_progress->minimum},{"maximum",c.message_progress->maximum}}:nlohmann::json(nullptr)}});return {{"active",rows},{"initial_complete",s.initial_complete},{"leveltwo_complete",s.leveltwo_complete},{"startingpoint",s.startingpoint?startingpoint::encode(*s.startingpoint):nlohmann::json(nullptr)}};}
bool decode(const nlohmann::json& j,State& out,std::string& e){out={};try{
    if(j.contains("startingpoint"))fields(j,{"active","initial_complete","leveltwo_complete","startingpoint"});else fields(j,{"active","initial_complete","leveltwo_complete"});if(!j.at("active").is_array()||j.at("active").size()>2||!j.at("initial_complete").is_boolean()||!j.at("leveltwo_complete").is_boolean())throw std::runtime_error("tutorial state types differ");State s;
    if(j.contains("startingpoint")&&!j.at("startingpoint").is_null()){startingpoint::Row r;if(!startingpoint::decode(j.at("startingpoint"),r,e))return false;s.startingpoint=r;}
    s.initial_complete=j.at("initial_complete").get<bool>();s.leveltwo_complete=j.at("leveltwo_complete").get<bool>();
    for(const auto& row:j.at("active")){if(row.contains("message_progress"))fields(row,{"scenario","count","next","complete","history","last","message_progress"});else fields(row,{"scenario","count","next","complete","history","last"});if(!row.at("complete").is_boolean()||!row.at("history").is_array()||row.at("history").size()>512)throw std::runtime_error("tutorial cursor types/history bound differ");Cursor c;c.scenario=row.at("scenario").get<std::string>();c.count=integer(row.at("count"));c.next=integer(row.at("next"));c.complete=row.at("complete").get<bool>();c.history=row.at("history").get<std::vector<std::string>>();c.last=row.at("last").get<std::string>();if(row.contains("message_progress")&&!row.at("message_progress").is_null()){
        const auto& progress=row.at("message_progress");fields(progress,{"shown","value","minimum","maximum"});
        if(!progress.at("shown").is_boolean()||!progress.at("value").is_number_float()||!progress.at("minimum").is_number_float()||!progress.at("maximum").is_number_float())throw std::runtime_error("tutorial message progress exact boolean/float types required");
        MessageProgress values{progress.at("shown").get<bool>(),progress.at("value").get<float>(),progress.at("minimum").get<float>(),progress.at("maximum").get<float>()};
        if(!valid(values,e))return false;c.message_progress=values;
    }s.active.push_back(std::move(c));}
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
