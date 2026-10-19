#include "gm_policy_native.h"
#include <map>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace gp=mt2mp::game::gm_policy;
namespace {
unsigned checks=0,notifications=0;std::string property;
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
void notify(void*,const void* text){struct String{const char* data;size_t length;};const auto& value=*static_cast<const String*>(text);property.assign(value.data,value.length);++notifications;}
struct Memory {std::map<uintptr_t,uint8_t> data;template<class T>void put(uintptr_t p,const T& value){const auto* bytes=reinterpret_cast<const uint8_t*>(&value);for(size_t n=0;n<sizeof value;++n)data[p+n]=bytes[n];}
    template<class T>bool read(uintptr_t p,size_t offset,T& value,std::string& error){auto* bytes=reinterpret_cast<uint8_t*>(&value);for(size_t n=0;n<sizeof value;++n){auto it=data.find(p+offset+n);if(it==data.end()){error="missing native byte";return false;}bytes[n]=it->second;}return true;}};
void domain(){gp::State state;need(gp::valid(state),"empty native policies invalid");
    for(size_t p=0;p<2;++p)for(size_t f=0;f<7;++f){gp::Intent intent{uint8_t(p),gp::Field(f),0,gp::maximum(gp::Field(f))};need(gp::valid(intent),"native policy limit rejected");
        gp::Proof before{state,std::string(64,'a')},after=before;after.state.policies[p][f]=intent.value;need(gp::effect(intent,before,after),"exact original property change rejected");
        after.state.policies[1-p][0]=1;need(!gp::effect(intent,before,after),"other policy drift accepted");
        ++intent.value;need(!gp::valid(intent),"value beyond original UI limit accepted");}
    need(!gp::valid(gp::Intent{}),"no-change intent accepted");gp::Intent invalid{2,gp::Field::Enabled,0,1};need(!gp::valid(invalid),"unknown native policy accepted");
    auto wire=gp::encode(state);need(gp::codec::state(wire)==state,"policy state roundtrip");
    for(const auto& malformed:{nlohmann::json::array(),nlohmann::json::array({wire[0]}),nlohmann::json::array({wire[0],nlohmann::json::array()})}){bool rejected=false;try{(void)gp::codec::state(malformed);}catch(const std::exception&){rejected=true;}need(rejected,"malformed full policy extent accepted");}
    for(const auto& malformed:{nlohmann::json(true),nlohmann::json(1.5),nlohmann::json(-1),nlohmann::json(uint64_t(INT32_MAX)+1)}){auto bad=wire;bad[0][0]=malformed;bool rejected=false;try{(void)gp::codec::state(bad);}catch(const std::exception&){rejected=true;}need(rejected,"coerced policy scalar accepted");}
}
void native_reader(){constexpr uintptr_t image=0x10000000,queue=0x20000000;Memory memory;memory.put(image+0x13df360,queue);memory.put(queue,image+0x16fdc60);
    for(size_t p=0;p<2;++p){const auto owner=queue+0x48+p*40;memory.put(owner,image+0x16dab90);for(size_t f=0;f<7;++f){if(gp::integer(gp::Field(f)))memory.put(owner+gp::offsets[f],int32_t(0));else memory.put(owner+gp::offsets[f],uint8_t(0));}}
    auto read=[&](uintptr_t p,size_t o,auto& value,std::string& error){return memory.read(p,o,value,error);};gp::State out;std::string error;need(gp::read(read,image,queue,out,error),error);
    memory.put(image+0x13df360,queue+8);need(!gp::read(read,image,queue,out,error),"stale Queue owner accepted");memory.put(image+0x13df360,queue);
    memory.put(queue+0x70,image+0x16dab98);need(!gp::read(read,image,queue,out,error),"wrong embedded policy class accepted");memory.put(queue+0x70,image+0x16dab90);
    memory.put(queue+0x48+gp::offsets[0],uint8_t(2));need(!gp::read(read,image,queue,out,error),"non-Boolean native policy accepted");memory.put(queue+0x48+gp::offsets[0],uint8_t(0));
    memory.put(queue+0x70+gp::offsets[1],int32_t(3601));need(!gp::read(read,image,queue,out,error),"native integer overflowed property domain");
}
void native_store(){alignas(8) std::array<uint8_t,40> owner{};const uintptr_t image=uintptr_t(&notify)-0x3af510;
    for(size_t field=0;field<7;++field){gp::Intent intent{0,gp::Field(field),0,gp::maximum(gp::Field(field))};const auto count=notifications;const auto receipt=gp::native::change(image,uintptr_t(owner.data()),intent);
        need(receipt.stored&&receipt.notified&&!receipt.exception&&notifications==count+1&&property==gp::names[field],"original policy property notification not exactly once");
        const auto duplicate=gp::native::change(image,uintptr_t(owner.data()),intent);need(!duplicate.stored&&!duplicate.notified&&!duplicate.exception&&notifications==count+1,"stale before CAS notified again");}
    auto receipt=gp::native::change(image,uintptr_t(owner.data()),{});need(receipt.exception&&!receipt.stored&&!receipt.notified,"invalid no-change native operation executed");
}
}
int main()try{domain();native_reader();native_store();std::cout<<checks<<" GM policy domain/current owner/actual member store assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
