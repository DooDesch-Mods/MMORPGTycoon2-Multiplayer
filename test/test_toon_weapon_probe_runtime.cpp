#define main retained_runtime_fixture_main
#include "test_toon_weapon_runtime.cpp"
#undef main
#include "toon_weapon_visual/probe.h"
namespace probe_fixture {int calls=0;bool change=false,fail=false;}
namespace mt2mp::game::toon_weapon_visual {
bool inspect(uintptr_t toon,Observation& out,std::string& error){
    ++probe_fixture::calls;out={};out.plan.toon=toon;
    if(probe_fixture::change)++fixture::world.generation;
    if(probe_fixture::fail){error="actual attachment owner differs";return false;}
    return true;
}
}
namespace mt2mp::game::typed_action_runtime {
#include "toon_weapon_visual/probe_runtime.inc"
}
int main(){try{
    {World w;probe_fixture::calls=0;nlohmann::json out;std::string e;need(rt::read_subscriber_weapon(59,out,e)&&out.at("read_only")==true&&out.at("attempted")==false&&out.at("serialized_id")==59&&probe_fixture::calls==1&&!fixture::refresh_calls,"bound Client probe refreshed or lost ID");}
    {World w;w.runtime.controller.context.role=action::Role::Host;w.runtime.session.context=w.runtime.controller.context;fixture::scale=4;nlohmann::json out;std::string e;need(rt::read_subscriber_weapon(59,out,e)&&out.at("role")=="Host","bound running Host observation rejected");}
    {World w;w.runtime.controller.value.locked=true;fixture::fence.journal_failed("retained original failure");nlohmann::json out;std::string e;need(rt::read_subscriber_weapon(59,out,e)&&fixture::fence.cause()=="retained original failure","read-only diagnostic cleared or rejected retained failure");}
    {World w;probe_fixture::calls=0;put(w.subscriber.data(),0x80,uintptr_t(0));nlohmann::json out;std::string e;need(rt::read_subscriber_weapon(59,out,e)&&out.at("toon_absent")==true&&!probe_fixture::calls,"absent Toon invoked visual methods");}
    {World w;w.runtime.thread=0;nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(59,out,e)&&e.find("Mainthread")!=std::string::npos,"off-thread probe accepted");}
    {World w;w.runtime.controller.value.started=false;nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(59,out,e),"unjoined probe accepted");}
    {World w;nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(60,out,e)&&e.find("absent from current census")!=std::string::npos,"missing Subscriber accepted");}
    {World w;put(w.toon.data(),0x940,uintptr_t(0));nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(59,out,e)&&e.find("reciprocal")!=std::string::npos,"foreign reciprocal owner accepted");}
    {World w;w.runtime.session.context.snapshot_uuid="different";nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(59,out,e)&&e.find("Session")!=std::string::npos,"changed Context accepted");}
    {World w;probe_fixture::change=true;nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(59,out,e)&&e.find("lifetime")!=std::string::npos&&out.at("read_confirmed")==false,"post-read lifetime change accepted");probe_fixture::change=false;}
    {World w;probe_fixture::fail=true;nlohmann::json out;std::string e;need(!rt::read_subscriber_weapon(59,out,e)&&e=="actual attachment owner differs","original probe failure lost");probe_fixture::fail=false;}
    std::cout<<checks<<" actual read-only Runtime probe checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
