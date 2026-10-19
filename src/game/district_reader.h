#pragma once
#include "district_saved_identity.h"
#include "typed_action/district_action.h"
#include "native_polyshape.h"
#include <bit>
#include <cmath>
namespace mt2mp::game::district_reader {
namespace q=district_saved_identity;
using State=action_session::DistrictState;
template<class Memory>
bool ui(Memory& memory,uintptr_t image,uintptr_t selection,uintptr_t object,std::string& e){
    uintptr_t manager=0,focused=0,handler=0,window=0,target=0,proxy=0,weak=0,command=0;uint8_t progress=0,queued=0,visible=0;
    if(!q::at(memory,image,0x13df630,manager,e)||!q::typed(memory,image,manager,0x17205d0,e)||
       !q::at(memory,manager,0xd1,progress,e)||!q::at(memory,manager,0xd3,queued,e)||
       !q::at(memory,manager,0x80,focused,e))return false;
    if(manager!=selection||focused!=object||progress||queued)return q::fail(e,"District normal UI selection manager/focus/edit state differs");
    for(size_t offset:{size_t(0x20),size_t(0x38)}){
        q::Bounds selected;
        if(!q::typed(memory,image,manager+offset,0x1748c50,e)||!q::bounds(memory,manager+offset,1,selected,e)||
           selected.count!=1||!q::at(memory,selected.data,0,target,e)||target!=object)
            return q::fail(e,e.empty()?"District normal UI requires exact single selection":e);
    }
    if(!q::at(memory,manager,0xb8,handler,e)||!q::typed(memory,image,handler,0x1731760,e)||
       !q::at(memory,handler,0x10,window,e)||!q::typed(memory,image,window,0x172bcb0,e)||
       !q::at(memory,image+0x172bcb0,0x168,command,e)||!q::at(memory,window,0x45f,visible,e)||
       !q::at(memory,window,0x4f0,target,e)||!q::at(memory,window,0x4f8,proxy,e)||!q::at(memory,proxy,0,weak,e))return false;
    return (command==image+0x6cb400&&visible==1&&target==object&&weak==object+0x20)||
        q::fail(e,"District normal InfoWindow command/visibility/weak owner differs");
}
template<class Memory>
bool text(Memory& memory,uintptr_t object,size_t offset,std::string& out,std::string& e){
    uintptr_t data=0;uint64_t count=0;
    if(!q::at(memory,object,offset,data,e)||!q::at(memory,object,offset+8,count,e))return false;
    if(count>4096||(count&&(!data||data>UINTPTR_MAX-count)))return q::fail(e,"District native text span invalid");
    out.resize(size_t(count));return !count||memory.read(data,out.data(),out.size(),e);
}
template<class Memory,class Contains>
bool read(Memory& memory,uintptr_t image,uintptr_t map,const q::Current& target,Contains&& contains,State& out,std::string& e){
    State result;result.region=target.saved_region;const auto object=target.row.pointer;
    uintptr_t root=0,region=0,selected=0,manager=0,owner=0;uint64_t uid=0;
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& error){return q::at(memory,p,offset,value,error);};
    if(!q::typed(memory,image,object,0x16da000,e)||!q::at(memory,object,0x18,uid,e)||
       !q::at(memory,object,0x170,root,e)||!q::at(memory,object,0x178,region,e)||
       !region_saved_identity::resolve(image,map,map+0x1c0,target.saved_region,region,selected,at,e)||
       !q::at(memory,region,0x970,manager,e)||!q::typed(memory,image,manager,0x1719080,e)||
       !q::at(memory,manager,0x60,owner,e))return false;
    if(root!=map+0x1c0||owner!=region||uid!=target.row.uid||!uid)return q::fail(e,"District current root/Region/full UID differs");
    if(!text(memory,object,0x38,result.name,e)||!text(memory,object,0x2b8,result.description,e)||
       !q::at(memory,object,0x100,result.pose,e)||!q::at(memory,object,0x218,result.geo,e)||
       !q::at(memory,object,0x2d8,result.constructed_day,e)||!q::at(memory,object,0x2dc,result.saved_version,e))return false;
    // Only declared scalar properties enter shared state. Native title/overlay
    // allocation pointers and padding are local presentation details.
    constexpr std::array<size_t,33> offsets{0x290,0x294,0x298,0x29c,0x2a0,0x2a4,0x2a8,0x2ac,
        0x2e0,0x2e4,0x2e8,0x2ec,0x2f0,0x2f4,0x2f8,0x2fc,0x300,0x304,0x308,0x30c,0x310,0x314,0x318,
        0x328,0x32c,0x330,0x334,0x338,0x33c,0x340,0x344,0x348,0x34c};
    constexpr std::array<size_t,13> booleans{0x2b0,0x2b1,0x31c,0x31d,0x31e,0x31f,0x320,0x321,0x322,0x323,0x324,0x350,0x351};
    for(size_t i=0;i<offsets.size();++i)if(!q::at(memory,object,offsets[i],result.values[i],e))return false;
    for(size_t i=0;i<booleans.size();++i)if(!q::at(memory,object,booleans[i],result.flags[i],e))return false;
    struct Copy {Memory& memory;bool copy(uintptr_t p,void* out,size_t n,std::string& error){return memory.read(p,out,n,error);}} copy{memory};
    if(!ui_terrain_runtime::poly::read(copy,object+0x220,result.shape,e))return false;
    if(result.geo<0||result.geo>2)return q::fail(e,"District native geography invalid actual="+std::to_string(result.geo));
    for(auto value:result.flags)if(value>1)return q::fail(e,"District native boolean invalid");
    for(auto value:result.values)if(!std::isfinite(std::bit_cast<float>(value)))return q::fail(e,"District native property is not finite");
    for(auto value:result.pose)if(!std::isfinite(std::bit_cast<float>(value)))return q::fail(e,"District native pose is not finite");
    if(result.flags[11]&&!contains(object+0x220,object+0x344,result.manual_title_inside,e))return false;
    out=std::move(result);return true;
}
}
