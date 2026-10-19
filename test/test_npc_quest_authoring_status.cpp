#include "npc_quest_authoring_fixture.h"
#include <iostream>
#include <map>
namespace a=mt2mp::action_session;using json=nlohmann::json;
namespace quest_bridge{
json record_json(const mt2mp::npc_quest_records::Record& value){
 std::string e;std::vector<uint8_t> wire;auto message=result();need(encode(message,wire,e),e);const auto encoded=json::parse(wire);
 if(value==before().record)return encoded["evidence"]["quest_before"]["record"];
 if(value==after(before()).record)return encoded["evidence"]["quest_after"]["record"];
 need(value==mt2mp::npc_quest_records::Record{},"unexpected actual status record");return json::object();
}}
struct Operation{a::Request request;bool attempted=false,returned=false,seed_attempted=false,confirmed=false,retired=false;std::string cause;a::Evidence evidence;};
std::map<std::string,Operation> operations;std::map<std::string,json> jobs;
bool ready=true,fault=false;bool pending_ui(){return false;}uint64_t captures=1,submitted=1,rejected=0;std::string cause,last_action=action_id;
json rating_status(){return {{"ready",true},{"host_completed",1},{"client_suppressed",2},{"failures",0},{"cause",""},{"last",nullptr}};}
#include "quest_status_body.inc"
int main(){try{unsigned n=0;need(status()["operations"].empty(),"empty status invented operation");++n;
 need(status()["rating_authority"]==rating_status(),"rating authority receipt missing from actual Quest status");++n;
 Operation op;op.request=request().request;op.attempted=op.returned=op.confirmed=true;op.evidence=result().evidence;operations[action_id]=op;
 std::string e;std::vector<uint8_t> wire;need(encode(result(),wire,e),e);const auto expected=json::parse(wire)["evidence"];
 auto actual=status()["operations"][action_id];
 for(const auto key:{"before","after","quest_before","quest_after","undo_confirmed"})need(actual["evidence"][key]==expected[key],std::string("native receipt differs from actual codec: ")+key);++n;
 need(actual["before"]==actual["evidence"]["quest_before"]["record"]&&actual["after"]==actual["evidence"]["quest_after"]["record"],"existing row readback changed");++n;
 need(actual["evidence"]["retirement_confirmed"]==false,"unretired operation copied stale evidence flag");operations[action_id].retired=true;need(status()["operations"][action_id]["evidence"]["retirement_confirmed"]==true,"current retirement absent");++n;
 operations[action_id].confirmed=false;operations[action_id].cause="retained original cause";actual=status()["operations"][action_id];need(actual["cause"]=="retained original cause"&&actual["confirmed"]==false,"status hid failed operation");++n;
 std::cout<<n<<" actual Quest status/full-codec receipt cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
