#include "dungeon_monster_reference.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace references=mt2mp::game::dungeon_monster_reference;
namespace identities=mt2mp::game::direct_monster_identity;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t changing=0;unsigned hits=0;
    template<class T>void put(uintptr_t address,const T& value){const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=source[i];}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& error){auto* out=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i){const auto at=bytes.find(object+offset+i);if(at==bytes.end()){error="original Dungeon Monster memory read failed";return false;}out[i]=at->second;}
        if(changing==object+offset&&++hits==2)out[0]^=8;return true;}
};
int main()try{
    constexpr uintptr_t image=0x10000000,root=0x20000000,shard=0x21000000,region=0x22000000,manager=0x23000000,data=0x24000000,target=0x25000000,sibling=0x26000000;
    constexpr uint64_t target_uid=0x100000007,sibling_uid=0x200000007;
    Memory memory;std::string error;size_t checks=0;identities::Registry registry;
    identities::Scope scope{"11111111-2222-3333-4444-555555555555",6,7,root,region,manager};
    mt2mp::game::dungeon_scene_roster::Scene scene;scene.owner.type=1;scene.owner.shard=shard;scene.owner.region=region;scene.owner.key=std::string(64,'a')+":world:building:7/template";
    const auto canonical=std::string(64,'a')+":world:monster:8";
    const auto check=[&](bool valid,const std::string& cause){++checks;if(!valid)throw std::runtime_error(cause+" comparison="+std::to_string(checks));};
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.read(object,offset,value,cause);};
    std::map<uint64_t,uintptr_t> full_uids;bool resolve_fail=false;
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){const auto value=full_uids.find(uid);if(value==full_uids.end()){cause="original Dungeon full UID lookup failed";return false;}found=value->second;return true;};
    auto resolve=[&](const identities::Scope& current,uintptr_t object,uint64_t uid,std::string& key,std::string& cause){if(resolve_fail){cause="original confirmed identity resolution failed";return false;}return registry.resolve(current,object,uid,key,cause);};
    const auto setup=[&]{scope.scene=scene.owner.key;memory={};full_uids={{target_uid,target},{sibling_uid,sibling}};resolve_fail=false;registry={};
        memory.put(manager,image+0x1714100);memory.put(manager+0x80,region);memory.put(region+0x70,shard);memory.put(region+0x988,manager);
        memory.put(manager+0x38,image+0x16e8b60);memory.put(manager+0x40,data);memory.put(manager+0x48,int32_t(2));memory.put(manager+0x4c,int32_t(2));memory.put(data,target);memory.put(data+8,sibling);
        for(const auto object:{target,sibling}){memory.put(object,image+0x16c0770);memory.put(object+0x18,object==target?target_uid:sibling_uid);memory.put(object+0x170,shard);memory.put(object+0x178,region);memory.put(object+0x8d0,uintptr_t(0));memory.put(object+0x8d8,uintptr_t(0));}
        if(!registry.restore(scope,{{canonical,{scope,target,target_uid,true,false}}},error))throw std::runtime_error(error);
    };
    references::Witness witness;setup();check(references::read(at,find,resolve,image,scope,scene,manager,target,witness,error),error);
    check(witness.key==canonical&&witness.uid==target_uid&&witness.shard==shard&&witness.members.size()==2,"Template reference key/full UID/complete manager lost");
    scene.owner.type=2;scene.owner.instance_index=17;scene.owner.key=std::string(64,'a')+":world:building:7/instance/17";setup();
    check(references::read(at,find,resolve,image,scope,scene,manager,target,witness,error)&&witness.scene==scene.owner.key,error);
    for(int mode=0;mode<15;++mode){setup();
        if(mode==0)memory.put(manager,image+0x1714101);if(mode==1)memory.put(manager+0x80,region+8);if(mode==2)memory.put(region+0x70,shard+8);
        if(mode==3)memory.put(region+0x988,manager+8);if(mode==4)memory.put(manager+0x38,image+0x16e8b61);if(mode==5)memory.put(data+8,target);
        if(mode==6)memory.put(sibling+0x18,target_uid);if(mode==7)memory.put(target+0x18,sibling_uid);if(mode==8)full_uids[target_uid]=sibling;
        if(mode==9)memory.put(target+0x8d0,uintptr_t(0x27000000));if(mode==10)memory.put(target+0x8d8,uintptr_t(0x28000000));
        if(mode==11)memory.put(target+0x170,root);if(mode==12)memory.put(sibling+0x178,region+8);if(mode==13)memory.changing=manager+0x40;
        if(mode==14)memory.changing=target+0x18;
        check(!references::read(at,find,resolve,image,scope,scene,manager,target,witness,error)&&!error.empty()&&witness.key.empty()&&witness.members.empty(),"Foreign/racing Dungeon Monster admitted mode="+std::to_string(mode));
    }
    setup();memory.bytes.erase(target+0x18);check(!references::read(at,find,resolve,image,scope,scene,manager,target,witness,error)&&error=="original Dungeon Monster memory read failed","Original native read cause replaced");
    setup();full_uids.erase(target_uid);check(!references::read(at,find,resolve,image,scope,scene,manager,target,witness,error)&&error=="original Dungeon full UID lookup failed","Original lookup failure cause replaced");
    setup();resolve_fail=true;check(!references::read(at,find,resolve,image,scope,scene,manager,target,witness,error)&&error=="original confirmed identity resolution failed","Original canonical identity cause replaced");
    setup();check(!references::read(at,find,resolve,image,scope,scene,manager,target+8,witness,error)&&error.find("absent")!=std::string::npos,"Unmanaged target was inferred by matching content");
    setup();auto wrong_scope=scope;wrong_scope.generation++;
    check(!references::read(at,find,resolve,image,wrong_scope,scene,manager,target,witness,error),"Foreign world generation adopted a confirmed identity");
    setup();auto wrong_scene=scope;wrong_scene.scene="world";
    check(!references::read(at,find,resolve,image,wrong_scene,scene,manager,target,witness,error),"Existing direct canonical binding crossed Root/Dungeon scene owner");
    setup();scene.owner.type=0;check(!references::read(at,find,resolve,image,scope,scene,manager,target,witness,error),"Root scene accepted through Dungeon-only binder");
    std::cout<<checks<<" Dungeon direct Monster current scene/full UID/manager/confirmed key comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
