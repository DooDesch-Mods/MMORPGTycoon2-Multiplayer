#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace mt2mp::game::building_uptime {
using Values=std::vector<int32_t>;
inline bool valid(const Values& value){
    if(value.size()>10)return false;
    for(auto bit:value)if(bit!=0&&bit!=1)return false;
    return true;
}
struct Native {
    uintptr_t data=0;int32_t count=0,capacity=0,retention=0;Values values;
    bool operator==(const Native&)const=default;
};
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
template<class Memory>
bool read(Memory& memory,uintptr_t image,uintptr_t building,Native& out,std::string& e){
    out={};uintptr_t ap=0,array_ap=0;
    if(!building||building>UINTPTR_MAX-0xc94||image>UINTPTR_MAX-0x1749370)return fail(e,"Building uptime address invalid");
    if(!memory.copy(building+0xc60,&ap,sizeof ap,e)||!memory.copy(building+0xc78,&array_ap,sizeof array_ap,e)||
       !memory.copy(building+0xc80,&out.data,sizeof out.data,e)||!memory.copy(building+0xc88,&out.count,sizeof out.count,e)||
       !memory.copy(building+0xc8c,&out.capacity,sizeof out.capacity,e)||!memory.copy(building+0xc90,&out.retention,sizeof out.retention,e))return false;
    if(ap!=image+0x17138d0||array_ap!=image+0x1749370||out.retention!=10||out.count<0||out.count>10||
       out.capacity<out.count||out.capacity>100000||(out.capacity&&!out.data)||out.data>UINTPTR_MAX-size_t(out.capacity)*sizeof(int32_t))
        return fail(e,"Building uptime class/history span/retention differs");
    out.values.resize(size_t(out.count));
    if(out.count&&!memory.copy(out.data,out.values.data(),out.values.size()*sizeof(int32_t),e))return false;
    return valid(out.values)||fail(e,"Building uptime history is not Boolean int32");
}
struct Receipt {bool attempted=false,unknown=false,confirmed=false;uint32_t stores=0;};
template<class Memory,class Compare,class Reserve,class Guard>
bool apply(Memory& memory,uintptr_t image,uintptr_t building,const Values& before,const Values& target,
           Compare&& compare,Reserve&& reserve,Guard&& guard,Receipt& out,std::string& e){
    out={};Native actual;
    if(!valid(before)||!valid(target))return fail(e,"Building uptime target history invalid");
    if(!guard(e)||!read(memory,image,building,actual,e))return false;
    if(actual.values!=before)return fail(e,"Building uptime complete before differs");
    if(before==target){out.confirmed=true;return true;}
    const auto failed=[&](){out.unknown=out.attempted;return false;};
    if(size_t(actual.capacity)<target.size()){
        if(!guard(e))return false;out.attempted=true;
        if(!reserve(building+0xc78,10,e))return failed();
        Native grown;if(!read(memory,image,building,grown,e))return failed();
        if(grown.values!=actual.values||grown.count!=actual.count||grown.capacity!=10||grown.retention!=actual.retention){
            fail(e,"Building uptime original Reserve readback differs");return failed();}
        actual=std::move(grown);
    }
    for(size_t index=0;index<target.size();++index){
        int32_t old=0;if(!guard(e)||!memory.copy(actual.data+index*4,&old,sizeof old,e))return failed();
        if(old==target[index])continue;
        out.attempted=true;if(!compare(actual.data+index*4,&old,&target[index],sizeof old,e))return failed();++out.stores;
    }
    const auto count=int32_t(target.size());
    if(actual.count!=count){
        if(!guard(e))return failed();out.attempted=true;
        if(!compare(building+0xc88,&actual.count,&count,sizeof count,e))return failed();++out.stores;
    }
    Native after;if(!guard(e)||!read(memory,image,building,after,e))return failed();
    if(after.data!=actual.data||after.capacity!=actual.capacity||after.retention!=actual.retention||after.values!=target){
        fail(e,"Building uptime complete after/owner differs");return failed();}
    out.confirmed=true;e.clear();return true;
}
}
