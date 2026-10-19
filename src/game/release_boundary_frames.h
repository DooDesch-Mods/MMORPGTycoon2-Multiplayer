#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::release_boundary {
enum class Origin {Other,Finish,Skip,Begin,Immediate};
inline Origin origin(uintptr_t immediate,const std::vector<uintptr_t>& native_frames){
    if(immediate!=0x4faf6a||native_frames.size()<3||native_frames[0]!=immediate||native_frames[1]!=0x4fc025)return Origin::Other;
    switch(native_frames[2]){
    case 0x4e6d0c:return Origin::Finish;
    case 0x4e6dec:return Origin::Skip;
    case 0x4e6f53:return Origin::Begin;
    case 0x79be1f:return Origin::Immediate;
    default:return Origin::Other;
    }
}
// Uses Windows unwind metadata and the current thread's stack bounds. It never
// treats an arbitrary stack word as a return address.
bool current_native_frames(uintptr_t image,size_t image_bytes,std::vector<uintptr_t>&,std::string&);
}
