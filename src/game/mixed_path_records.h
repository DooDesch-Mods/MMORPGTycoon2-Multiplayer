#pragma once
#include "mixed_path_reader.h"
#include "duplicate_records.h"
#include "subscriber_lifecycle/capture.h"
#include <map>

namespace mt2mp::game::mixed_path_records {
namespace records=subscriber_lifecycle;
struct Witness {std::map<std::string,duplicate_records::Witness> objects;};
inline bool reject(std::string& error,const std::string& cause){error="mixed Path original Record: "+cause;return false;}
inline std::string canonical(uintptr_t pointer,const mixed_path_native::Census& census,
                             const std::map<uint64_t,std::string>& keys){
    for(const auto& region:census.regions)if(region.object==pointer)return "region:"+std::to_string(region.id);
    for(const auto& path:census.paths)if(path.member.object==pointer){const auto found=keys.find(path.member.uid);return found==keys.end()?std::string{}:found->second;}
    for(const auto& node:census.nodes)if(node.member.object==pointer){const auto found=keys.find(node.member.uid);return found==keys.end()?std::string{}:found->second;}
    return {};
}
template<class NativeMemory>struct Memory final:records::Memory {
    NativeMemory& native;
    explicit Memory(NativeMemory& value):native(value){}
    bool read(uintptr_t address,void* target,size_t size,std::string& error)override {
        return native.copy(address,target,size,error);
    }
};
template<class NativeMemory>struct Capture {
    Memory<NativeMemory> memory;records::Graph graph;
    explicit Capture(NativeMemory& value):memory(value){}
    static bool inspect(void* user,const void* record,const void* context,std::string& error){
        auto& value=*static_cast<Capture*>(user);
        return records::read_graph(value.memory,uintptr_t(record),uintptr_t(context),
                                   {100000,1000000,32*1024*1024,100000},value.graph,error);
    }
};
template<class NativeMemory>
bool read_one(NativeMemory& memory,uintptr_t image,uintptr_t object,bool path,
              const mixed_path_native::Census& census,const std::map<uint64_t,std::string>& keys,
              duplicate_records::Witness& out,std::string& error){
    const uintptr_t prepare=path?0x10f51c0:0xf40e50,save=path?0x10f5220:0xf40eb0;
    Capture<NativeMemory> value(memory);records::CaptureReceipt cleanup;
    const records::CaptureApi api{
        reinterpret_cast<records::Construct>(image+0x9667c0),
        reinterpret_cast<records::Construct>(image+0x9676a0),
        reinterpret_cast<records::Construct>(image+0x89e580),
        reinterpret_cast<records::Construct>(image+0x89e5f0),
        reinterpret_cast<records::PrepareSave>(image+prepare),
        reinterpret_cast<records::SaveRecord>(image+save)};
    if(!records::capture_original(api,reinterpret_cast<const void*>(object),Capture<NativeMemory>::inspect,&value,cleanup,error))return false;
    auto owner=[&](uintptr_t pointer,std::string& key,std::string& cause){
        key=canonical(pointer,census,keys);
        return !key.empty()||reject(cause,"external SaveContext owner lacks canonical Region/Path/Intersection identity");};
    return duplicate_records::summarize(value.graph,object,owner,out,error);
}
template<class NativeMemory>
bool read_all(NativeMemory& memory,uintptr_t image,const mixed_path_native::Census& census,
              const std::map<uint64_t,std::string>& keys,Witness& out,std::string& error){
    Witness value;
    auto record=[&](const mixed_path_native::Member& member,bool path){
        const auto found=keys.find(member.uid);
        if(found==keys.end()||found->second.empty())return reject(error,"owned native object lacks canonical key");
        duplicate_records::Witness row;
        if(!read_one(memory,image,member.object,path,census,keys,row,error))return false;
        return value.objects.emplace(found->second,std::move(row)).second||reject(error,"canonical Record key repeated");};
    for(const auto& row:census.paths)if(!record(row.member,true))return false;
    for(const auto& row:census.nodes)if(!record(row.member,false))return false;
    out=std::move(value);return true;
}
}
