#include "monster_live_bridge.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace mt2mp::game::live_host_state {
#include "monster_frame_apply.inc"
}
namespace {
size_t checks=0;void need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
struct Port:live::Port {bool read(live::Pin&,std::string&)override{return false;}bool store(const live::Pin&,size_t,int32_t,int32_t,std::string&)override{return false;}};
struct Transport:mt2mp::action_session::Transport {bool claim(Handler,std::string&)override{return false;}bool send(uint64_t,const std::vector<uint8_t>&,std::string&)override{return false;}};
struct Monsters:live::MonsterPort {
    int calls=0;bool attempted=false,confirmed=true,unknown=false,okay=true,throws=false,bind_okay=true;
    bool bind(const live::Context&,const live::Binding&,uint64_t,std::string&)override{return bind_okay;}
    bool suspend(std::string&)override{return true;}
    bool capture(const live::Context&,const live::Binding&,const live::Frame&,live::monsters::State&,std::string&)override{return false;}
    bool preflight(const live::Context&,const live::Binding&,const live::Frame&,std::string&)override{return false;}
    bool apply(const live::Context&,const live::Binding&,const live::Frame&,live::monsters::Receipt& r,std::string& e)override{++calls;r.attempted=attempted;r.confirmed=confirmed;r.unknown=unknown;
        if(throws)throw std::runtime_error("exact native cause");if(!okay)e="exact native cause";return okay;}
    bool after_action(const live::Context&,const live::Binding&,std::string&)override{return true;}
    nlohmann::json status()const override{return nullptr;}
};
}
int main(){try{Port p;Transport t;std::string e;
    for(int mode=0;mode<8;++mode){live::Bridge bridge({},{},1,p,t,{});Monsters port;need(bridge.bind_monsters(port,e),"initial bind failed");need(!bridge.bind_monsters(port,e),"duplicate bind accepted");
        live::Frame frame;frame.monsters=live::monsters::State{};bridge.receipt_.confirmed=true;
        if(mode==1)port.okay=false;if(mode==2)port.confirmed=false;if(mode==3){port.okay=false;port.attempted=true;}if(mode==4)port.unknown=true;
        if(mode==5){port.throws=true;port.attempted=true;}if(mode==6){port.okay=false;bridge.receipt_.attempted=true;}if(mode==7)frame.monsters.reset();
        const bool result=bridge.apply_monsters(frame,e);need(result==(mode==0),"incomplete Monster component eligible for full ACK");need(port.calls==(mode==7?0:1),"native callback replay or missing call");
        if(mode)need(!bridge.receipt_.confirmed&&!e.empty(),"failure looked confirmed or lost original cause");
        need(bridge.locked_==(mode>=3&&mode<=6),"uncertain or earlier Frame mutation did not lock");
        if(mode==1||mode==3||mode==5||mode==6)need(e.find("exact native cause")!=std::string::npos,"upstream cause lost");
    }
    live::Bridge absent({},{},1,p,t,{});live::Frame frame;need(absent.apply_monsters(frame,e),"legacy absent component failed");frame.monsters=live::monsters::State{};need(!absent.apply_monsters(frame,e)&&!absent.locked_,"missing provider accepted or falsely attempted");
    live::Bridge late({},{},1,p,t,{});Monsters port;late.host_revision_=1;need(!late.bind_monsters(port,e),"late provider armed after publication");
    live::Bridge failed({},{},1,p,t,{});port.bind_okay=false;need(!failed.bind_monsters(port,e)&&!failed.monster_port_,"failed native bind installed provider");
    std::cout<<checks<<" actual Monster Bridge gates passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
