#include "dungeon_config_fixture.h"
#include "dungeon_config_reader.h"
#include <cstring>
#include <iostream>
template<size_t N,class T>void put(std::array<unsigned char,N>& bytes,size_t offset,const T& value){std::memcpy(bytes.data()+offset,&value,sizeof value);}
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(checks));};
    for(auto counts:{std::pair{1,1},std::pair{1,4},std::pair{4,1}})for(bool mode:{false,true})for(int32_t price:{0,100}){previous_count=counts.first;desired_count=counts.second;sandbox=mode;instance_price=price;
        for(const auto& message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Dungeon config complete schema35 roundtrip differs");}
        const auto invalid=[&](auto mutate){auto m=result();mutate(m);check(!encode(m,bytes,error),"Dungeon config foreign effect/domain accepted");};
        invalid([](auto& m){m.request.dungeon_config.desired=0;});invalid([](auto& m){m.request.dungeon_config.desired=11;});invalid([](auto& m){m.request.dungeon_config.before=-1;});invalid([](auto& m){m.request.dungeon_config.key="different";});
        invalid([](auto& m){m.evidence.dungeon_config_after.desired+=1;});invalid([](auto& m){m.evidence.dungeon_config_after.instances[1].index=4;});invalid([](auto& m){m.evidence.dungeon_config_after.instances[0].started=false;});
        invalid([](auto& m){m.evidence.dungeon_config_after.price+=1;});invalid([](auto& m){m.evidence.dungeon_config_after.sandbox=!m.evidence.dungeon_config_before.sandbox;});invalid([](auto& m){m.evidence.dungeon_config_after.undo_count+=1;});
        invalid([](auto& m){m.evidence.after.cash+=1;});invalid([](auto& m){m.evidence.after.record_cash+=1;});invalid([](auto& m){m.evidence.after.record_income+=1;});invalid([](auto& m){m.evidence.after.record_expense+=1;});invalid([](auto& m){m.evidence.after.line_counts[0]-=1;});invalid([](auto& m){m.evidence.after.line_income[1]-=1;});
        invalid([](auto& m){m.request.costume_edit.id=1;});invalid([](auto& m){m.evidence.costume_edit_after.owner.name="foreign";});
        check(dungeon_config_effect(request().request,money(),charged(),before()),"Exact original category0 charge rejected");check(dungeon_config_wire_fits(request().request,before(),money(),error),"Normal Dungeon config packet preflight rejected");
        auto wide=result();wide.evidence.dungeon_config_before.global_random[2]=UINT64_MAX;wide.evidence.dungeon_config_after.global_random[2]=UINT64_MAX;
        check(encode(wide,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==wide,"Dungeon RNG64 word truncated in protocol");
        auto oversized=before();for(int32_t i=3;i<10000;++i)oversized.instances.push_back({i,false});check(!dungeon_config_wire_fits(request().request,oversized,money(),error),"Oversized full instance receipt admitted before effect");
        auto poor=money();poor.cash=-1;auto state=before();state.sandbox=false;check(!dungeon_config_wire_fits(request().request,state,poor,error),"Original insufficient cash admitted");state.sandbox=true;check(dungeon_config_wire_fits(request().request,state,poor,error),"Original Sandbox negative cash rejected");
    }
    using namespace mt2mp::game;constexpr uintptr_t image=0x140000000,root=0x1234;std::array<unsigned char,0xa40> exterior{};std::array<unsigned char,0x700> dungeon{};std::array<unsigned char,0x48> instance{};std::array<unsigned char,0x78> region{};std::array<unsigned char,0x50> shard{};
    const auto b=uintptr_t(exterior.data()),d=uintptr_t(dungeon.data()),i=uintptr_t(instance.data()),r=uintptr_t(region.data()),s=uintptr_t(shard.data());std::array<uintptr_t,1> pointers{i};
    put(exterior,0,image+0x16d93c0);put(exterior,0x18,uint64_t(77));put(exterior,0x170,root);put(exterior,0xa38,d);put(dungeon,0,image+0x16c0560);put(dungeon,0x6f8,b);put(dungeon,0x40c,int32_t(3));put(dungeon,0x4c0,uintptr_t(pointers.data()));put(dungeon,0x4c8,int32_t(1));put(dungeon,0x4cc,int32_t(1));
    put(instance,0,image+0x1719450);put(instance,0x18,int32_t(7));put(instance,0x20,uint8_t(1));put(instance,0x30,d);put(instance,0x38,s);put(instance,0x40,r);put(region,0,image+0x1787930);put(region,0x70,s);put(shard,0x20,i);
    auto read=[](uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;};DungeonConfigState state;dungeon_config_reader::Local local;
    check(dungeon_config_reader::read(read,image,root,b,77,state,local,error)&&state.instances==std::vector<DungeonInstanceRecord>{{7,true}}&&local.instances[0][0]==i,"Dungeon saved-index census read differs");
    check(!dungeon_config_reader::read(read,image,root,b,78,state,local,error),"Wrong exterior full UID admitted");put(instance,0x20,uint8_t(2));check(!dungeon_config_reader::read(read,image,root,b,77,state,local,error),"Invalid native started flag admitted");put(instance,0x20,uint8_t(1));
    put(shard,0x20,d);check(!dungeon_config_reader::read(read,image,root,b,77,state,local,error),"Foreign instance Shard reciprocal owner admitted");
    std::cout<<checks<<" Dungeon config count/cash/category0/instance/Undo/protocol checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
