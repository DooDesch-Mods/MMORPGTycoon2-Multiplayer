#include "startup_calendar_normalization.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

namespace sc=mt2mp::game::startup_calendar;
namespace {
void require(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
sc::State initial() {
    sc::State s;s.image=0x100000;s.clock=0x200000;s.business=0x300000;s.record=0x400000;
    s.history=0x500000;s.member=s.record;s.software=0x600000;s.world={1,2,3,4,5};
    s.generation=9;s.epoch=3;s.thread=123;s.uuid="snapshot";s.sha="sha";s.history_count=1;s.history_capacity=4;
    s.clock_bytes.fill(0x35);s.business_bytes.fill(0x42);s.record_bytes.fill(0x57);
    sc::assign(s.clock_bytes,0x38,int32_t{1});sc::assign(s.clock_bytes,0x3c,int32_t{1});
    sc::assign(s.clock_bytes,0x40,int32_t{1});sc::assign(s.clock_bytes,0x44,int32_t{0});
    sc::assign(s.clock_bytes,0x48,int32_t{449});sc::assign(s.clock_bytes,0x4c,int32_t{7});
    sc::assign(s.clock_bytes,0x58,0.0f);sc::assign(s.clock_bytes,0x5c,1.0f);
    sc::assign(s.business_bytes,0x38,int32_t{94000});sc::assign(s.business_bytes,0x3c,int32_t{0});
    sc::assign(s.business_bytes,0x100,s.record);
    sc::assign(s.record_bytes,0x18,int32_t{2});sc::assign(s.record_bytes,0x1c,int32_t{0});
    sc::assign(s.record_bytes,0x20,int32_t{94000});sc::assign(s.record_bytes,0x28,int32_t{150000});
    sc::assign(s.record_bytes,0x2c,int32_t{6000});return s;
}
struct Port {
    sc::State state=initial();int reads=0,clock_calls=0,business_calls=0,fail_read=0;
    bool code=true,clock_ok=true,business_ok=true;
    std::function<void(sc::State&,int)> on_read;
    std::function<void(sc::State&)> after_clock,after_business;
    bool read(sc::State& out,std::string& e){++reads;if(on_read)on_read(state,reads);
        if(reads==fail_read){e="native read failed";return false;}out=state;return true;}
    bool verify_code(std::string& e){if(!code)e="original code differs";return code;}
    bool resolve_clock(const sc::State& before,std::string& e){
        require(before==state,"Clock received stale state");++clock_calls;
        sc::assign(state.clock_bytes,0x44,int32_t{1});sc::assign(state.clock_bytes,0x40,int32_t{2});
        if(after_clock)after_clock(state);if(!clock_ok)e="Clock original SEH";return clock_ok;
    }
    bool resolve_business(const sc::State& before,std::string& e){
        require(before==state,"Business received stale state");++business_calls;
        sc::assign(state.record_bytes,0x1c,int32_t{1});
        if(after_business)after_business(state);if(!business_ok)e="Business original SEH";return business_ok;
    }
};
void success() {
    Port p;const auto before=p.state;std::string e;
    require(sc::normalize(p,e)==sc::Result::Normalized&&e.empty(),"valid loaded day0 rejected");
    auto expected=before;sc::assign(expected.clock_bytes,0x40,int32_t{2});
    sc::assign(expected.clock_bytes,0x44,int32_t{1});sc::assign(expected.record_bytes,0x1c,int32_t{1});
    require(p.state==expected,"normalization altered another byte or owner");
    require(p.clock_calls==1&&p.business_calls==1&&p.reads==4,"native operation/read count differs");
    require(sc::normalize(p,e)==sc::Result::Unsupported,"normalized state replayed");
    require(p.clock_calls==1&&p.business_calls==1,"repeat called an original again");
}
void unsupported() {
    const std::vector<std::function<void(sc::State&)>> cases={
        [](auto& s){s.generation=0;},[](auto& s){s.epoch=0;},[](auto& s){s.uuid.clear();},[](auto& s){s.sha.clear();},
        [](auto& s){s.history_count=0;},[](auto& s){s.history_count=2;},[](auto& s){s.history_capacity=0;},
        [](auto& s){s.history=0;},[](auto& s){s.record=0;},[](auto& s){++s.member;},
        [](auto& s){sc::assign(s.record_bytes,0x18,int32_t{1});},
        [](auto& s){sc::assign(s.record_bytes,0x1c,int32_t{1});},
        [](auto& s){sc::assign(s.business_bytes,0x100,uintptr_t{});},
        [](auto& s){sc::assign(s.business_bytes,0x38,int32_t{100000});},
        [](auto& s){sc::assign(s.business_bytes,0x3c,int32_t{50000});},
        [](auto& s){s.software_state=1;},
        [](auto& s){sc::assign(s.clock_bytes,0x38,int32_t{0});},
        [](auto& s){sc::assign(s.clock_bytes,0x3c,int32_t{0});},
        [](auto& s){sc::assign(s.clock_bytes,0x3c,std::numeric_limits<int32_t>::max());},
        [](auto& s){sc::assign(s.clock_bytes,0x40,int32_t{0});},
        [](auto& s){sc::assign(s.clock_bytes,0x44,int32_t{1});},
        [](auto& s){sc::assign(s.clock_bytes,0x4c,int32_t{0});},
        [](auto& s){sc::assign(s.clock_bytes,0x58,1.0f);},
        [](auto& s){sc::assign(s.clock_bytes,0x5c,-1.0f);},
        [](auto& s){sc::assign(s.clock_bytes,0x5c,std::numeric_limits<float>::quiet_NaN());}
    };
    for(const auto& change:cases){Port p;change(p.state);const auto before=p.state;std::string e;
        require(sc::normalize(p,e)==sc::Result::Unsupported&&!e.empty(),"unsupported shape did not retain reload fallback");
        require(!p.clock_calls&&!p.business_calls&&p.state==before,"unsupported shape mutated state");}
}
void failures() {
    std::string e;
    {Port p;p.code=false;require(sc::normalize(p,e)==sc::Result::Failed&&e=="original code differs"&&!p.clock_calls,"code failure lost");}
    for(int read=1;read<=4;++read){Port p;p.fail_read=read;
        require(sc::normalize(p,e)==sc::Result::Failed&&e=="native read failed","read failure lost");
        require(p.clock_calls==(read>=3?1:0)&&p.business_calls==(read==4?1:0),"read failure replayed or skipped original");}
    {Port p;p.clock_ok=false;require(sc::normalize(p,e)==sc::Result::Failed&&e=="Clock original SEH"&&p.clock_calls==1&&!p.business_calls,"Clock failure replayed");}
    {Port p;p.business_ok=false;require(sc::normalize(p,e)==sc::Result::Failed&&e=="Business original SEH"&&p.clock_calls==1&&p.business_calls==1,"Business failure replayed");}
    {Port p;p.on_read=[](auto& s,int n){if(n==2)++s.generation;};
        require(sc::normalize(p,e)==sc::Result::Failed&&!p.clock_calls,"pre-call origin drift accepted");}
    const std::vector<std::function<void(sc::State&)>> changes={
        [](auto& s){++s.clock_bytes[0x48];},[](auto& s){++s.business_bytes[0x38];},
        [](auto& s){++s.record_bytes[0x2c];},[](auto& s){++s.record_bytes[0x5cf];},
        [](auto& s){++s.history_capacity;},[](auto& s){++s.member;},[](auto& s){++s.day_calls[0];},
        [](auto& s){++s.world[3];},[](auto& s){++s.generation;},[](auto& s){s.uuid="foreign";}
    };
    for(const auto& change:changes){
        Port p;p.after_clock=change;require(sc::normalize(p,e)==sc::Result::Failed&&p.clock_calls==1&&!p.business_calls,"Clock side effect escaped readback");
        Port q;q.after_business=change;require(sc::normalize(q,e)==sc::Result::Failed&&q.clock_calls==1&&q.business_calls==1,"Business side effect escaped readback");
    }
}
}
int main(){success();unsupported();failures();std::cout<<"startup calendar: guarded native sequence, fallback and complete readback checks passed\n";}
