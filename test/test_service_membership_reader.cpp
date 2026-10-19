#include "service_membership_fixture.h"
#include <iostream>
using namespace service_fixture;
int main(){try{unsigned cases=0;std::string e;a::ServiceMembershipState value;reader::Local local;auto m=memory();
    need(read(m,value,local,e),e);need(value.rows.size()==3&&value.rows[2].character==0&&local.rows[0].uid==0x200000001,"full service value/local binding");++cases;
    const auto original=value;const auto original_local=local;
    for(const int32_t satisfaction:{0,100}){
        for(const auto& row:local.rows)m.put(row.object+0x140,satisfaction);
        need(read(m,value,local,e),e);
        need(value==original&&local==original_local,"local UI satisfaction cache entered shared Service membership authority");++cases;
        for(const auto& row:value.rows){need(row.index==-1,"legacy index slot exported a UI cache value");++cases;}
    }
    const a::ServiceMembershipIntent remove{a::ServiceMembershipKind::Remove,-1,1};
    a::ServiceMembershipState expected_removed;need(a::service_membership_expected(remove,original,expected_removed),"remove expectation");
    mutate(m,remove,value,local);reader::Local removed_local;
    need(read(m,value,removed_local,e)&&value==expected_removed&&reader::transition(remove,original,original_local,removed_local),"UI-open Client service Remove differs from unopened Host authority");++cases;
    auto bad=[&](auto change){auto copy=memory();change(copy);need(!read(copy,value,local,e),"bad Service reader accepted");need(!e.empty(),"missing cause");++cases;};
    bad([](auto& f){f.put(service+0x148,uintptr_t(0));});bad([](auto& f){f.put(data+8,service);});
    bad([](auto& f){f.put(service+0x18,uint64_t(0));});bad([](auto& f){f.put(service+0x1000+0x18,uint64_t(0x200000001));});
    bad([](auto& f){f.put(service+0x138,uintptr_t(0xdead));});bad([](auto& f){f.put(service+0x150,uintptr_t(0xdead));});
    bad([](auto& f){f.put(service+0x178,uintptr_t(0xdead));});bad([](auto& f){f.put(service+0x48,std::array<uint8_t,3>{0,2,0});});
    bad([](auto& f){f.put(character+0x2c,int32_t(4));});bad([](auto& f){f.put(character+0x30,int32_t(2));});
    bad([](auto& f){f.put(service+0x98,std::array<int32_t,8>{11,10,0,0,0,10,0,0});});
    bad([](auto& f){f.put(service+0x130,uintptr_t(0xdead));});
    for(auto intent:{a::ServiceMembershipIntent{a::ServiceMembershipKind::AddSpawn,-1,-1},a::ServiceMembershipIntent{a::ServiceMembershipKind::AddSpawn,0,-1},a::ServiceMembershipIntent{a::ServiceMembershipKind::Remove,-1,1}}){
        m=memory();need(read(m,value,local,e),e);auto before=value;auto pins=local;mutate(m,intent,before,pins);need(read(m,value,local,e),e);
        a::ServiceMembershipState expected;need(a::service_membership_expected(intent,before,expected)&&expected==value,"original replacement/delete value");
        need(reader::transition(intent,before,pins,local),"actual native local survivor ownership");++cases;
        auto changed=local;changed.rows[0].uid^=uint64_t(1)<<32;need(!reader::transition(intent,before,pins,changed),"high UID survivor changed");++cases;
        changed=local;changed.character_uids[0]++;need(!reader::transition(intent,before,pins,changed),"Character owner changed");++cases;
        changed=local;changed.rows[0].daily[0][7]++;need(!reader::transition(intent,before,pins,changed),"survivor DailyCount revision changed");++cases;
    }
    std::cout<<cases<<" actual Service membership reader/ownership cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
