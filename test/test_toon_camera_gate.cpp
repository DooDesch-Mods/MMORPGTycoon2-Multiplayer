#include "toon_time_fence/camera_gate_scope.h"
#include "toon_time_fence/gate_relocation.h"
#include "toon_time_fence/npc_camera_owner.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace mt2mp::game::toon_time_fence::camera_gate;
extern "C" {
void gate_test_original();void gate_test_deferred();void gate_test_call(uintptr_t,uint64_t);void gate_test_travel_call(uintptr_t,uint64_t);void gate_test_attribute_call(uintptr_t,uint64_t);void gate_test_npc_call(uintptr_t,uint64_t);
uintptr_t mt2mp_toon_gate_original=reinterpret_cast<uintptr_t>(&gate_test_original);
uintptr_t mt2mp_toon_gate_full=reinterpret_cast<uintptr_t>(&gate_test_deferred);
uintptr_t mt2mp_toon_travel_original=reinterpret_cast<uintptr_t>(&gate_test_original);
uintptr_t mt2mp_toon_travel_rotation=reinterpret_cast<uintptr_t>(&gate_test_deferred);
uintptr_t mt2mp_toon_attribute_original=reinterpret_cast<uintptr_t>(&gate_test_original);
uintptr_t mt2mp_toon_attribute_batched=reinterpret_cast<uintptr_t>(&gate_test_deferred);
uintptr_t mt2mp_npc_gate_original=reinterpret_cast<uintptr_t>(&gate_test_original);
uintptr_t mt2mp_npc_gate_full=reinterpret_cast<uintptr_t>(&gate_test_deferred);
uint64_t gate_route=0,gate_flags_before=0,gate_flags_after=0,gate_registers[7]{};
std::array<uint64_t,32> gate_xmm{};
int mt2mp_toon_gate_selected(uintptr_t receiver) noexcept{return selected(receiver)?1:0;}
}
int main(){
    unsigned checks=0;
    const auto need=[&](bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);};
    const auto call=[&](uintptr_t receiver,uint64_t expected){
      for(const auto invoke:{gate_test_call,gate_test_travel_call,gate_test_attribute_call,gate_test_npc_call})for(uint64_t zero:{0ULL,1ULL}){
        gate_route=0;invoke(receiver,zero);
        need(gate_route==expected,"wrong native continuation");
        need((gate_flags_before&0x8d5)==(gate_flags_after&0x8d5),"condition flags changed");
        for(size_t i=0;i<7;++i)need(gate_registers[i]==0x101+i,"volatile register changed");
        for(size_t i=0;i<16;++i)need(gate_xmm[i*2]==0x0123456789abcdefULL&&gate_xmm[i*2+1]==0x0fedcba987654321ULL,"XMM changed");
      }
    };
    try{
        constexpr uintptr_t image=0x10000000;
        {
            std::array<unsigned char,0x900> npc{};std::array<unsigned char,0xa00> region{};std::array<unsigned char,0x100> manager{};
            const uintptr_t npc_ptr=uintptr_t(npc.data());uintptr_t member=npc_ptr;
            const auto put=[](auto& object,size_t offset,const auto& value){std::memcpy(object.data()+offset,&value,sizeof value);};
            uintptr_t ap=image+0x1742980,root=0x13570000,region_ptr=uintptr_t(region.data()),manager_ptr=uintptr_t(manager.data()),data=uintptr_t(&member);
            int32_t kind=1,mode=2,count=1;
            put(npc,0,ap);put(npc,0x170,root);put(npc,0x178,region_ptr);put(npc,0x80c,kind);put(npc,0x838,mode);
            put(region,0x998,manager_ptr);put(manager,0xc8,region_ptr);put(manager,0x70,data);put(manager,0x78,count);put(manager,0x7c,count);
            const auto read=[](uintptr_t object,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<void*>(object+offset),sizeof value);return true;};
            bool admitted=false;std::string error;
            const auto check=[&](uintptr_t caller){return mt2mp::game::toon_time_fence::npc_camera_owner::admit(read,image,root,npc_ptr,caller,admitted,error);};
            need(check(image+0x297a34)&&admitted,"actual NPC owner rejected");
            need(check(image+1)&&!admitted,"wrong NPC caller admitted");
            mode=1;put(npc,0x838,mode);need(check(image+0x297a34)&&!admitted,"wrong NPC scheduling mode admitted");
            mode=2;put(npc,0x838,mode);auto other=root+1;put(npc,0x170,other);need(!check(image+0x297a34),"foreign NPC root admitted");
            put(npc,0x170,root);member=0;need(!check(image+0x297a34),"missing NPC member admitted");
        }
        need(image+full_update_rva==image+0x3ea480,"ordered camera gate does not select original full update");
        std::array<unsigned char,30> camera_before{0x80,0xbb,0xc0,0x01,0,0,0};
        std::array<unsigned char,64> camera_code{},travel_code{},attribute_code{},npc_code{};
        std::copy_n(camera_before.begin(),7,camera_code.begin());
        const auto jump=[](auto& code,size_t at,uintptr_t target){code[at]=0xff;code[at+1]=0x25;std::memcpy(code.data()+at+6,&target,sizeof target);};
        jump(camera_code,7,image+0x3ea469);travel_code[0]=0x75;travel_code[1]=0x0e;
        jump(travel_code,2,image+0x3b8140);jump(travel_code,16,image+0x3b7b3d);
        attribute_code[0]=0x74;attribute_code[1]=0x0e;
        jump(attribute_code,2,image+0x3a1480);jump(attribute_code,16,image+0x3a0948);
        npc_code[0]=0x80;npc_code[1]=0x38;npc_code[2]=0;npc_code[3]=0x74;npc_code[4]=0x0e;
        jump(npc_code,5,image+0x3c8e78);jump(npc_code,19,image+0x3c8dc0);jump(npc_code,33,1234);
        need(camera_relocation(camera_before,camera_code,image)&&travel_relocation(travel_code,image)&&attribute_relocation(attribute_code,image)&&npc_relocation(npc_code,image,1234),"valid relocations rejected");
        for(size_t offset:{size_t(7),size_t(8),size_t(9),size_t(10),size_t(21),size_t(22),size_t(23),size_t(24),size_t(35),size_t(36),size_t(37),size_t(38)}){
            auto changed=npc_code;changed[offset]=1;need(!npc_relocation(changed,image,1234),"NPC RIP displacement accepted");
        }
        for(size_t offset:{size_t(11),size_t(25),size_t(39)}){auto changed=npc_code;changed[offset]^=1;need(!npc_relocation(changed,image,1234),"NPC destination accepted");}
        for(size_t offset:{size_t(9),size_t(10),size_t(11),size_t(12)}){
            auto changed=camera_code;changed[offset]=1;need(!camera_relocation(camera_before,changed,image),"camera RIP displacement accepted");
        }
        for(size_t offset:{size_t(4),size_t(5),size_t(6),size_t(7),size_t(18),size_t(19),size_t(20),size_t(21)}){
            auto changed=travel_code;changed[offset]=1;need(!travel_relocation(changed,image),"travel RIP displacement accepted");
            changed=attribute_code;changed[offset]=1;need(!attribute_relocation(changed,image),"attribute RIP displacement accepted");
        }
        {auto changed=travel_code;changed[8]^=1;need(!travel_relocation(changed,image),"changed branch destination accepted");}
        {auto changed=travel_code;changed[22]^=1;need(!travel_relocation(changed,image),"changed fallthrough destination accepted");}
        {auto changed=attribute_code;changed[8]^=1;need(!attribute_relocation(changed,image),"changed attribute branch destination accepted");}
        {auto changed=attribute_code;changed[22]^=1;need(!attribute_relocation(changed,image),"changed attribute fallthrough destination accepted");}
        call(7,1);
        {
            Step step(10);need(step.admitted(),"valid step rejected");
            call(7,1);
            {
                Entry owner(7);call(7,2);call(8,1);
                {Entry nested(8);call(8,2);call(7,1);}
                call(7,2);
                {Step nested(11);need(!nested.admitted(),"nested step accepted");}
                call(7,2);
            }
            call(7,1);
        }
        call(7,1);
        std::cout<<checks<<" native camera gate register, flag, scope and continuation checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
