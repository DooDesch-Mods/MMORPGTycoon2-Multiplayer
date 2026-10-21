#pragma once
template<class Check>void scene_scenery_mapping_cases(Check&& check){
    namespace sc=mt2mp::game::scene_clone;
    const std::string operation="61000000-0000-4000-8000-000000000001";
    const std::string exterior="action:61000000-0000-4000-8000-000000000002";
    const auto scene=exterior+"/template";std::string error;
    sc::Mapping mapping,decoded;mapping.operation=operation;mapping.scenes.push_back({scene,1,0,0,{},1,2});
    mapping.members.push_back({scene,sc::birth_key(operation,sc::Family::Gizmo,0),sc::Family::Gizmo,0,0,operation});
    mapping.members.push_back({scene,sc::birth_key(operation,sc::Family::Building,1),sc::Family::Building,0,1,operation});
    for(uint32_t child=0;child<2;++child)mapping.members.push_back({scene,sc::birth_key(operation,sc::Family::Scenery,child+2),sc::Family::Scenery,child,child+2,operation});
    check(sc::valid(mapping,error),error);const auto wire=sc::encode(mapping);
    check(wire.at("schema")==4&&wire.at("scenes")[0].size()==9,"Scenery Mapping4 requires one complete count after unchanged Building field");
    check(sc::decode(wire,decoded,error)&&decoded==mapping,"Complete Gizmo/Building/Scenery mapping changed across codec");
    auto bad=wire;bad["schema"]=3;check(!sc::decode(bad,decoded,error),"Legacy3 accepted a Scenery tuple");
    bad=wire;bad["schema"]=2;check(!sc::decode(bad,decoded,error),"Legacy2 accepted a Scenery tuple");
    bad=wire;bad["scenes"][0][8]=1;check(!sc::decode(bad,decoded,error),"Incomplete Scenery count accepted");
    bad=wire;bad["members"][3][4]=bad["members"][2][4];check(!sc::decode(bad,decoded,error),"Scenery aliases share an original birth key");
    bad=wire;bad["members"][3][1]=8;check(!sc::decode(bad,decoded,error),"Unknown family admitted in Mapping4");
    auto legacy=mapping;legacy.scenes[0].scenery=0;legacy.members.resize(2);
    check(sc::encode(legacy).at("schema")==3&&sc::decode(sc::encode(legacy),decoded,error)&&decoded==legacy,"Scenery-free Building mapping did not retain schema3");
    auto retired=mapping;retired.scenes.clear();retired.retired=retired.members;retired.members.clear();
    check(sc::valid(retired,error)&&sc::encode(retired).at("schema")==4&&sc::decode(sc::encode(retired),decoded,error)&&decoded==retired,"Retired Scenery identity lost schema4 after its scene departed");
    sc::Raw empty,born;empty.world.uuid=operation;empty.world.sha256=std::string(64,'a');empty.world.world_epoch=1;empty.world.generation=2;born=empty;
    sc::Pool pool;pool.scene.owner.key=scene;pool.scene.owner.exterior_key=exterior;pool.scene.owner.type=1;pool.scene.owner.instance_index=-1;
    pool.scene.owner.shard=0x100;pool.scene.owner.region=0x200;pool.scene.owner.dungeon=0x300;pool.scene.owner.exterior=0x400;
    pool.scene.owner.exterior_uid=(uint64_t(1)<<40)+7;pool.scene.dungeon_uid=(uint64_t(1)<<40)+8;pool.managers[0]=0x500;pool.building_manager=0x600;
    pool.members.push_back({sc::Family::Building,0,0x700,(uint64_t(1)<<40)+9,{}});
    pool.members.push_back({sc::Family::Scenery,0,0x800,(uint64_t(1)<<40)+10,{}});born.pools.push_back(pool);
    sc::Mapping initial,removed,restored;sc::Changes delta;
    check(sc::plan(empty,born,operation,{},nullptr,initial,delta,error,exterior)&&initial.scenes[0].buildings==1&&initial.scenes[0].scenery==1&&delta.births.size()==2,error);
    for(size_t i=0;i<born.pools[0].members.size();++i){auto& member=born.pools[0].members[i];const auto& row=initial.members[i];member.key=row.key;member.origin=row.origin;member.ordinal=row.ordinal;}
    const std::string remove="61000000-0000-4000-8000-000000000003",restore="61000000-0000-4000-8000-000000000004";
    check(sc::plan(born,empty,remove,initial,nullptr,removed,delta,error,exterior)&&delta.retired.size()==2,"Complete Scene retirement omitted Building or Scenery");
    auto recreated=born;recreated.pools[0].scene.owner.exterior+=0x1000;
    for(auto& member:recreated.pools[0].members){member.key.clear();member.origin.clear();member.ordinal=0;}
    check(sc::plan(empty,recreated,restore,removed,nullptr,restored,delta,error,exterior,&born)&&restored.members==initial.members&&delta.restored.size()==2,"Original restore lost shared Building/Scenery aliases");
    recreated.pools[0].members[1].uid+=uint64_t(1)<<32;
    check(!sc::plan(empty,recreated,restore,removed,nullptr,restored,delta,error,exterior,&born),"Scenery restore accepted equal low UID with changed full UID");
}
