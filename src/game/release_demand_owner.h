#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::release_demand_owner {
struct Storage {uintptr_t data=0;uint64_t extent=0;bool operator==(const Storage&)const=default;};
struct Receipt {uint32_t attempted=0,stored=0;bool confirmed=false;};
template<class Read>bool read(Read& at,uintptr_t image,uintptr_t object,bool temporary,Storage& value,std::string& e){
    uintptr_t ap=0,secondary=0;
    if(!at(object,0,ap,e)||ap!=image+(temporary?0x16ebf20:0x16e2100)||!at(object,8,value.data,e)||!at(object,0x10,value.extent,e)){if(e.empty())e="Release prepared demand owner class/storage differs";return false;}
    if(temporary&&(!at(object,0x18,secondary,e)||secondary!=image+0x16ebf88)){if(e.empty())e="Release temporary demand owner secondary class differs";return false;}
    const auto count=int32_t(uint32_t(value.extent)),capacity=int32_t(uint32_t(value.extent>>32));
    if(count<0||count>4096||capacity<count||capacity>65536||(!value.data&&capacity)||value.data>UINTPTR_MAX-size_t(capacity)*8){e="Release prepared demand storage bounds invalid";return false;}return true;
}
// Only storage moves. The manager embeds a 24-byte ArrayStore, while the
// temporary original ObjectArray also owns its separate NullObject base.
template<class Read,class Exchange,class Guard>bool transfer(Read& at,Exchange& exchange,Guard& guard,uintptr_t image,uintptr_t destination,uintptr_t temporary,const Storage& old,const Storage& fresh,Receipt& receipt,std::string& e){
    receipt={};Storage a,b;if(destination==temporary||!guard(e)||!read(at,image,destination,false,a,e)||!read(at,image,temporary,true,b,e)||a!=old||b!=fresh){if(e.empty())e="Release prepared demand transfer owners changed before effect";return false;}
    const uintptr_t addresses[4]{destination+8,destination+0x10,temporary+8,temporary+0x10};
    const uint64_t expected[4]{old.data,old.extent,fresh.data,fresh.extent},desired[4]{fresh.data,fresh.extent,old.data,old.extent};
    for(unsigned i=0;i<4;++i){bool stored=false;++receipt.attempted;if(!exchange(addresses[i],expected[i],desired[i],stored,e)||!stored){if(e.empty())e="Release prepared demand storage CAS rejected slot="+std::to_string(i);return false;}++receipt.stored;}
    if(!guard(e)||!read(at,image,destination,false,a,e)||!read(at,image,temporary,true,b,e)||a!=fresh||b!=old){if(e.empty())e="Release prepared demand transfer actual readback differs";return false;}receipt.confirmed=true;e.clear();return true;
}
}
