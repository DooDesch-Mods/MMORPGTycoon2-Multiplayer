#pragma once
#include "state.h"
#include "message_progress_contract.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace mt2mp::game::tutorial_replication::message_progress {
struct Binding {
    uintptr_t sequence=0,steps=0,step=0,step_ap=0,window=0,meter=0,meter_window=0;
    int32_t count=0,capacity=0,next=0;
    uint8_t step_complete=0,window_shown=0;
    std::vector<uintptr_t> entries;
    std::optional<MessageProgress> values;
    bool operator==(const Binding&)const=default;
};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
template<class Memory,class T>bool at(Memory& memory,uintptr_t owner,size_t offset,T& out,std::string& error){
    if(!owner||owner>std::numeric_limits<uintptr_t>::max()-offset)return fail(error,"tutorial progress owner/offset invalid");
    return memory.copy(owner+offset,&out,sizeof(out),error);
}
template<class Memory>bool read_once(Memory& memory,uintptr_t image,uintptr_t sequence,int32_t expected,Binding& out,std::string& error){
    out={};out.sequence=sequence;uintptr_t ap=0;
    if(!at(memory,sequence,0,ap,error))return false;
    if(ap!=image+0x16dbd80)return fail(error,"tutorial progress Sequence AP differs");
    if(!at(memory,sequence,0x20,out.steps,error)||!at(memory,sequence,0x28,out.count,error)||!at(memory,sequence,0x2c,out.capacity,error)||
       !at(memory,sequence,0x48,out.step,error)||!at(memory,sequence,0x50,out.next,error))return false;
    if(out.count<0||out.count>2048||out.capacity<out.count||out.capacity>2048||out.next!=expected||out.next<0||out.next>out.count)
        return fail(error,"tutorial progress indexed cursor/count differs");
    if(!out.step)return out.next==0||fail(error,"tutorial progress current step absent");
    if(!out.steps||out.next<1||out.steps>UINTPTR_MAX-size_t(out.count)*sizeof(uintptr_t))return fail(error,"tutorial progress step array invalid");
    out.entries.resize(size_t(out.count));
    if(!memory.copy(out.steps,out.entries.data(),out.entries.size()*sizeof(uintptr_t),error))return false;
    if(out.entries[size_t(out.next-1)]!=out.step||std::count(out.entries.begin(),out.entries.end(),out.step)!=1)
        return fail(error,"tutorial progress current step is not uniquely indexed");
    if(!at(memory,out.step,0,out.step_ap,error)||!at(memory,out.step,0x18,out.step_complete,error))return false;
    if(out.step_complete>1)return fail(error,"tutorial progress step complete byte invalid");
    if(out.step_ap!=image+0x16fda00)return true;
    if(!at(memory,out.step,0x1a8,out.window,error)||!at(memory,out.step,0x1b0,out.meter,error))return false;
    if(!out.meter)return true;
    if(!at(memory,out.window,0,ap,error))return false;
    if(ap!=image+0x17883d0)return fail(error,"tutorial progress Window AP differs");
    if(!at(memory,out.window,0x69,out.window_shown,error)||out.window_shown>1)return fail(error,"tutorial progress Window visibility invalid");
    if(!at(memory,out.meter,0,ap,error))return false;
    if(ap!=image+0x174a110)return fail(error,"tutorial progress Meter AP differs");
    if(!at(memory,out.meter,0x280,out.meter_window,error))return false;
    if(out.meter_window!=out.window)return fail(error,"tutorial progress Meter belongs to another Window");
    MessageProgress values;uint8_t shown=0;
    if(!at(memory,out.meter,0x69,shown,error)||!at(memory,out.meter,0x3a4,values.value,error)||
       !at(memory,out.meter,0x3a8,values.minimum,error)||!at(memory,out.meter,0x3ac,values.maximum,error))return false;
    if(shown>1)return fail(error,"tutorial progress Meter visibility invalid");
    values.shown=shown!=0;if(!valid(values,error))return false;out.values=values;return true;
}
template<class Memory>bool read(Memory& memory,uintptr_t image,uintptr_t sequence,int32_t expected,Binding& out,std::string& error){
    Binding before,after;if(!read_once(memory,image,sequence,expected,before,error)||!read_once(memory,image,sequence,expected,after,error))return false;
    if(before!=after)return fail(error,"tutorial progress native ownership/value changed during read");
    out=std::move(after);error.clear();return true;
}
template<class Memory,class Calls>bool apply(Memory& memory,uintptr_t image,uintptr_t sequence,int32_t expected,
        const std::optional<MessageProgress>& target,Calls& calls,bool& attempted,std::string& error,const Fingerprints& fingerprints=production_fingerprints){
    if(target&&!valid(*target,error))return false;
    if(!verify_code(memory,image,error,fingerprints))return false;
    Binding before;if(!read(memory,image,sequence,expected,before,error))return false;
    if(before.values.has_value()!=target.has_value())return fail(error,"tutorial progress Host/local Meter presence differs");
    if(before.values==target)return true;
    attempted=true;
    if(target->shown&&!calls.label(before.meter,error))return false;
    if(!calls.shown(before.meter,target->shown,error)||!calls.value(before.meter,*target,error))return false;
    Binding after;if(!read(memory,image,sequence,expected,after,error))return false;
    auto left=before,right=after;left.values.reset();right.values.reset();
    if(left!=right)return fail(error,"tutorial progress ownership changed during native presentation");
    if(after.values!=target)return fail(error,"tutorial progress native presentation readback differs from Host");
    error.clear();return true;
}
}
