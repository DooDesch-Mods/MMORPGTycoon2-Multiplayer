#include "subscriber_intervention.h"
#include <iostream>
#include <climits>
#include <map>
#include <stdexcept>
namespace s=mt2mp::game::subscriber_intervention;
namespace {
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Memory {std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n)bytes[address+n]=source[n];}
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){auto* target=static_cast<unsigned char*>(out);for(size_t n=0;n<size;++n){const auto found=bytes.find(address+n);if(found==bytes.end()){error="missing fixture byte";return false;}target[n]=found->second;}return true;}
    template<class T>bool read(uintptr_t object,size_t offset,T& out,std::string& error){return copy(object+offset,&out,sizeof out,error);}
};
#include "subscriber_cheat_state_cases.inc"
void fixture(){constexpr uintptr_t image=0x10000000,manager=0x20000000,data=0x30000000,a=0x40000000,b=0x50000000;Memory memory;memory.put(image+0x13df690,manager);memory.put(manager,image+0x1725f40);memory.put(manager+0x40,data);memory.put(manager+0x48,int32_t(2));memory.put(manager+0x4c,int32_t(2));memory.put(data,std::array<uintptr_t,2>{a,b});
    for(auto [object,id]:{std::pair{a,7},std::pair{b,3}}){memory.put(object,image+0x16e75f0);memory.put(object+0x20,id);memory.put(object+0x80,uintptr_t(0));memory.put(object+0x50,uint32_t(2));memory.put(object+0x70,std::bit_cast<uint32_t>(4.5f));memory.put(object+0xc8,uint8_t(0));memory.put(object+0x125,uint8_t(1));}
    auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& error){return memory.read(object,offset,out,error);};auto copy=[&](uintptr_t object,void* out,size_t size,std::string& error){return memory.copy(object,out,size,error);};
    s::State state; s::Owners owners;std::string error;need(s::read_all(read,copy,image,state,owners,error),error);need(state.size()==2&&state[0].id==3&&state[1].id==7&&owners.rows[0].first==a&&owners.rows[0].second.id==7,"native order was confused with serialized identity");s::State decoded;need(s::decode(s::encode(state),decoded,error)&&decoded==state,error);
    memory.put(b+0x20,int32_t(7));need(!s::read_all(read,copy,image,state,owners,error),"duplicated serialized Subscriber identity accepted");memory.put(b+0x20,int32_t(3));
    memory.put(data+8,a);need(!s::read_all(read,copy,image,state,owners,error),"aliased native Subscriber accepted");memory.put(data+8,b);
    memory.put(a+0x125,uint8_t(2));need(!s::read_all(read,copy,image,state,owners,error),"non-Boolean banned member accepted");memory.put(a+0x125,uint8_t(1));
    memory.put(a+0x70,uint32_t(0x7fc00000));need(!s::read_all(read,copy,image,state,owners,error),"NaN watch timer accepted");memory.put(a+0x70,std::bit_cast<uint32_t>(4.5f));
    memory.put(manager+0x4c,int32_t(1));need(!s::read_all(read,copy,image,state,owners,error),"native count beyond capacity accepted");memory.put(manager+0x4c,int32_t(2));
    need(s::read_all(read,copy,image,state,owners,error),error);const auto wire=s::encode(state);
    for(const auto& value:{nlohmann::json(true),nlohmann::json(-1),nlohmann::json(1.5),nlohmann::json(uint64_t(UINT32_MAX)+1)}){auto bad=wire;bad[0]["warnings"]=value;need(!s::decode(bad,decoded,error),"coerced/overflowing warning count accepted");}
    auto bad=wire;bad[1]["id"]=bad[0]["id"];need(!s::decode(bad,decoded,error),"duplicate serialized rows accepted");bad=wire;bad[0]["banned"]=1;need(!s::decode(bad,decoded,error),"integer Boolean accepted");bad=wire;bad[0]["unknown"]=0;need(!s::decode(bad,decoded,error),"foreign field accepted");bad=wire;bad[0].erase("watch_timer_bits");need(!s::decode(bad,decoded,error),"missing actual watch timer accepted");
    need(s::read_all(read,copy,image,state,owners,error),error);auto desired=state;for(auto& row:desired){++row.value.warnings;row.value.watch_timer_bits=std::bit_cast<uint32_t>(8.25f);row.value.leaving=true;row.value.banned=false;}
    auto sample=[&](s::State& values,s::Owners& current,std::string& cause){return s::read_all(read,copy,image,values,current,cause);};unsigned guards=0,compares=0,fail_at=UINT_MAX;
    auto guard=[&](const s::Owners& current,size_t n,std::string& cause){++guards;const auto& [object,row]=current.rows[n];s::Value actual;if(!s::read(read,image,object,row.id,actual,cause))return false;if(actual!=row.value){cause="current physical value changed";return false;}return true;};
    auto compare=[&](uintptr_t address,unsigned bytes,uint32_t expected,uint32_t value,std::string& cause){const auto call=compares++;if(call==fail_at){cause="injected exact native CAS refusal";return false;}
        if(bytes==1){uint8_t actual=0;if(!memory.copy(address,&actual,1,cause)||actual!=expected){cause="actual byte differs";return false;}memory.put(address,uint8_t(value));}
        else if(bytes==4){uint32_t actual=0;if(!memory.copy(address,&actual,4,cause)||actual!=expected){cause="actual int32 bits differ";return false;}memory.put(address,value);}
        else{cause="unsupported field width";return false;}return true;};
    struct Receipt{bool attempted=false;uint32_t attempted_stores=0,stores=0;}receipt;
    need(s::adopt(desired,sample,guard,compare,receipt,error),error);need(receipt.attempted&&receipt.attempted_stores==8&&receipt.stores==8&&compares==8&&guards==8,"actual four fields per original ID were not adopted exactly once");
    receipt={};guards=compares=0;need(s::adopt(desired,sample,guard,compare,receipt,error)&&!receipt.attempted&&!compares&&!guards,"unchanged authoritative rows repeated native writes");
    auto missing=desired;missing.pop_back();need(!s::adopt(missing,sample,guard,compare,receipt,error)&&!receipt.attempted&&!compares,"missing actual Subscriber lifecycle was silently filled");
    auto partial=desired;for(auto& row:partial){++row.value.warnings;row.value.leaving=false;}fail_at=1;need(!s::adopt(partial,sample,guard,compare,receipt,error),"partial authoritative CAS failure reported success");need(receipt.attempted&&receipt.attempted_stores==2&&receipt.stores==1&&error=="injected exact native CAS refusal","native partial attempt/store cause was lost");
    memory.put(manager+0x48,int32_t(0));memory.put(manager+0x4c,int32_t(0));memory.put(manager+0x40,uintptr_t(0));need(s::read_all(read,copy,image,state,owners,error)&&state.empty()&&owners.rows.empty(),error);
}
}
int main()try{fixture();subscriber_cheat_fixture::run();std::cout<<checks<<" actual Subscriber intervention owner/field/schema assertions passed\n";return 0;}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
