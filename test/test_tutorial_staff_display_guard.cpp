#include "tutorial_replication/owned_staff_display_entry.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace sd=mt2mp::game::tutorial_replication::staff_display;
using Owner=mt2mp::game::tutorial_sync::OwnedSequenceEntry;
namespace {
unsigned checks=0;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t unreadable=0;unsigned reads=0;
    template<class T>void put(uintptr_t p,T value){const auto* b=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(value);++i)bytes[p+i]=b[i];}
    bool copy(uintptr_t p,void* out,size_t count,std::string& e){++reads;if(p==unreadable){e="original physical hook read failed";return false;}auto* b=static_cast<unsigned char*>(out);for(size_t i=0;i<count;++i){auto it=bytes.find(p+i);if(it==bytes.end()){e="fixture unmapped code";return false;}b[i]=it->second;}return true;}
};
struct Code:Memory {
    Owner owner{0x100000,0x300000,0x200000,true,true};
    Code(bool negative=false){if(negative)std::swap(owner.target,owner.trampoline);for(size_t i=0;i<sd::display_entry.size();++i)bytes[owner.target+i]=sd::display_entry[i];for(size_t i=0;i<6;++i)bytes[owner.trampoline+i]=sd::display_entry[i];
        constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};for(size_t i=0;i<6;++i){bytes[owner.trampoline+6+i]=jump[i];bytes[owner.trampoline+20+i]=jump[i];}
        put(owner.trampoline+12,owner.target+6);put(owner.trampoline+26,owner.detour);bytes[owner.target]=0xe9;
        put<int32_t>(owner.target+1,int32_t(int64_t(owner.trampoline+20)-int64_t(owner.target+5)));
    }
    bool verify(std::string& e){return sd::verify_owned_display_entry(owner,owner.target,[&](auto p,auto out,auto n,auto& error){return copy(p,out,n,error);},e);}
};
}
int main(){try{
    std::string e;for(bool negative:{false,true}){Code c(negative);need(c.verify(e),"exact own forward/backward physical route rejected");++checks;}
    {Code c;c.owner.enabled=false;need(!c.verify(e)&&c.reads==0&&e.find("enabled owner")!=std::string::npos,"unpublished owner read as trusted code");++checks;}
    {Code c;c.bytes[c.owner.target+15]^=1;need(!c.verify(e)&&e.find("suffix")!=std::string::npos,"unchanged entry suffix not checked");++checks;}
    {Code c;c.bytes[c.owner.trampoline+5]^=1;need(!c.verify(e)&&e.find("original/continuation")!=std::string::npos,"sixth copied native byte ignored");++checks;}
    {Code c;c.put(c.owner.trampoline+12,c.owner.target+5);need(!c.verify(e)&&e.find("continuation")!=std::string::npos,"wrong native return offset admitted");++checks;}
    {Code c;c.put(c.owner.trampoline+26,c.owner.detour+1);need(!c.verify(e)&&e.find("detour")!=std::string::npos,"foreign relay detour admitted");++checks;}
    {Code c;c.unreadable=c.owner.trampoline;need(!c.verify(e)&&e=="original physical hook read failed","original physical read cause lost");++checks;}
    {Code c;c.put<int32_t>(c.owner.target+1,0);need(!c.verify(e)&&e.find("relay")!=std::string::npos,"unowned entry jump inherited ownership");++checks;}
    std::cout<<checks<<" actual owned-display hook guard checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
