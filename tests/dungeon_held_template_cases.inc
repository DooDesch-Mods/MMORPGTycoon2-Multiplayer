#pragma once
#include "dungeon_held_template.h"
#include "dungeon_held_graph.h"
#include <cstring>
#include <map>

template<class Check>
void dungeon_held_template_cases(Check&& assert_case){
    namespace held=mt2mp::game::dungeon_held_template;
    namespace roster=mt2mp::game::dungeon_scene_roster;
    constexpr uintptr_t image=0x10000000,root_shard=0x20000000,node=0x21000000;
    constexpr uintptr_t dungeon=0x22000000,template_shard=0x23000000,template_region=0x24000000;
    constexpr uintptr_t instance=0x25000000,instance_shard=0x26000000,instance_region=0x27000000,array=0x28000000;
    constexpr uint64_t exterior_uid=0x100000007ULL,dungeon_uid=0x200000007ULL;
    std::map<uintptr_t,uint8_t> bytes;
    std::map<uint64_t,uintptr_t> objects{{dungeon_uid,dungeon}};
    const auto write=[&](uintptr_t address,const auto& value){
        const auto* data=reinterpret_cast<const uint8_t*>(&value);
        for(size_t i=0;i<sizeof(value);++i)bytes[address+i]=data[i];
    };
    uintptr_t injected=0;unsigned injected_reads=0;
    auto read=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){
        auto* data=reinterpret_cast<uint8_t*>(&value);
        for(size_t i=0;i<sizeof(value);++i){
            const auto found=bytes.find(address+offset+i);
            if(found==bytes.end()){cause="held fixture original read failed";return false;}data[i]=found->second;
        }
        if(address+offset==injected&&++injected_reads==2){cause="held fixture second owner read failed";return false;}
        return true;
    };
    auto find=[&](uint64_t uid,uintptr_t& object,std::string&){object=objects.contains(uid)?objects.at(uid):0;return true;};
    {
        constexpr uintptr_t graph=0x30000000,operations=0x31000000;
        write(graph+0x18,image+0x16e2dc0);write(graph+0x20,operations);
        write(graph+0x28,int32_t{2});write(graph+0x2c,int32_t{2});
        write(operations,node);write(operations+8,node+0x100);
        std::string cause;
        assert_case(mt2mp::game::dungeon_held_graph::member(read,image,graph,node,cause),cause);
        write(operations+8,node);
        assert_case(!mt2mp::game::dungeon_held_graph::member(read,image,graph,node,cause),
                    "Held graph accepted duplicated native ownership");
        write(operations+8,node+0x100);write(operations,node+0x200);
        assert_case(!mt2mp::game::dungeon_held_graph::member(read,image,graph,node,cause),
                    "Trimmed/consumed held node remained an owned graph member");
        write(operations,node);write(graph+0x18,uintptr_t{0});
        assert_case(!mt2mp::game::dungeon_held_graph::member(read,image,graph,node,cause),
                    "Foreign native graph array class admitted");
        write(graph+0x18,image+0x16e2dc0);injected=operations;injected_reads=0;
        assert_case(!mt2mp::game::dungeon_held_graph::member(read,image,graph,node,cause)&&
                    cause=="held fixture second owner read failed","Held graph lost second read cause");
        injected=0;injected_reads=0;
    }
    roster::Scene template_scene;
    auto& owner=template_scene.owner;
    owner.exterior_key="action:51000000-0000-4000-8000-000000000002";owner.key=owner.exterior_key+"/template";
    owner.type=1;owner.instance_index=-1;owner.exterior_uid=exterior_uid;owner.dungeon=dungeon;
    owner.shard=template_shard;owner.region=template_region;template_scene.dungeon_uid=dungeon_uid;
    auto instance_scene=template_scene;auto& instance_owner=instance_scene.owner;
    instance_owner.key=owner.exterior_key+"/instance/4";instance_owner.type=2;instance_owner.instance_index=4;
    instance_owner.instance=instance;instance_owner.shard=instance_shard;instance_owner.region=instance_region;
    const std::vector<roster::Scene> scenes{template_scene,instance_scene};
    write(node,uintptr_t(image+0x173c5b0));write(image+0x173c5b0+0x10,uintptr_t(image+0x548c70));
    write(node+0xb8,exterior_uid);write(node+0x168,root_shard);write(node+0x1e0,dungeon);
    write(dungeon,uintptr_t(image+0x16c0560));write(dungeon+0x18,dungeon_uid);
    write(dungeon+0x48,template_shard);write(dungeon+0x50,template_region);write(dungeon+0x6f8,uintptr_t(0));
    write(dungeon+0x4c0,array);write(dungeon+0x4c8,int32_t(1));write(dungeon+0x4cc,int32_t(1));write(array,instance);
    write(template_shard,uintptr_t(image+0x174ad20));write(template_shard+0x48,int32_t(1));write(template_shard+0x18,dungeon);
    write(template_region,uintptr_t(image+0x1787930));write(template_region+0x70,template_shard);
    write(instance,uintptr_t(image+0x1719450));write(instance+0x18,int32_t(4));write(instance+0x30,dungeon);
    write(instance+0x38,instance_shard);write(instance+0x40,instance_region);
    write(instance_shard,uintptr_t(image+0x174ad20));write(instance_shard+0x48,int32_t(2));write(instance_shard+0x20,instance);
    write(instance_region,uintptr_t(image+0x1787930));write(instance_region+0x70,instance_shard);
    const auto initial=bytes;std::string cause;
    const auto run=[&](){cause.clear();return held::read(read,find,image,root_shard,node,template_scene,scenes,cause);};
    assert_case(run(),cause);
    objects[exterior_uid]=0x29000000;assert_case(!run(),"Alive removed exterior admitted");objects.erase(exterior_uid);
    write(node+0x1e0,uintptr_t(0));assert_case(!run(),"Consumed inverse retained a Dungeon");bytes=initial;
    write(node+0xb8,uint64_t(7));assert_case(!run(),"Inverse accepted truncated exterior UID");bytes=initial;
    write(dungeon+0x18,uint64_t(7));assert_case(!run(),"Dungeon accepted truncated full UID");bytes=initial;
    write(dungeon+0x6f8,uintptr_t(0x29000000));assert_case(!run(),"Still-attached Dungeon admitted");bytes=initial;
    write(instance+0x30,uintptr_t(0x29000000));assert_case(!run(),"Foreign Instance owner admitted");bytes=initial;
    write(instance+0x18,int32_t(5));assert_case(!run(),"Changed original Instance index admitted");bytes=initial;
    write(dungeon+0x4c8,int32_t(0));assert_case(!run(),"Missing retained Instance admitted");bytes=initial;
    write(image+0x173c5b0+0x10,uintptr_t(image+0x548b00));assert_case(!run(),"Wrong original Make method admitted");bytes=initial;
    injected=node+0x1e0;injected_reads=0;
    assert_case(!run()&&cause=="held fixture second owner read failed","Second owner read or original cause was lost");
}
