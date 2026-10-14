#pragma once
#include "network_node_reader.h"
#include <tuple>
namespace mt2mp::game::live_host_state::network {
// Membership receipts live only inside one callback-free scalar block.
class ScalarMembership {
    struct Array {uintptr_t parent=0,data=0;size_t offset=0;int32_t count=0,capacity=0;std::vector<uintptr_t> values;std::map<uintptr_t,size_t> index;};
    struct Slot {std::pair<uintptr_t,size_t> array;size_t index=0;};
    std::map<std::pair<uintptr_t,size_t>,Array> arrays_;
    std::map<uintptr_t,Slot> slots_;
    static bool reject(std::string& e,const std::string& cause){e=cause;return false;}
public:
    template<class Memory>
    bool add(Memory& memory,uintptr_t parent,size_t offset,uintptr_t object,int32_t expected,std::string& e){
        namespace r=network_node_native::read;
        const auto key=std::pair{parent,offset};auto found=arrays_.find(key);
        if(found==arrays_.end()){
            Array a;a.parent=parent;a.offset=offset;
            if(!r::at(memory,parent,offset,a.data,e)||!r::at(memory,parent,offset+8,a.count,e)||!r::at(memory,parent,offset+12,a.capacity,e)||
               !r::array(memory,parent,offset,offset+8,offset+12,65536,a.values,e))return false;
            uintptr_t data=0;int32_t count=-1,capacity=-1;
            if(!r::at(memory,parent,offset,data,e)||!r::at(memory,parent,offset+8,count,e)||!r::at(memory,parent,offset+12,capacity,e))return false;
            if(data!=a.data||count!=a.count||capacity!=a.capacity)return reject(e,"network scalar array changed during capture");
            for(size_t i=0;i<a.values.size();++i)if(!a.values[i]||!a.index.emplace(a.values[i],i).second)return reject(e,"network scalar array null/duplicate member");
            found=arrays_.emplace(key,std::move(a)).first;
        }
        const auto& index=found->second.index;
        const auto slot=index.find(object);
        if(slot==index.end()||(expected>=0&&size_t(expected)!=slot->second))return reject(e,"network scalar target absent from actual ordered parent array");
        if(!slots_.emplace(object,Slot{key,slot->second}).second)return reject(e,"network scalar duplicate target receipt");
        return true;
    }
    template<class Memory>
    bool check(Memory& memory,uintptr_t object,std::string& e)const{
        namespace r=network_node_native::read;
        const auto slot=slots_.find(object);if(slot==slots_.end())return reject(e,"network scalar target receipt absent");
        const auto& a=arrays_.at(slot->second.array);uintptr_t data=0,receiver=0;int32_t count=-1,capacity=-1;
        if(!r::at(memory,a.parent,a.offset,data,e)||!r::at(memory,a.parent,a.offset+8,count,e)||!r::at(memory,a.parent,a.offset+12,capacity,e))return false;
        if(data!=a.data||count!=a.count||capacity!=a.capacity)return reject(e,"network scalar parent array header changed");
        if(!r::at(memory,data,slot->second.index*sizeof(uintptr_t),receiver,e))return false;
        return receiver==object||reject(e,"network scalar target slot changed");
    }
};
}
