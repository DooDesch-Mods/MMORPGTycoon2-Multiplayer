#pragma once
#include "read.h"
#include <cstring>
#include <set>
namespace mt2mp::game::developer_staff {
template<class Copy>
bool read_window(Copy&& copy,const Capture& c,uintptr_t& out,std::string& e){
    out=0;auto fail=[&](const char* cause){e=cause;return false;};
    auto at=[&](uintptr_t owner,size_t offset,auto& value){
        if(!owner||owner>UINTPTR_MAX-offset||owner+offset>UINTPTR_MAX-sizeof(value))return fail("Developer fixture owned field address overflow");
        return copy(owner+offset,&value,sizeof(value),e);
    };
    uintptr_t data=0;int32_t count=0,capacity=0;const auto mode=uintptr_t(c.pin.origin.mode);
    if(!at(mode,0xa0,data)||!at(mode,0xa8,count)||!at(mode,0xac,capacity))return false;
    if(!data||count<1||capacity<count||capacity>512)return fail("Developer fixture actual Mode window bounds differ");
    std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t window=0,ap=0;
        if(!at(data,size_t(i)*8,window))return false;
        if(!window||!seen.insert(window).second)return fail("Developer fixture Mode windows null/duplicate");
        if(!at(window,0,ap))return false;
        if(ap!=c.image+0x1722e60)continue;
        uintptr_t text=0,logic=0;uint64_t length=0;std::array<char,12> id{};
        if(!at(window,0x1b0,text)||!at(window,0x1b8,length)||!at(window,0x318,logic))return false;
        if(out||logic||length!=11||!text)return fail("Developer fixture DevelopmentWindow identity/logic differs");
        if(!copy(text,id.data(),id.size(),e))return false;
        if(std::memcmp(id.data(),"development",12))return fail("Developer fixture native window ID differs");
        out=window;
    }
    return out!=0||fail("Developer fixture actual owned DevelopmentWindow absent");
}
}
