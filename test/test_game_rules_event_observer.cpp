#include "game_rules_event_observer.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>
namespace observer=mt2mp::game::game_rules::event_observer;
namespace {
constexpr uintptr_t fixture_image=0x10000000;
constexpr uintptr_t fixture_table=fixture_image+0x1a2dd00+59*56;
std::map<uintptr_t,uint64_t> fixture_memory;
unsigned fixture_checks=0,fixture_head_reads=0;
bool fixture_change=false;
void need(bool value,const char* cause){++fixture_checks;if(!value)throw std::runtime_error(cause);}
void reset(){fixture_memory={{fixture_image+0x13b9820,1},{fixture_table+0x10,0},{fixture_table+0x18,0}};fixture_head_reads=0;fixture_change=false;}
void recipient(uintptr_t node,uintptr_t next,uintptr_t listener,uintptr_t vtable,uintptr_t callback){
    fixture_memory[node]=next;fixture_memory[node+8]=listener;
    if(listener){fixture_memory[listener]=vtable;if(vtable)fixture_memory[vtable]=callback;}
}
auto fixture_read=[](uintptr_t object,size_t offset,auto& value,std::string& error){
    auto address=object+offset;
    if(address==fixture_table+0x10&&++fixture_head_reads==2&&fixture_change)fixture_memory[0x3000]=0x5001;
    const auto found=fixture_memory.find(address);
    if(found==fixture_memory.end()){error="fixture original guarded read failed";return false;}
    value=static_cast<std::remove_reference_t<decltype(value)>>(found->second);return true;
};
bool observe(observer::State& state,std::string& error){error.clear();return observer::capture(fixture_read,fixture_image,state,error);}
}
int main(){try{
    observer::State actual;std::string error;
    reset();need(observe(actual,error),"empty list");need(actual.enabled==1&&actual.recipients.empty()&&actual.count==0,"empty exact state");
    fixture_memory[fixture_image+0x13b9820]=0;need(observe(actual,error)&&actual.enabled==0,"disabled event gate is observed");
    fixture_memory[fixture_image+0x13b9820]=2;need(!observe(actual,error)&&error=="Feature event59 gate/list extent invalid","invalid gate rejected");
    reset();fixture_memory[fixture_table+0x10]=0x1000;fixture_memory[fixture_table+0x18]=2;
    recipient(0x1000,0x1100,0x2000,0x3000,0x5000);recipient(0x1100,0,0,0,0);
    need(observe(actual,error),"complete list including native null listener");
    need(actual.recipients.size()==2&&actual.recipients[0].callback==0x5000&&actual.recipients[1].listener==0,"native order and virtual receiver exact");
    fixture_change=true;fixture_head_reads=0;need(!observe(actual,error)&&error=="Feature event59 recipients changed during double read","callback mutation detected");fixture_change=false;
    fixture_memory[0x1100]=0x1000;need(!observe(actual,error)&&error=="Feature event59 recipient cycle/extent invalid","cycle rejected");fixture_memory[0x1100]=0;
    fixture_memory[fixture_table+0x18]=1;need(!observe(actual,error)&&error=="Feature event59 recipient count differs from native list","count mismatch rejected");fixture_memory[fixture_table+0x18]=2;
    fixture_memory[0x3000]=0;need(!observe(actual,error)&&error=="Feature event59 recipient virtual target absent","null callback rejected");fixture_memory[0x3000]=0x5000;
    fixture_memory[0x2000]=0;need(!observe(actual,error)&&error=="Feature event59 recipient virtual target absent","null vtable rejected");fixture_memory[0x2000]=0x3000;
    fixture_memory.erase(0x3000);need(!observe(actual,error)&&error=="fixture original guarded read failed","original read error preserved");
    reset();fixture_memory[fixture_table+0x18]=257;need(!observe(actual,error)&&error=="Feature event59 gate/list extent invalid","count bound rejected");
    reset();fixture_memory[fixture_table+0x10]=0x1000;fixture_memory[fixture_table+0x18]=256;
    for(uintptr_t index=0;index<256;++index)recipient(0x1000+index*16,index==255?0:0x1010+index*16,0,0,0);
    need(observe(actual,error)&&actual.recipients.size()==256,"exact bounded complete list");
    fixture_memory[0x1ff0]=0x2000;recipient(0x2000,0,0,0,0);need(!observe(actual,error)&&error=="Feature event59 recipient cycle/extent invalid","walk bound independent of count");
    std::cout<<fixture_checks<<" Feature event observer assertions passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
