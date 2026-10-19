#pragma once
#include "npc_quest_targets.h"
#include "vendor/nlohmann/json.hpp"
#include <cmath>

namespace mt2mp::game::scenery_stroke_bugs {
template<class Memory,class Hash>bool read(Memory& memory,uintptr_t image,uintptr_t region,Hash&& hash,uint64_t& total,std::string& digest,std::string& e){
    namespace q=npc_quest_targets;total=0;digest.clear();auto rows=nlohmann::json::array();
    for(size_t family=0;family<2;++family){
        uintptr_t manager=0;if(!q::at(memory,region,family?0x998:0x950,manager,e)||!manager)return q::fail(e,e.empty()?"Scenery stroke bug target manager is absent":e);
        for(size_t bucket=0;bucket<(family?3u:1u);++bucket){
            const size_t offset=family?0x40+bucket*0x30:0x410;uintptr_t data=0;int32_t count=0,capacity=0;
            if(!q::at(memory,manager,offset,data,e)||!q::at(memory,manager,offset+8,count,e)||!q::at(memory,manager,offset+12,capacity,e))return false;
            if(count<0||count>1000000||capacity<count||capacity>1000000||(!data&&capacity))return q::fail(e,"Scenery stroke bug target native array differs");
            for(int32_t index=0;index<count;++index){
                uintptr_t target=0,ap=0,collection=0;std::array<float,3> position{};
                if(!q::at(memory,data,size_t(index)*8,target,e)||!q::at(memory,target,0,ap,e)||!q::at(memory,target,0x110,position,e)||
                   !q::at(memory,target,family?0x848:0xbd8,collection,e))return false;
                if(ap<image||ap-image>0x2000000)return q::fail(e,"Scenery stroke bug target class is outside the exact image");
                for(float part:position)if(!std::isfinite(part))return q::fail(e,"Scenery stroke bug target position is nonfinite");
                auto bugs=nlohmann::json::array();
                if(collection){uintptr_t anchor=0,objects=0;int32_t size=0,cap=0;
                    if(!q::typed(memory,image,collection,0x1705640,e)||!q::at(memory,collection,0x48,anchor,e)||anchor!=target||
                       !q::at(memory,collection,0x20,objects,e)||!q::at(memory,collection,0x28,size,e)||!q::at(memory,collection,0x2c,cap,e))
                        return q::fail(e,e.empty()?"Scenery stroke bug collection reciprocal owner differs":e);
                    if(size<0||size>1000000||cap<size||cap>1000000||(!objects&&cap)||uint64_t(size)>1000000-total)
                        return q::fail(e,"Scenery stroke complete bug collection span exceeds its bound");
                    for(int32_t item=0;item<size;++item){uintptr_t bug=0,owner=0;std::array<uint8_t,27> values{};
                        if(!q::at(memory,objects,size_t(item)*8,bug,e)||!q::typed(memory,image,bug,0x1742200,e)||
                           !q::at(memory,bug,0x50,owner,e)||owner!=target||!q::at(memory,bug,0x2c,values,e))
                            return q::fail(e,e.empty()?"Scenery stroke bug reciprocal anchor differs":e);
                        bugs.push_back(values);
                    }
                    total+=uint64_t(size);uintptr_t repeat_data=0;int32_t repeat_size=0,repeat_cap=0;
                    if(!q::at(memory,collection,0x20,repeat_data,e)||!q::at(memory,collection,0x28,repeat_size,e)||!q::at(memory,collection,0x2c,repeat_cap,e))return false;
                    if(repeat_data!=objects||repeat_size!=size||repeat_cap!=cap)return q::fail(e,"Scenery stroke bug collection changed during complete read");
                }
                // Local IDs and pointers are excluded. Original target order
                // and every initialized authored bug value remain in the hash.
                rows.push_back(nlohmann::json::array({family,bucket,index,ap-image,position,std::move(bugs)}));
            }
            uintptr_t repeat_data=0;int32_t repeat_count=0,repeat_capacity=0;
            if(!q::at(memory,manager,offset,repeat_data,e)||!q::at(memory,manager,offset+8,repeat_count,e)||!q::at(memory,manager,offset+12,repeat_capacity,e))return false;
            if(repeat_data!=data||repeat_count!=count||repeat_capacity!=capacity)return q::fail(e,"Scenery stroke bug target array changed during complete read");
        }
    }
    return hash(rows.dump(),digest,e);
}
}
