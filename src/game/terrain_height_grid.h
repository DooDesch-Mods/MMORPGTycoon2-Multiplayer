#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::terrain_height {
struct Grid {int32_t width=0,height=0;float cell_size=0;std::array<float,3> origin{};std::vector<float> values;bool operator==(const Grid&)const=default;};
template<class Read>bool read(uintptr_t image,uintptr_t region,Grid& out,Read&& at,std::string& error){
    out={};Grid value;uintptr_t ap=0,data=0;int32_t count=-1,capacity=-1;
    if(!at(region,0xc0,value.width,error)||!at(region,0xc4,value.height,error)||!at(region,0xc8,value.cell_size,error)||!at(region,0x138,value.origin,error)||!at(region,0xe8,ap,error)||!at(region,0xf0,data,error)||!at(region,0xf8,count,error)||!at(region,0xfc,capacity,error))return false;
    if(ap!=image+0x1749340||value.width<1||value.height<1||int64_t(value.width)*value.height>1000000||count!=int64_t(value.width)*value.height||capacity<count||!data||!std::isfinite(value.cell_size)||value.cell_size<=0){error="terrain height native array/dimensions/cell size differ";return false;}
    for(float f:value.origin)if(!std::isfinite(f)){error="terrain height native grid origin is not finite";return false;}
    for(int32_t i=0;i<count;++i){float f=0;if(!at(data,size_t(i)*4,f,error))return false;if(!std::isfinite(f)){error="terrain height native sample is not finite";return false;}value.values.push_back(f);}
    uintptr_t current=0;int32_t actual=-1;if(!at(region,0xf0,current,error)||!at(region,0xf8,actual,error))return false;
    if(current!=data||actual!=count){error="terrain height array changed during read";return false;}out=std::move(value);error.clear();return true;
}
inline bool index(const Grid& grid,const std::array<float,3>& point,int32_t& out,std::string& error){
    out=-1;if(grid.width<1||grid.height<1||grid.values.size()!=size_t(grid.width)*size_t(grid.height)||!std::isfinite(grid.cell_size)||grid.cell_size<=0){error="terrain height cell geometry invalid";return false;}
    const float x=std::floor((point[0]-grid.origin[0])/grid.cell_size),z=std::floor((point[2]-grid.origin[2])/grid.cell_size);
    if(!std::isfinite(x)||!std::isfinite(z)){error="terrain height cell coordinate invalid";return false;}
    auto clamp=[](float f,int32_t count){return f<0?0:f>=float(count)?count-1:int32_t(f);};out=clamp(z,grid.height)*grid.width+clamp(x,grid.width);error.clear();return true;
}
inline bool delta(const Grid& before,const Grid& after,int32_t cell,bool up,std::string& error){
    auto geometry=after;geometry.values=before.values;
    if(geometry!=before||cell<0||size_t(cell)>=before.values.size()||after.values.size()!=before.values.size()){error="terrain height native geometry/index changed";return false;}
    for(size_t i=0;i<before.values.size();++i){const float wanted=before.values[i]+(i==size_t(cell)?(up?20.0f:-20.0f):0.0f);if(std::bit_cast<uint32_t>(after.values[i])!=std::bit_cast<uint32_t>(i==size_t(cell)?wanted:before.values[i])){error="terrain height original cell consequence differs index="+std::to_string(i);return false;}}
    error.clear();return true;
}
}
