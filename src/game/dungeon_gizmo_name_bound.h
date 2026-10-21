#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
namespace mt2mp::game::dungeon_gizmo_name_bound {
inline std::string widest(const std::string& type,const std::string& separator){
    // Native MakeUniqueName uses "%s #%s" and DoFormatNumber(int32 ID).
    // INT32_MIN has the maximum sign/digit count and three group separators.
    return type+" #-2"+separator+"147"+separator+"483"+separator+"648";
}
struct Text {uintptr_t data=0;uint64_t size=0;std::array<uint64_t,2> storage{};bool operator==(const Text&)const=default;};
static_assert(sizeof(Text)==32);
template<class Read>bool separator(Read& read,uintptr_t image,std::string& out,std::string& error){
    out.clear();Text first,second;const auto address=image+0x1c4eaa0;
    if(!read(address,&first,sizeof first,error))return false;
    if(first.size>8192||(!first.data&&first.size)){error="Gizmo native integer separator string bounds differ";return false;}
    std::string value(size_t(first.size),'\0'),again(size_t(first.size),'\0');
    if((first.size&&!read(first.data,value.data(),value.size(),error))||!read(address,&second,sizeof second,error))return false;
    if(first!=second){error="Gizmo native integer separator owner changed";return false;}
    if(first.size&&!read(second.data,again.data(),again.size(),error))return false;
    if(value!=again||value.find('\0')!=std::string::npos){error="Gizmo native integer separator bytes changed or contain NUL";return false;}
    out=std::move(value);return true;
}
}
