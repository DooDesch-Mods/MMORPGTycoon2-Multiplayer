#pragma once

template<class Check>
void scene_building_mapping_cases(Check&& check){
    namespace sc=mt2mp::game::scene_clone;
    const std::string operation="51000000-0000-4000-8000-000000000001";
    const std::string exterior="action:51000000-0000-4000-8000-000000000002";
    const auto scene=exterior+"/instance/0";
    std::string error;
    sc::Mapping mapping,decoded;
    mapping.operation=operation;
    mapping.scenes.push_back({scene,1,0,0,{},2});
    mapping.members.push_back({scene,sc::birth_key(operation,sc::Family::Gizmo,0),sc::Family::Gizmo,0,0,operation});
    for(uint32_t child=0;child<2;++child)
        mapping.members.push_back({scene,sc::birth_key(operation,sc::Family::Building,child+1),sc::Family::Building,child,child+1,operation});
    check(sc::valid(mapping,error),error);
    const auto encoded=sc::encode(mapping);
    check(encoded.at("schema")==3&&encoded.at("scenes")[0].size()==8,"Building mapping did not use exact new tuple");
    check(sc::decode(encoded,decoded,error)&&decoded==mapping,"Complete Building mapping did not survive codec");
    auto bad=encoded;bad["scenes"][0][7]=1;
    check(!sc::decode(bad,decoded,error),"Omitted current Building count admitted");
    bad=encoded;bad["members"][2][4]=bad["members"][1][4];
    check(!sc::decode(bad,decoded,error),"Two Buildings share a canonical alias");
    bad=encoded;bad["members"][2][1]=7;
    check(!sc::decode(bad,decoded,error),"Unknown family admitted");
    bad=encoded;bad["schema"]=2;
    check(!sc::decode(bad,decoded,error),"Legacy schema accepted extended tuple");
    bad=encoded;bad["scenes"][0].erase(7);
    check(!sc::decode(bad,decoded,error),"New schema accepted incomplete scene tuple");
    auto legacy=mapping;legacy.scenes[0].buildings=0;legacy.members.resize(1);
    auto legacy_wire=sc::encode(legacy);legacy_wire["schema"]=2;legacy_wire["scenes"][0].erase(7);
    check(sc::decode(legacy_wire,decoded,error)&&decoded==legacy,"Exact legacy Mapping2 no longer decodes");
    bad=legacy_wire;bad["members"][0][1]=6;
    check(!sc::decode(bad,decoded,error),"Legacy schema accepted new Building family");
    bad=legacy_wire;bad["scenes"][0].push_back(0);
    check(!sc::decode(bad,decoded,error),"Legacy schema accepted extra zero Building count");
    bad=legacy_wire;bad["extra"]=0;
    check(!sc::decode(bad,decoded,error),"Legacy exact object fields weakened");

    sc::Raw empty,born;
    empty.world.uuid=operation;empty.world.sha256=std::string(64,'a');empty.world.world_epoch=1;empty.world.generation=2;
    born=empty;sc::Pool pool;pool.scene.owner.key=exterior+"/template";pool.scene.owner.exterior_key=exterior;
    pool.scene.owner.type=1;pool.scene.owner.instance_index=-1;
    pool.scene.owner.shard=0x100;pool.scene.owner.region=0x200;pool.scene.owner.dungeon=0x300;
    pool.scene.owner.exterior=0x400;pool.scene.owner.exterior_uid=(uint64_t(1)<<40)+7;pool.scene.dungeon_uid=(uint64_t(1)<<40)+8;
    pool.building_manager=0x500;
    sc::Member member;member.family=sc::Family::Building;member.object=0x600;member.uid=(uint64_t(1)<<40)+9;pool.members.push_back(member);
    born.pools.push_back(pool);
    sc::Changes changes;sc::Mapping admitted,retired,restored;
    check(!sc::plan(empty,born,operation,{},nullptr,admitted,changes,error),"Ordinary Frame admitted new template");
    check(sc::plan(empty,born,operation,{},nullptr,admitted,changes,error,exterior),error);
    check(changes.births.size()==1&&admitted.scenes[0].buildings==1,"Original template Building birth omitted");
    born.pools[0].members[0].key=admitted.members[0].key;
    born.pools[0].members[0].origin=admitted.members[0].origin;
    born.pools[0].members[0].ordinal=admitted.members[0].ordinal;
    const std::string retirement="51000000-0000-4000-8000-000000000003";
    check(!sc::plan(born,empty,retirement,admitted,nullptr,retired,changes,error),"Ordinary Frame retired a template");
    check(sc::plan(born,empty,retirement,admitted,nullptr,retired,changes,error,exterior),error);
    check(changes.retired.size()==1&&retired.retired[0]==admitted.members[0],"Template retirement lost original Building alias");
    auto recreated=born;
    auto& actual=recreated.pools[0];
    actual.scene.owner.exterior+=0x1000;
    actual.members[0].key.clear();actual.members[0].origin.clear();actual.members[0].ordinal=0;
    const std::string restoration="51000000-0000-4000-8000-000000000004";
    check(sc::plan(empty,recreated,restoration,retired,nullptr,restored,changes,error,exterior,&born),error);
    check(changes.births.empty()&&changes.restored.size()==1&&restored.members==admitted.members&&restored.retired.empty(),
        "Original exterior recreation failed to retain its inverse-owned scene aliases");
    sc::Mapping replica;sc::Changes replica_changes;
    check(sc::plan(empty,recreated,restoration,retired,&restored,replica,replica_changes,error,exterior,&born)&&replica==restored,
        "Exact restored Host mapping could not bind actual peer members");
    auto wrong=restored;wrong.members[0].origin=retirement;
    check(!sc::plan(empty,recreated,restoration,retired,&wrong,replica,replica_changes,error,exterior,&born),
        "Restoration accepted different original provenance");
    auto changed=recreated;changed.pools[0].members.push_back(changed.pools[0].members[0]);
    check(!sc::plan(empty,changed,restoration,retired,nullptr,replica,replica_changes,error,exterior,&born),
        "Restoration accepted extra original member");
    changed=recreated;changed.pools[0].members[0].uid+=uint64_t(1)<<32;
    check(!sc::plan(empty,changed,restoration,retired,nullptr,replica,replica_changes,error,exterior,&born),
        "Restoration accepted replaced full UID with matching low bits");
    changed=recreated;changed.pools[0].building_manager++;
    check(!sc::plan(empty,changed,restoration,retired,nullptr,replica,replica_changes,error,exterior,&born),
        "Restoration accepted a replacement for an inverse-owned manager");
    changed=born;changed.pools[0].building_manager++;
    check(!sc::plan(born,changed,restoration,admitted,nullptr,replica,replica_changes,error),
        "Surviving BuildingManager replacement accepted");
    changed=born;changed.pools[0].scene.exterior_region=7;changed.pools[0].scene.exterior_slot=3;
    check(sc::plan(born,changed,restoration,admitted,nullptr,replica,replica_changes,error)&&
          replica.members==admitted.members&&replica_changes.births.empty()&&replica_changes.retired.empty(),
          "Original exterior move/reordered root slot changed complete scene identity");
    changed=born;changed.pools[0].scene.owner.shard++;
    check(!sc::plan(born,changed,restoration,admitted,nullptr,replica,replica_changes,error),
          "Slot-independent scene owner accepted a changed native Shard");
}
