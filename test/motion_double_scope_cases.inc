namespace motion_double_scope {
using MotionRead=mt2mp::game::live_host_state::day::economy::MotionRead;
using NativeHistory=mt2mp::game::live_host_state::day::NativeHistory;
using Target=mt2mp::game::live_host_state::day::Target;
using Pin=mt2mp::game::live_host_state::Pin;
using Motion=mt2mp::game::subscriber_motion::Pose;
namespace subscriber_needs=mt2mp::game::subscriber_needs;
unsigned copies=0,history_calls=0,fail_pass=0;
bool drift=false,throw_read=false;
uintptr_t first_subscriber=0,clock=0;
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){
    ++copies;placement_copy::ProcessMemory reader;return reader.copy(p+offset,&out,sizeof out,e);
}
bool ap(uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){
    uintptr_t value=0;return at(p,0,value,e)&&(value==image+expected||fail(e,"actual motion fixture AP differs"));
}
struct MotionSamplePass {};
bool motion_read(const Pin&,uintptr_t subscriber,std::optional<Motion>& out,std::string& e,
                 void*,void*,MotionSamplePass*){
    int32_t value=0;if(!at(subscriber,0x100,value,e))return false;
    Motion next;next.region=value;out=next;return true;
}
bool finish_motion_pass(const Pin&,const MotionSamplePass&,std::string& e){int32_t value=0;return at(clock,4,value,e);}
struct History {
    NativeHistory source;
    bool read(NativeHistory& out,std::string& e){
        ++history_calls;
        if(throw_read)throw std::runtime_error("original motion leaf exception");
        if(history_calls==fail_pass)return fail(e,"original motion leaf failure");
        if(drift&&history_calls==2)put(first_subscriber,0x100,int32_t(9));
        int32_t value=0;if(!at(clock,0,value,e))return false;
        out=source;return true;
    }
};
struct NativePort {
    History history_;Target actual_day_receipt_;
    bool motion_once(MotionRead&,std::string&);
    bool read_motion(MotionRead&,std::string&);
    bool motion_once_original(MotionRead&,std::string&);
    bool read_motion_original(MotionRead&,std::string&);
};
#include "motion_double_scoped.inc"
#include "motion_double_original.inc"
void setup(NativePort& port){
    std::memset(allocation,0,32768);copies=history_calls=fail_pass=0;drift=throw_read=false;
    const auto base=uintptr_t(allocation),image=base-0x13df690,manager=base+512,data=base+768,business=base+1024;
    clock=base+64;first_subscriber=base+4096;
    port={};port.history_.source.pin.image=image;port.history_.source.pin.clock=clock;port.history_.source.pin.business=business;
    put(base,0,manager);put(manager,0,image+0x1725f40);put(manager,0x40,data);
    put(manager,0x48,int32_t(3));put(manager,0x4c,int32_t(3));put(manager,0x68,int32_t(3));
    for(size_t i=0;i<3;++i){
        const auto subscriber=first_subscriber+i*2048,brain=subscriber+512;
        put(data,i*8,subscriber);put(subscriber,0,image+0x16e75f0);put(subscriber,0x20,int32_t(i));
        put(subscriber,0x88,brain);put(brain,0,image+0x16fdd60);put(brain,0x28,subscriber);
        put(brain+0x180,0,image+0x16c0cf0);put(subscriber,0x100,int32_t(7));
    }
}
void run(){
    NativePort port;MotionRead old,current;std::string e;
    setup(port);auto start=queries();need(port.read_motion_original(old,e),"original actual complete motion pair");
    const auto old_queries=queries()-start;const auto old_copies=copies;
    need(old_queries==2&&old_copies==130&&history_calls==2,"original two private passes retain 130 field copies and each query one homogeneous region");
    setup(port);start=queries();SetLastError(123);
    need(port.read_motion(current,e),"candidate actual complete motion pair");
    const auto new_queries=queries()-start;const auto new_copies=copies;
    need(new_queries==1&&new_copies==old_copies&&history_calls==2,"shared metadata halves OS queries without dropping any field read/pass");
    need(current.rows==old.rows&&current.subscribers==old.subscribers&&current.needs_owners==old.needs_owners&&current.subscriber_slots==old.subscriber_slots,"candidate retains actual selected values and canonical ownership/order");
    need(GetLastError()==123&&timing::page_read_scope==nullptr,"motion reader closes its own scope and preserves LastError");
    history_calls=copies=0;put(first_subscriber,0x100,int32_t(8));start=queries();
    need(port.read_motion(current,e)&&current.rows[0].motion->region==8&&history_calls==2&&copies==old_copies&&queries()-start==1,"next motion pair rereads changed value and new metadata");
    setup(port);drift=true;e.clear();
    need(!port.read_motion(current,e)&&history_calls==2&&e=="economic motion actual owned arrays/poses changed during double read"&&timing::page_read_scope==nullptr,"second-pass value change still rejects with original cause");
    for(unsigned pass=1;pass<=2;++pass){setup(port);fail_pass=pass;e.clear();
        need(!port.read_motion(current,e)&&history_calls==pass&&e=="original motion leaf failure"&&timing::page_read_scope==nullptr,"either full pass failure closes scope and preserves cause");
    }
    setup(port);throw_read=true;bool caught=false;
    try{port.read_motion(current,e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original motion leaf exception";}
    need(caught&&timing::page_read_scope==nullptr,"motion leaf exception propagates and closes metadata");
    setup(port);need(port.read_motion(current,e)&&timing::page_read_scope==nullptr,"reader ends before scalar store");
    need(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(first_subscriber+0x100),8,7)==7,"caller CAS occurs after reader scope ends");
    DWORD prior=0,unused=0;
    need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&prior)!=0,"effect after reader changes owned page protection");e.clear();
    need(!port.read_motion(current,e)&&e=="error: native placement UI page unreadable"&&timing::page_read_scope==nullptr,"next reader freshly rejects effect-changed protection");
    need(VirtualProtect(allocation,4096,prior,&unused)!=0,"restore motion metadata fixture page");
    setup(port);
    {timing::PageReadScope parent;int32_t value=0;need(at(clock,0,value,e),"prime parent reader metadata");
        auto* outer=timing::page_read_scope;
        need(port.read_motion(current,e)&&timing::page_read_scope==outer,"owned motion boundary restores parent scope");
        need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&prior)!=0,"effect after inner reader changes mapping");e.clear();
        need(!at(clock,0,value,e)&&e=="error: native placement UI page unreadable","motion exit invalidates parent metadata before effect");
        need(VirtualProtect(allocation,4096,prior,&unused)!=0,"restore parent metadata fixture page");
    }
    need(timing::page_read_scope==nullptr,"no metadata receipt retained after motion fixture");
    std::cout<<"Motion double-pass OS queries: "<<old_queries<<" -> "<<new_queries<<"; native field copies: "<<old_copies<<" -> "<<new_copies<<"; complete passes: 2 -> 2\n";
}
}
