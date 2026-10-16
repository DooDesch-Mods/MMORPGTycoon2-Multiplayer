#include "toon_time_fence/normalization.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace timef=mt2mp::game::toon_time_fence;
struct Calls {
    struct Owner {int subscriber_id=60;int epoch=1;bool operator==(const Owner&)const=default;};
    bool owned=true,cas_ok=true,original_ok=true,throw_original=false;
    int pins=0,fail_pin=0,change_pin=0,clears=0,originals=0;
    uint32_t bits=std::bit_cast<uint32_t>(2.0f),after_original=0;
    float passed=-1,effective=-1;
    bool pin(uintptr_t,Owner& out,bool& is_owned,std::string& error){
        ++pins;is_owned=owned;if(pins==fail_pin){error="exact owner failure";return false;}
        if(pins==change_pin)out.epoch=2;return true;
    }
    bool read_backlog(uintptr_t,uint32_t& out,std::string&){out=bits;return true;}
    bool clear_backlog(uintptr_t,uint32_t expected,std::string& error){
        ++clears;if(!cas_ok||bits!=expected){error="exact CAS failure";return false;}bits=0;return true;
    }
    bool original(uintptr_t,float dt,std::string& error){
        ++originals;passed=dt;effective=dt+std::bit_cast<float>(bits);
        if(throw_original)throw std::runtime_error("original exception");
        bits=after_original;if(!original_ok)error="original SEH";return original_ok;
    }
};
int main(){unsigned checks=0;const auto need=[&](bool yes,const char* cause){++checks;if(!yes)throw std::runtime_error(cause);};
try{
    {Calls c;timef::Receipt r;need(timef::normalize(c,1,3,r)&&r.confirmed&&r.cache_cleared&&r.original_returned&&c.originals==1&&c.passed==0&&c.effective==0,"positive backlog not normalized before original once");}
    {Calls c;c.owned=false;timef::Receipt r;need(timef::normalize(c,1,3,r)&&r.forward&&!c.clears&&!c.originals&&c.bits==std::bit_cast<uint32_t>(2.0f),"unowned path changed original input");}
    {Calls c;c.bits=0;timef::Receipt r;need(timef::normalize(c,1,0,r)&&!c.clears&&c.originals==1,"zero cache changed original-once contract");}
    {Calls c;c.bits=0x80000000;timef::Receipt r;need(timef::normalize(c,1,0,r)&&c.clears==1&&c.bits==0,"negative zero not canonicalized");}
    for(float dt:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){Calls c;timef::Receipt r;need(!timef::normalize(c,1,dt,r)&&!r.unknown&&!r.forward&&!c.originals&&!c.clears,"invalid input reached original");}
    for(float cache:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){Calls c;c.bits=std::bit_cast<uint32_t>(cache);timef::Receipt r;need(!timef::normalize(c,1,0,r)&&!r.unknown&&!c.originals&&!c.clears,"invalid cache reached original");}
    for(int pin:{1,2,3}){Calls c;c.fail_pin=pin;timef::Receipt r;need(!timef::normalize(c,1,0,r)&&r.cause=="exact owner failure"&&!r.forward&&r.unknown==(pin==3)&&c.originals==(pin==3?1:0),"owner failure lost phase or cause");}
    for(int pin:{2,3}){Calls c;c.change_pin=pin;timef::Receipt r;need(!timef::normalize(c,1,0,r)&&!r.forward&&r.unknown==(pin==3),"changed owner accepted");}
    {Calls c;c.cas_ok=false;timef::Receipt r;need(!timef::normalize(c,1,0,r)&&r.unknown&&r.cache_attempted&&!r.cache_cleared&&!c.originals&&r.cause=="exact CAS failure","CAS failure replayed original");}
    {Calls c;c.original_ok=false;timef::Receipt r;need(!timef::normalize(c,1,0,r)&&r.unknown&&c.originals==1&&!r.original_returned&&r.cause=="original SEH","original failure lost Unknown");}
    {Calls c;c.throw_original=true;timef::Receipt r;need(!timef::normalize(c,1,0,r)&&r.unknown&&c.originals==1&&r.cause.find("original exception")!=std::string::npos,"original C++ exception escaped");}
    {Calls c;c.after_original=std::bit_cast<uint32_t>(1.0f);timef::Receipt r;need(!timef::normalize(c,1,0,r)&&r.unknown&&r.original_returned&&!r.confirmed&&c.originals==1,"original cache write hidden");}
    {Calls c;timef::Receipt r;need(timef::normalize(c,1,0,r)&&timef::normalize(c,1,0,r)&&c.originals==2&&c.clears==1,"healthy subsequent call not original once");}
    std::cout<<checks<<" actual normalization checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
