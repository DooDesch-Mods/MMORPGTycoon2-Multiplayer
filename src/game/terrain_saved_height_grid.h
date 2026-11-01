#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::terrain_height {
// Native RecordV2 cellHeight stores every float payload, including untouched
// allocation bits. Snapshot equality preserves those bits without float math.
// Editing continues to use the finite-only terrain_height::read contract.
struct SavedGrid {
    int32_t width=0,height=0;float cell_size=0;std::array<float,3> origin{};
    std::vector<uint32_t> samples;
};
template<class Read>bool read_saved_bits(uintptr_t image,uintptr_t region,SavedGrid& out,Read&& at,std::string& error){
    out={};SavedGrid value;uintptr_t ap=0,data=0;int32_t count=-1,capacity=-1,cell=-1;
    const auto reject=[&](const std::string& cause){
        error=cause+" region="+std::to_string(region)+" cell="+std::to_string(cell)+
            " width="+std::to_string(value.width)+" height="+std::to_string(value.height)+
            " count="+std::to_string(count)+" capacity="+std::to_string(capacity)+
            " array_ap="+std::to_string(ap)+" data="+std::to_string(data);return false;
    };
    if(!at(region,0xc0,value.width,error)||!at(region,0xc4,value.height,error)||
       !at(region,0xc8,value.cell_size,error)||!at(region,0x138,value.origin,error)||
       !at(region,0xe8,ap,error)||!at(region,0xf0,data,error)||
       !at(region,0xf8,count,error)||!at(region,0xfc,capacity,error))return reject(error);
    if(ap!=image+0x1749340||value.width<1||value.height<1||
       int64_t(value.width)*value.height>1000000||count!=int64_t(value.width)*value.height||
       capacity<count||!data||!std::isfinite(value.cell_size)||value.cell_size<=0)
        return reject("terrain saved height native array/dimensions/cell size differ");
    for(float f:value.origin)if(!std::isfinite(f))return reject("terrain saved height native grid origin is not finite");
    value.samples.reserve(size_t(count));
    for(cell=0;cell<count;++cell){uint32_t bits=0;
        if(!at(data,size_t(cell)*4,bits,error))return reject(error);
        value.samples.push_back(bits);
    }
    uintptr_t current=0;int32_t actual=-1;
    if(!at(region,0xf0,current,error)||!at(region,0xf8,actual,error))return reject(error);
    if(current!=data||actual!=count)return reject("terrain saved height array changed during read");
    out=std::move(value);error.clear();return true;
}
}
