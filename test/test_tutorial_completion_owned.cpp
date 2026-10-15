#include "tutorial_replication/owned_scenario_start_entry.h"
#include "tutorial_replication/owned_advance_entry.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace sync=mt2mp::game::tutorial_sync;
namespace {
unsigned checks=0;
void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Code {
    sync::OwnedSequenceEntry owner{0x100000,0x300000,0x200000,true,true};
    std::map<uintptr_t,unsigned char> bytes;uintptr_t unreadable=0;unsigned reads=0;
    size_t copied=0;
    template<class T>void put(uintptr_t p,T value){const auto* b=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[p+i]=b[i];}
    Code(bool start,bool negative):copied(start?6:5){if(negative)std::swap(owner.target,owner.trampoline);const auto& entry=start?sync::scenario_start_entry:sync::advance_entry;
        for(size_t i=0;i<entry.size();++i)bytes[owner.target+i]=entry[i];for(size_t i=0;i<copied;++i)bytes[owner.trampoline+i]=entry[i];
        constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};for(size_t i=0;i<6;++i){bytes[owner.trampoline+copied+i]=jump[i];bytes[owner.trampoline+copied+14+i]=jump[i];}
        put(owner.trampoline+copied+6,owner.target+copied);put(owner.trampoline+copied+20,owner.detour);bytes[owner.target]=0xe9;
        put<int32_t>(owner.target+1,int32_t(int64_t(owner.trampoline+copied+14)-int64_t(owner.target+5)));
    }
    bool copy(uintptr_t p,void* out,size_t count,std::string& e){++reads;if(p==unreadable){e="original code read failed";return false;}auto* b=static_cast<unsigned char*>(out);for(size_t i=0;i<count;++i){auto it=bytes.find(p+i);if(it==bytes.end()){e="fixture code unmapped";return false;}b[i]=it->second;}return true;}
    bool verify(bool start,std::string& e){auto read=[&](auto p,auto out,auto n,auto& error){return copy(p,out,n,error);};return start?sync::verify_owned_scenario_start_entry(owner,owner.target,read,e):sync::verify_owned_advance_entry(owner,owner.target,read,e);}
};
}
int main(){try{std::string e;for(bool start:{false,true}){
    for(bool negative:{false,true}){Code c(start,negative);need(c.verify(start,e),"exact own physical forward/backward route rejected");++checks;}
    {Code c(start,false);c.owner.enabled=false;need(!c.verify(start,e)&&!c.reads,"unpublished owner accepted");++checks;}
    {Code c(start,false);c.bytes[c.owner.target+15]^=1;need(!c.verify(start,e)&&e.find("suffix")!=std::string::npos,"entry suffix mutation accepted");++checks;}
    {Code c(start,false);c.bytes[c.owner.trampoline+c.copied-1]^=1;need(!c.verify(start,e)&&e.find("continuation")!=std::string::npos,"last copied opcode mutation accepted");++checks;}
    {Code c(start,false);c.put(c.owner.trampoline+c.copied+6,c.owner.target+c.copied+1);need(!c.verify(start,e),"wrong native continuation accepted");++checks;}
    {Code c(start,false);c.put(c.owner.trampoline+c.copied+20,c.owner.detour+1);need(!c.verify(start,e)&&e.find("detour")!=std::string::npos,"foreign detour accepted");++checks;}
    {Code c(start,false);c.unreadable=c.owner.trampoline;need(!c.verify(start,e)&&e=="original code read failed","read failure cause lost");++checks;}
    {Code c(start,false);c.put<int32_t>(c.owner.target+1,0);need(!c.verify(start,e)&&e.find("relay")!=std::string::npos,"foreign entry jump accepted");++checks;}
    {Code c(start,false);c.owner.trampoline=UINTPTR_MAX-12;need(!c.verify(start,e)&&!c.reads&&e.find("overflow")!=std::string::npos,"overflow read executed");++checks;}
}std::cout<<checks<<" actual owned Start/Advance physical checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
