#include "tutorial_replication/state.h"
#include "live_host_state/state.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
namespace {void require(bool result,const char* cause){if(!result)throw std::runtime_error(cause);}}
int main(){try{
    using namespace tutorial_replication;
    State source{{Cursor{"initial_tutorial","zone_2",107,41,false,{"welcome","wasd","zoom","zone"}}},false,false};
    State decoded;std::string e;require(decode(encode(source),decoded,e)&&decoded==source,"tutorial roundtrip lost history");
    auto duplicate=source;duplicate.active.push_back(source.active.front());require(!valid(duplicate,e),"duplicate scenario accepted");
    auto conflicting=source;conflicting.initial_complete=true;require(!valid(conflicting,e),"active/completed conflict accepted");
    auto hostile=encode(source);hostile["active"][0]["next"]=UINT64_MAX;require(!decode(hostile,decoded,e),"uint64 cursor overflow accepted");
    hostile=encode(source);hostile["active"][0]["history"]=std::vector<std::string>(513,"welcome");require(!decode(hostile,decoded,e),"history bound ignored");
    hostile=encode(source);hostile["active"][0]["complete"]=1;require(!decode(hostile,decoded,e),"integer accepted as completion bool");
    hostile=encode(source);hostile["active"][0]["pointer"]=123;require(!decode(hostile,decoded,e),"extra native pointer field accepted");
    require(command_policy("MessageTo Game TutorialComplete",false)==Start::Skip,"Client profile effect allowed");
    require(command_policy("MessageTo SoftwareStatus TutorialLevelUp",true)==Start::Skip,"Client restored snapshot version overwritten");
    require(command_policy("MessageTo SoftwareStatus TutorialLevelUp",false)==Start::Reject,"unreplicated version advanced");
    require(command_policy("MessageTo Hud LockMode \"Tutorial_01\"",false)==Start::Native,"native tutorial UI rejected");
    require(command_policy("MessageTo Game NewGame",true)==Start::Reject,"arbitrary command allowed in restore");
    live_host_state::Frame frame;frame.session=1;frame.epoch=2;frame.source_generation=3;frame.revision=4;frame.snapshot="11111111-1111-4111-8111-111111111111";frame.values.calendar={1,1,1,0,0,7};frame.values.record={0,0,0,0,0};frame.tutorial=source;
    std::vector<uint8_t> bytes;require(live_host_state::encode(frame,bytes,e),"Host frame encode failed");live_host_state::Frame received;require(live_host_state::decode(bytes.data(),bytes.size(),received,e)&&received==frame,"existing frame stream lost tutorial payload");
    mt2mp::action_session::Context context{1,2,1,3,mt2mp::action_session::Role::Client,frame.snapshot};
    require(live_host_state::admit(1,context,2,3,nullptr,received,0,0,false,e)==live_host_state::Admission::New,"bound Host state rejected");
    auto changed=received;changed.tutorial->active.front().next=42;
    require(live_host_state::admit(1,context,2,3,&received,changed,0,0,false,e)==live_host_state::Admission::Rejected,"same revision conflicting duplicate step ID cursor accepted");
    require(live_host_state::admit(2,context,2,3,nullptr,received,0,0,false,e)==live_host_state::Admission::Rejected,"Client forged tutorial authority accepted");
    std::cout<<"17 tutorial codec/authority/effect checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
