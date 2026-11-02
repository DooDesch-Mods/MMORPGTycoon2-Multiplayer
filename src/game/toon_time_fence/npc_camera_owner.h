#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::toon_time_fence::npc_camera_owner {
template<class Read>
bool admit(Read&& read, uintptr_t image, uintptr_t root, uintptr_t npc, uintptr_t caller,
           bool& admitted, std::string& error) {
    admitted=false;
    if(!root||caller!=image+0x297a34)return true;
    uintptr_t ap=0, shard=0, region=0, manager=0, reciprocal=0, data=0;
    int32_t kind=-1, mode=-1, count=-1, capacity=-1;
    if(!read(npc,0,ap,error)||!read(npc,0x170,shard,error)||!read(npc,0x178,region,error)||
       !read(npc,0x80c,kind,error)||!read(npc,0x838,mode,error))return false;
    if(ap!=image+0x1742980||shard!=root||!region||kind<0||kind>2){error="NPC camera current class, root, region or kind differs";return false;}
    if(mode!=2)return true;
    const size_t offset=size_t(kind)*0x30;
    if(!read(region,0x998,manager,error))return false;
    if(!manager){error="NPC camera current manager missing";return false;}
    if(!read(manager,0xc8,reciprocal,error)||
       !read(manager,0x40+offset,data,error)||!read(manager,0x48+offset,count,error)||
       !read(manager,0x4c+offset,capacity,error))return false;
    if(reciprocal!=region||count<0||capacity<count||count>1000000||(count&&!data)){
        error="NPC camera current manager or bucket differs";return false;
    }
    int matches=0;
    for(int32_t i=0;i<count;++i){uintptr_t item=0;if(!read(data,size_t(i)*sizeof(uintptr_t),item,error))return false;if(item==npc)++matches;}
    if(matches!=1){error="NPC camera current bucket membership differs";return false;}
    admitted=true;return true;
}
}
