#include "dungeon_instance_live.h"
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace live=mt2mp::game::dungeon_instance_live;
using Scene=mt2mp::game::dungeon_scene_key::Scene;
constexpr uintptr_t fixture_image=0x140000000,fixture_instance=0x20000000,fixture_dungeon=0x21000000;
constexpr uintptr_t fixture_shard=0x22000000,fixture_region=0x23000000,fixture_party=0x24000000;
const std::string fixture_uuid="12345678-1234-1234-1234-123456789abc";
const std::string fixture_key="action:"+fixture_uuid;
const std::string fixture_party_key="action:"+fixture_uuid+":party:0";
unsigned comparisons=0;
void check(bool value,const char* cause){++comparisons;if(!value)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;
    void put_bytes(uintptr_t p,const void* value,size_t count){const auto* source=static_cast<const uint8_t*>(value);for(size_t i=0;i<count;++i)bytes[p+i]=source[i];}
    template<class T>void put(size_t offset,const T& value){put_bytes(fixture_instance+offset,&value,sizeof value);}
    template<class T>bool operator()(uintptr_t p,size_t offset,T& value,std::string& error){
        auto* target=reinterpret_cast<uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i){const auto found=bytes.find(p+offset+i);if(found==bytes.end()){error="fixture native field absent";return false;}target[i]=found->second;}return true;
    }
};
struct Fixture {
    Memory memory;Scene scene;std::string error;unsigned stores=0,reserves=0;bool fail_store=false,fail_reserve=false;
    Fixture(){
        scene.type=2;scene.key=fixture_key+"/instance/37";scene.exterior_key=fixture_key;scene.instance_index=37;
        scene.instance=fixture_instance;scene.dungeon=fixture_dungeon;scene.shard=fixture_shard;scene.region=fixture_region;
        memory.put(0,uintptr_t(fixture_image+0x1719450));memory.put(0x18,int32_t(37));memory.put(0x30,fixture_dungeon);
        memory.put(0x38,fixture_shard);memory.put(0x40,fixture_region);memory.put(0x68,uintptr_t(0));
        memory.put(0x1c,0.25f);memory.put(0x24,-0.125f);memory.put(0x70,2.5f);
        for(const auto offset:{0x20,0x28,0x74,0x75})memory.put(size_t(offset),uint8_t(0));
    }
    bool take(live::Local& out){auto identify=[](uintptr_t p,std::string& key,std::string& e){if(p!=fixture_party){e="foreign Party";return false;}key=fixture_party_key;return true;};return live::read(memory,identify,fixture_image,scene,out,error);}
    bool apply(const live::Local& before,const live::State& target,live::Receipt& receipt){
        auto resolve=[](const std::string& key,uintptr_t& p,std::string& e){if(key!=fixture_party_key){e="unbound Party";return false;}p=fixture_party;return true;};
        auto store=[&](const live::Local&,size_t offset,const void* old_value,const void* new_value,size_t bytes,std::string& e){
            ++stores;if(fail_store){e="injected scalar store failure";return false;}
            const auto* expected=static_cast<const uint8_t*>(old_value);for(size_t i=0;i<bytes;++i)if(memory.bytes.at(fixture_instance+offset+i)!=expected[i]){e="actual expected scalar differs";return false;}
            memory.put_bytes(fixture_instance+offset,new_value,bytes);return true;};
        auto reserve=[&](const live::Local&,uintptr_t p,std::string& e){++reserves;if(fail_reserve){e="injected original Reserve failure";return false;}memory.put(0x68,p);return true;};
        auto readback=[&](live::Local& value,std::string& e){const bool ok=take(value);if(!ok)e=error;return ok;};
        return live::adopt(before,target,resolve,store,reserve,readback,receipt,error);
    }
};
int main()try{
    Fixture fixture;live::Local before;check(fixture.take(before),"current original saved values read");
    check(before.state.index==37&&before.state.timeout==-0.125f,"native index and crossed-zero timeout preserved");
    check(before.state.scene==fixture.scene.key&&before.state.party.empty(),"canonical scene and null Party");
    const auto target_base=before.state;auto target=target_base;target.online_fraction=1;target.timeout=17.25f;target.completion=18;
    target.wants_online=true;target.timing_out=true;target.used=true;target.completed=true;target.party=fixture_party_key;
    live::Receipt receipt;check(fixture.apply(before,target,receipt),"complete saved-value adoption");
    check(receipt.confirmed&&receipt.attempted&&!receipt.unknown&&receipt.stores==7&&receipt.party_calls==1,"actual scalar and original Party receipts");
    live::Local after;check(fixture.take(after)&&after.state==target&&after.party==fixture_party,"native complete readback and local Party binding");
    check(after.state.party==fixture_party_key&&after.state.party.find("24000000")==std::string::npos,"portable Party key has no native pointer");
    check(fixture.apply(after,target,receipt)&&receipt.confirmed&&!receipt.attempted&&receipt.stores==0&&receipt.party_calls==0,"unchanged current values do no native work");
    auto changed=target;changed.index=38;changed.scene=fixture_key+"/instance/38";const auto stores_before=fixture.stores;
    check(!fixture.apply(after,changed,receipt)&&!receipt.attempted&&fixture.stores==stores_before,"replacement requires original lifecycle");
    changed=target;changed.party="action:aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:party:0";
    check(!fixture.apply(after,changed,receipt)&&!receipt.attempted&&fixture.stores==stores_before,"foreign Party rejected before store");
    for(const auto offset:{0x20,0x28,0x74,0x75}){Fixture bad;bad.memory.put(size_t(offset),uint8_t(2));live::Local value;check(!bad.take(value),"native Boolean2 rejected");}
    for(const auto offset:{0x30,0x38,0x40}){Fixture bad;bad.memory.put(size_t(offset),uintptr_t(0x123));live::Local value;check(!bad.take(value),"foreign current scene owner rejected");}
    {Fixture bad;bad.memory.put(0,uintptr_t(0));live::Local value;check(!bad.take(value),"wrong Instance class rejected");}
    {Fixture bad;bad.memory.put(0x18,int32_t(0));live::Local value;check(!bad.take(value),"array slot cannot replace native saved index");}
    {Fixture bad;live::Local value;check(bad.take(value),"scalar failure before read");bad.fail_store=true;auto wanted=value.state;wanted.online_fraction=1;
        check(!bad.apply(value,wanted,receipt)&&receipt.attempted&&receipt.unknown&&!receipt.confirmed&&receipt.stores==0,"failed native store remains unknown");
        check(receipt.cause.find("injected scalar store failure")!=std::string::npos,"original store cause retained");}
    {Fixture bad;live::Local value;check(bad.take(value),"Reserve failure before read");bad.fail_reserve=true;auto wanted=value.state;wanted.party=fixture_party_key;
        check(!bad.apply(value,wanted,receipt)&&receipt.attempted&&receipt.unknown&&bad.reserves==1,"original Reserve called once and failure retained");}
    for(const auto number:{-1.0f,1.01f}){auto invalid=target_base;invalid.online_fraction=number;check(!live::valid(invalid,fixture.error),"fraction domain");}
    auto invalid=target_base;invalid.timeout=std::numeric_limits<float>::infinity();check(!live::valid(invalid,fixture.error),"nonfinite timeout rejected");
    invalid=target_base;invalid.completion=-1;check(!live::valid(invalid,fixture.error),"negative completion rejected");
    std::cout<<"PASS dungeon_instance_live "<<comparisons<<" comparisons\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL dungeon_instance_live: "<<e.what()<<'\n';return 1;}
