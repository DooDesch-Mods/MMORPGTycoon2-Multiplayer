#include "tutorial_observer.h"
#include "tutorial_replication/owned_sequence_entry.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace observer=mt2mp::game::tutorial_observer::detail;
using json=nlohmann::json;
namespace fixture {
constexpr uintptr_t image=0x10000000,state=0x20000000,scenario_log=0x20001000,active=0x20002000,
    scenario=0x20003000,sequence=0x20004000,steps=0x20005000,first=0x20006000,second=0x20007000,
    history=0x20008000,completed=0x20009000;
struct Fake:observer::Port {
    std::map<uintptr_t,std::vector<unsigned char>> memory;
    observer::Origin origin{0x2000a000,state,0x2000b000,0x2000c000,7,9,"50000000-0000-4000-8000-000000000001"};
    int origins=0;bool wrong_thread=false,retire=false;uintptr_t unreadable=0,change=0;int change_reads=0;
    void allocate(uintptr_t address,size_t count){memory[address].resize(count);}
    unsigned char* pointer(uintptr_t address,size_t count){
        auto found=memory.upper_bound(address);if(found==memory.begin())return nullptr;--found;
        const auto offset=address-found->first;
        return offset<=found->second.size()&&count<=found->second.size()-size_t(offset)?found->second.data()+offset:nullptr;
    }
    template<class T>void put(uintptr_t address,T value){auto* data=pointer(address,sizeof(value));if(!data)throw std::runtime_error("fixture write outside allocated memory");std::memcpy(data,&value,sizeof(value));}
    void text(uintptr_t owner,size_t offset,const std::string& value){const auto address=owner+0x280+offset;allocate(address,value.size()+1);std::memcpy(pointer(address,value.size()+1),value.c_str(),value.size()+1);put(owner+offset,address);put(owner+offset+8,uint64_t(value.size()));}
    void array(uintptr_t owner,size_t offset,uintptr_t data,int32_t count,int32_t capacity){put(owner+offset,data);put(owner+offset+8,count);put(owner+offset+12,capacity);}
    bool current(observer::Origin& out,std::string& error)override{
        ++origins;if(wrong_thread){error="current_world_read_wrong_thread";return false;}
        out=origin;if(retire&&origins==2)++out.generation;return true;
    }
    bool image(uintptr_t& out,std::string&)override{out=::fixture::image;return true;}
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)override{
        if(change==address&&++change_reads==2){auto* data=pointer(address,count);if(data)*data^=1;}
        auto* data=pointer(address,count);
        if(address==unreadable||!data){error="fixture native memory unreadable";return false;}
        std::memcpy(out,data,count);return true;
    }
    mt2mp::game::tutorial_sync::OwnedSequenceEntry owned;
    bool owned_sequence_entry(uintptr_t address,std::string& error)override{
        if(!owned.enabled){error="tutorial Sequence Update code guard differs";return false;}
        return mt2mp::game::tutorial_sync::verify_owned_sequence_entry(owned,address,[this](auto p,auto out,auto count,auto& e){return copy(p,out,count,e);},error);
    }
    void install_owned_hook(){
        constexpr uintptr_t trampoline=0x30000000,detour=0x30001000;
        owned={::fixture::image+0x4fc4a0,detour,trampoline,true,true};allocate(trampoline,34);
        std::memcpy(pointer(trampoline,6),mt2mp::game::tutorial_sync::sequence_entry.data(),6);
        const unsigned char jump[]={0xff,0x25,0,0,0,0};
        std::memcpy(pointer(trampoline+6,6),jump,6);put(trampoline+12,owned.target+6);
        std::memcpy(pointer(trampoline+20,6),jump,6);put(trampoline+26,detour);
        put(owned.target,uint8_t(0xe9));put(owned.target+1,int32_t(trampoline+20-owned.target-5));
    }
    Fake(){
        allocate(::fixture::image+0x4f5dc0,16);allocate(::fixture::image+0x4fc4a0,16);allocate(::fixture::image+0x13df2a0,8);allocate(::fixture::image+0x13df330,8);
        const unsigned char instance_entry[]={0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x38,0x48,0x83,0x79,0x58};
        const unsigned char sequence_entry[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa8};
        std::memcpy(pointer(::fixture::image+0x4f5dc0,16),instance_entry,16);std::memcpy(pointer(::fixture::image+0x4fc4a0,16),sequence_entry,16);
        for(auto owner:{state,scenario_log,scenario,sequence,first,second})allocate(owner,0x200);
        allocate(active,32);allocate(steps,16);allocate(history,128);allocate(completed,128);
        put(::fixture::image+0x13df2a0,state);put(::fixture::image+0x13df330,scenario_log);put(state+0x1e8,scenario_log);
        put(scenario_log,::fixture::image+0x16fd7b0);put(scenario_log+0x18,::fixture::image+0x16ed6c0);array(scenario_log,0x20,active,1,4);array(scenario_log,0x50,completed,0,4);
        put(active,scenario);put(scenario,::fixture::image+0x173ce70);text(scenario,0x18,"initial_tutorial");array(scenario,0x48,history,0,4);put(scenario+0x58,sequence);
        put(sequence,::fixture::image+0x16dbd80);put(sequence+0x18,::fixture::image+0x16ef9a0);array(sequence,0x20,steps,2,2);
        put(steps,first);put(steps+8,second);put(sequence+0x48,first);put(sequence+0x50,int32_t(1));
        put(first,::fixture::image+0x16fda00);text(first,0x20,"welcome");put(second,::fixture::image+0x16fda00);text(second,0x20,"wasd");
    }
};
int cases=0;
void check(bool value,const std::string& reason){if(!value)throw std::runtime_error(reason);}
json accepted(Fake& fake){json result;std::string error="stale";check(observer::observe(fake,result,error),"observer rejected: "+error);check(error.empty(),"success retained failure cause");++cases;return result;}
void rejected(const std::function<void(Fake&)>& configure,const char* required){Fake fake;configure(fake);json result={{"stale",true}};std::string error;check(!observer::observe(fake,result,error),"invalid observer succeeded");check(result.is_null(),"failure exposed partial/stale receipt");check(error.find(required)!=std::string::npos,"failure cause differs: "+error);++cases;}
}
int main()try{
    using namespace fixture;
    Fake welcome;const auto receipt=accepted(welcome);const auto& row=receipt.at("tutorials").at(0);
    check(receipt.at("state")=="active"&&receipt.at("mainthread_confirmed")==true&&receipt.at("lifetime_confirmed")==true,"missing confirmed read guards");
    check(row.at("scenario_id")=="initial_tutorial"&&row.at("current_step_index")==0&&row.at("current_step_type")=="mmoStepMessage"&&row.at("current_step_id")=="welcome","welcome scalar receipt differs");
    check(row.at("history_count")==0&&row.at("step_count")==2&&!row.at("complete").get<bool>(),"welcome counts differ");
    check(receipt.dump().find("pointer")==std::string::npos&&receipt.dump().find("address")==std::string::npos&&receipt.dump().find("vtable")==std::string::npos,"receipt exports process addresses");
    Fake hooked;hooked.install_owned_hook();accepted(hooked);
    rejected([](Fake& f){f.put(image+0x4fc4a0,uint8_t(0xe9));},"code guard differs");
    rejected([](Fake& f){f.install_owned_hook();f.put(f.owned.trampoline+26,uintptr_t(5));},"detour");
    rejected([](Fake& f){f.install_owned_hook();f.change=image+0x4fc4a0;},"patch/suffix");
    Fake absent;absent.put(scenario_log+0x28,int32_t(0));auto none=accepted(absent);check(none.at("state")=="inactive"&&none.at("tutorials").empty(),"absent tutorial was not normal inactive state");
    Fake done;done.put(scenario_log+0x28,int32_t(0));done.put(scenario_log+0x58,int32_t(1));done.text(completed,0,"initial_tutorial");auto completed_receipt=accepted(done);check(completed_receipt.at("initial_tutorial_complete")==true&&completed_receipt.at("state")=="inactive","completed tutorial scalar differs");
    Fake zone;zone.text(first,0x20,"zone_2");zone.text(second,0x20,"zone_2");const auto zone_before=accepted(zone);zone.origins=0;zone.put(sequence+0x48,second);zone.put(sequence+0x50,int32_t(2));const auto zone_after=accepted(zone);
    check(zone_before.at("tutorials").at(0).at("current_step_id")==zone_after.at("tutorials").at(0).at("current_step_id")&&zone_before.at("tutorials").at(0).at("current_step_index")==0&&zone_after.at("tutorials").at(0).at("current_step_index")==1,"duplicate zone IDs did not retain distinct indexed steps");
    rejected([](Fake& f){f.wrong_thread=true;},"current_world_read_wrong_thread");
    rejected([](Fake& f){f.retire=true;},"origin changed");
    rejected([](Fake& f){f.put(image+0x13df2a0,uintptr_t(5));},"GameState singleton differs");
    rejected([](Fake& f){f.put(state+0x1e8,uintptr_t(5));},"ScenarioLog owner differs");
    rejected([](Fake& f){f.put(scenario_log,uintptr_t(5));},"vtable differs");
    rejected([](Fake& f){f.put(scenario_log+0x28,int32_t(-1));},"count=-1");
    rejected([](Fake& f){f.put(scenario_log+0x28,int32_t(65));},"count=65");
    rejected([](Fake& f){f.put(scenario_log+0x2c,int32_t(0));},"capacity=0");
    rejected([](Fake& f){f.put(scenario,uintptr_t(5));},"scenario native type unsupported");
    rejected([](Fake& f){f.put(sequence+0x48,second);},"current pointer differs");
    rejected([](Fake& f){f.put(sequence+0x50,int32_t(3));},"cursor/completion");
    rejected([](Fake& f){f.put(sequence+0x50,int32_t(0));},"no indexed native owner");
    rejected([](Fake& f){f.put(steps+8,first);},"more than once");
    rejected([](Fake& f){f.put(first,uintptr_t(5));},"step vtable/complete");
    rejected([](Fake& f){f.put(scenario+0x50,int32_t(513));},"count=513");
    rejected([](Fake& f){f.put(image+0x4f5dc0,uint8_t(0));},"code guard differs");
    rejected([](Fake& f){f.unreadable=sequence+0x48;},"fixture native memory unreadable");
    rejected([](Fake& f){f.change=sequence+0x50;},"changed during read");
    rejected([](Fake& f){f.put(first+0x28,uint64_t(201));},"bytes=201");
    rejected([](Fake& f){f.text(first,0x20,std::string(1,char(-1)));},"not valid UTF-8");
    std::cout<<"tutorial observer cases="<<cases<<" passed; read-only fixtures, no game calls\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
