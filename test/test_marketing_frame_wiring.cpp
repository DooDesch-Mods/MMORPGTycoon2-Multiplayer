#include "marketing_bridge.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace m=mt2mp::game::marketing_authority;
namespace mt2mp::game::live_host_state {
#include "marketing_apply.inc"
#include "tutorial_publication.inc"
}
namespace {
size_t checks=0;void need(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
struct Port:live::Port {bool read(live::Pin&,std::string&)override{return false;}bool store(const live::Pin&,size_t,int32_t,int32_t,std::string&)override{return false;}};
struct Transport:mt2mp::action_session::Transport {bool claim(Handler,std::string&)override{return false;}bool send(uint64_t,const std::vector<uint8_t>&,std::string&)override{return false;}};
struct Marketing:live::MarketingPort {bool okay=true,confirmed=true,unknown=false,attempted=false,throws=false;int calls=0;
    bool capture(const live::Context&,const live::Binding&,const live::Frame&,m::State&,std::string&)override{return false;}
    bool apply(const live::Context&,const live::Binding&,const live::Frame&,live::MarketingReceipt& r,std::string& e)override{++calls;r={attempted,confirmed,unknown,{}};if(throws)throw std::runtime_error("original failure");e=okay?"":"original failure";return okay;}
    m::Json status()const override{return nullptr;}
};
}
int main(){try{Port p;Transport t;std::string e;
    namespace tutorial=mt2mp::game::tutorial_replication;
    std::optional<tutorial::State> older=tutorial::State{};
    older->research=tutorial::research::Row{};older->research->key="bridges";
    older->research->value.release_counter=7413;
    auto fresh=older;fresh->research->value.release_counter=8279;
    need(live::same_tutorial_publication(older,fresh),"scan stamp alone forces ordinary publication");
    need(older!=fresh&&fresh->research->value.release_counter==8279,"exact State equality or payload was changed");
    live::Frame old_frame,new_frame;old_frame.tutorial=older;new_frame.tutorial=fresh;
    need(old_frame!=new_frame,"wire Frame identity ignores scan stamp");
    need(live::same_tutorial_publication({},{}),"absent tutorial differs");
    need(!live::same_tutorial_publication(older,{}),"tutorial presence change suppressed");
    for(int field=0;field<16;++field){auto changed=fresh;
        if(field==0)changed->research.reset();
        if(field==1)changed->research->key="other";
        if(field==2)++changed->research->value.release_change_count;
        if(field==3)++changed->research->value.release_feature_count;
        if(field==4)++changed->research->value.inventory_count;
        if(field==5)changed->research->value.work=1;
        if(field==6)++changed->research->value.lines;
        if(field==7)++changed->research->value.bugs_total;
        if(field==8)++changed->research->value.desired_workers;
        if(field==9)changed->research->value.deployed=true;
        if(field==10)changed->active.push_back({});
        if(field==11)changed->terminal.push_back({});
        if(field==12)changed->initial_complete=true;
        if(field==13)changed->version=std::array<int32_t,3>{1,2,3};
        if(field==14)++changed->lifecycle_order[0][0];
        if(field==15)++changed->leveltwo_initial_next;
        need(!live::same_tutorial_publication(older,changed),"semantic tutorial change suppressed");
    }
    older->startingpoint=tutorial::startingpoint::Row{};older->terminal_startingpoint=*older->startingpoint;
    fresh=older;++fresh->startingpoint->release_counter;
    need(!live::same_tutorial_publication(older,fresh),"live Startingpoint stamp suppressed");
    fresh=older;++fresh->terminal_startingpoint->release_counter;
    need(!live::same_tutorial_publication(older,fresh),"terminal Startingpoint receipt suppressed");

    for(int mode=0;mode<6;++mode){live::Bridge bridge({},{},1,p,t,{});Marketing port;need(bridge.bind_marketing(port,e),"bind");need(!bridge.bind_marketing(port,e),"double bind");live::Frame frame;frame.marketing=m::State{};
        if(mode==1)port.okay=false;if(mode==2)port.confirmed=false;if(mode==3){port.okay=false;port.attempted=true;}if(mode==4)port.unknown=true;if(mode==5)port.throws=true;
        bridge.receipt_.confirmed=true;const bool okay=bridge.apply_marketing(frame,e);need(okay==(mode==0),"incomplete component eligible for ACK");need(port.calls==1,"native apply replayed");
        if(mode)need(!bridge.receipt_.confirmed,"failure preserved scalar confirmed result");if(mode>=3)need(bridge.locked_&&bridge.receipt_.unknown,"uncertain mutation unlocked");
    }
    live::Bridge bridge({},{},1,p,t,{});live::Frame frame;need(bridge.apply_marketing(frame,e),"legacy absent component failed");frame.marketing=m::State{};need(!bridge.apply_marketing(frame,e),"missing provider accepted");
    std::cout<<"PASS marketing_frame_wiring "<<checks<<" checks\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<"\n";return 1;}}
