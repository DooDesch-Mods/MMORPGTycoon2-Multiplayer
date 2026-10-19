#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::gm_ticket::storage {
struct Value {uintptr_t data=0;uint64_t extent=0;bool operator==(const Value&)const=default;};
struct Receipt {uint32_t attempted=0,stored=0;bool confirmed=false;};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class Read>bool read(Read& at,uintptr_t image,uintptr_t object,Value& value,std::string& error){
    uintptr_t primary=0,secondary=0;
    if(!at(object,0,primary,error)||!at(object,0x18,secondary,error)||!at(object,8,value.data,error)||!at(object,0x10,value.extent,error))return false;
    if(primary!=image+0x16f0720||secondary!=image+0x16f0788)return fail(error,"GM original Ticket ObjectArray primary/secondary class differs");
    const auto count=int32_t(uint32_t(value.extent)),capacity=int32_t(uint32_t(value.extent>>32));
    if(count<0||count>4096||capacity<count||capacity>65536||(!value.data&&capacity)||value.data>UINTPTR_MAX-size_t(capacity)*8)return fail(error,"GM original Ticket ObjectArray storage extent differs");
    return true;
}
// Both objects are real original Ticket ObjectArrays. Only their owning storage
// moves; the registered current Queue's array object and identity stay intact.
template<class Read,class Exchange,class Guard>bool transfer(Read& at,Exchange& exchange,Guard& guard,uintptr_t image,
    uintptr_t destination,uintptr_t temporary,const Value& old,const Value& fresh,Receipt& receipt,std::string& error){
    receipt={};Value a,b;
    if(destination==temporary)return fail(error,"GM Ticket ObjectArray transfer aliases its temporary owner");
    if(!guard(error)||!read(at,image,destination,a,error)||!read(at,image,temporary,b,error))return false;
    if(a!=old||b!=fresh)return fail(error,"GM Ticket ObjectArray before storage changed");
    const uintptr_t addresses[4]{destination+8,destination+0x10,temporary+8,temporary+0x10};
    const uint64_t expected[4]{old.data,old.extent,fresh.data,fresh.extent},desired[4]{fresh.data,fresh.extent,old.data,old.extent};
    for(unsigned i=0;i<4;++i){bool stored=false;++receipt.attempted;if(!exchange(addresses[i],expected[i],desired[i],stored,error))return false;
        if(!stored)return fail(error,"GM Ticket ObjectArray storage CAS rejected slot="+std::to_string(i));++receipt.stored;}
    if(!guard(error)||!read(at,image,destination,a,error)||!read(at,image,temporary,b,error))return false;
    if(a!=fresh||b!=old)return fail(error,"GM Ticket ObjectArray complete storage readback differs");
    receipt.confirmed=true;error.clear();return true;
}
}
