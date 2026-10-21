#include "scene_clone/raw.h"
#include "direct_monster_identity.h"
#include "world_zone_key.h"
#include "scene_building_mapping_cases.inc"
#include "scene_scenery_mapping_cases.inc"
#include "scene_scenery_snapshot_read_cases.inc"
#include "dungeon_held_template_cases.inc"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
namespace sc=scene_clone;
const std::string seed="11111111-1111-4111-8111-111111111111",op="22222222-2222-4222-8222-222222222222",op2="33333333-3333-4333-8333-333333333333",op3="44444444-4444-4444-8444-444444444444";
size_t checks=0;
void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
sc::Pool pool(uint32_t index,uintptr_t offset=0){
    sc::Pool p;p.scene.owner.type=index?2:1;p.scene.owner.instance_index=index;
    p.scene.owner.exterior_key="action:"+seed;p.scene.owner.key=p.scene.owner.exterior_key+(index?"/instance/"+std::to_string(index):"/template");
    p.scene.owner.region=offset+100+index*100;p.scene.owner.shard=offset+101+index*100;
    p.scene.owner.dungeon=offset+102;p.scene.owner.instance=index?offset+103+index*100:0;
    p.scene.owner.exterior=offset+104;p.scene.owner.exterior_uid=uint64_t(1)<<40;
    p.scene.dungeon_uid=(uint64_t(1)<<40)+1;
    for(size_t i=0;i<4;++i)p.managers[i]=offset+110+index*100+i;
    for(uint32_t i=0;i<3;++i){sc::Member m;m.family=sc::Family(i);m.object=offset+120+index*100+i;m.uid=(uint64_t(1)<<40)+m.object;p.members.push_back(m);}
    p.zones.zones.resize(1);p.zones.zones[0].pointer=p.members[1].object;p.zones.zones[0].uid=p.members[1].uid;
    if(!index)for(auto& m:p.members)m.key=sc::birth_key(seed,m.family,uint32_t(m.family));
    return p;
}
sc::Raw raw(uintptr_t offset=0){sc::Raw r;r.world.uuid=seed;r.world.sha256=std::string(64,'a');r.world.world_epoch=1;r.world.generation=2;r.pools.push_back(pool(0,offset));return r;}
void adopt(sc::Raw& raw,const sc::Mapping& mapping){for(auto& p:raw.pools)for(auto& m:p.members){auto it=std::find_if(mapping.members.begin(),mapping.members.end(),[&](const auto& a){return a.scene==p.scene.owner.key&&a.family==m.family&&a.child==m.child;});check(it!=mapping.members.end(),"adopt row missing");m.key=it->key;m.origin=it->origin;m.ordinal=it->ordinal;}}
int main(){try{std::string e;sc::Mapping first,second,third,copy;sc::Changes delta;auto before=raw(),after=before;after.pools.push_back(pool(1));after.pools.push_back(pool(2));
    check(sc::plan(before,after,op,{},nullptr,first,delta,e),e);check(first.members.size()==9&&delta.births.size()==6,"complete three-family batch");
    for(size_t i=0;i<6;++i){check(delta.births[i].ordinal==i,"birth ordinal reset per scene/family");check(delta.births[i].key==sc::birth_key(op,delta.births[i].family,uint32_t(i)),"derived key mismatch");}
    check(first.members[0].key==before.pools[0].members[0].key&&first.members[0].origin.empty(),"initial alias replaced");
    check(sc::decode(sc::encode(first),copy,e)&&copy==first,"portable complete mapping roundtrip");
    auto invalid=first;invalid.members.pop_back();check(!sc::valid(invalid,e),"partial current mapping accepted");invalid=first;std::swap(invalid.members[3],invalid.members[4]);check(!sc::valid(invalid,e),"unordered member accepted");
    invalid=first;invalid.members[3].origin=op2;check(!sc::valid(invalid,e),"forged provenance accepted");
    auto json=sc::encode(first);json["members"][3][2]=-1;check(!sc::decode(json,copy,e),"negative child accepted");json=sc::encode(first);json["extra"]=1;check(!sc::decode(json,copy,e),"unknown mapping field accepted");
    adopt(after,first);check(sc::plan(after,after,op2,first,nullptr,second,delta,e),e);check(delta.births.empty()&&second.members==first.members,"second drain lost prior birth assignments");
    auto clientBefore=raw(10000),clientAfter=clientBefore;clientAfter.pools.push_back(pool(1,10000));clientAfter.pools.push_back(pool(2,10000));
    check(sc::plan(clientBefore,clientAfter,op2,{},&second,copy,delta,e),e);check(copy==second&&delta.births.size()==6,"superseding frame could not admit prior births");
    auto changed=clientAfter;changed.pools[1].members[0].uid=0;check(!sc::plan(clientBefore,changed,op2,{},&second,copy,delta,e),"missing full UID accepted");
    changed=clientAfter;changed.pools[1].members[0].uid=changed.pools[1].members[1].uid;check(!sc::plan(clientBefore,changed,op2,{},&second,copy,delta,e),"duplicate full UID accepted");
    auto departed=after;departed.pools.erase(departed.pools.begin()+1);check(sc::plan(after,departed,op3,second,nullptr,third,delta,e),e);check(third.retired.size()==3&&delta.retired.size()==3,"retirement omitted family");
    sc::Mapping retired;retired.retired=delta.retired;std::vector<uint64_t> seen;
    check(sc::retirements_ready(after,retired,[&](uint64_t uid,uintptr_t& out,std::string&){seen.push_back(uid);out=0;return true;},e),e);
    check(seen.size()==3&&seen[0]>UINT32_MAX,"retirement truncated full UID");
    check(!sc::retirements_ready(after,retired,[](uint64_t,uintptr_t& out,std::string&){out=123;return true;},e),"partial native deletion accepted");
    check(e.find("full_uid=")!=std::string::npos,"retirement cause omitted full UID");
    check(!sc::retirements_ready(after,retired,[](uint64_t,uintptr_t&,std::string& cause){cause="FindUID unavailable";return false;},e)&&e=="FindUID unavailable","lookup failure suppressed");
    adopt(departed,third);check(sc::plan(departed,departed,seed,third,nullptr,copy,delta,e),e);check(copy.retired==third.retired&&delta.retired.empty(),"durable retirement lost/replayed");
    adopt(clientAfter,second);auto clientDeparted=clientAfter;clientDeparted.pools.erase(clientDeparted.pools.begin()+1);
    check(sc::plan(clientAfter,clientDeparted,op3,second,&third,copy,delta,e),e);invalid=third;invalid.retired.clear();check(!sc::plan(clientAfter,clientDeparted,op3,second,&invalid,copy,delta,e),"Host omitted local retirement");
    changed=after;changed.pools[0].managers[0]++;check(!sc::plan(after,changed,op3,second,nullptr,copy,delta,e),"surviving owner replacement accepted");
    changed=after;changed.pools.erase(changed.pools.begin());check(!sc::plan(after,changed,op3,second,nullptr,copy,delta,e),"template retirement accepted");
    namespace gi=dungeon_gizmo_identity;gi::Registry registry;gi::Scope scope{seed,std::string(64,'a'),1,2,100};
    gi::Member original{before.pools[0].scene.owner.key,10,20,30,40,(uint64_t(1)<<40)+10};const auto saved=gi::record(scope.sha,1,0,-1,0);
    check(registry.seal(scope,{{saved,original}},nullptr,e),e);auto preview=registry;
    for(const auto& row:first.members)if(row.family==sc::Family::Gizmo&&!row.origin.empty()){const auto n=row.scene==after.pools[1].scene.owner.key?1:2;const auto& p=after.pools[n];const auto& m=p.members[0];check(preview.append(scope,row.key,{row.scene,m.object,p.managers[3],p.scene.owner.shard,p.scene.owner.region,m.uid},e),e);}
    std::string key;check(preview.resolve(scope,original.object,original.uid,key,e)&&key==saved,"additive preview lost saved alias");check(!registry.resolve(scope,after.pools[1].members[0].object,after.pools[1].members[0].uid,key,e),"preview published early");
    registry=preview;const auto& p=after.pools[1];const auto& m=p.members[0];check(registry.retire(scope,first.members[3].key,{p.scene.owner.key,m.object,p.managers[3],p.scene.owner.shard,p.scene.owner.region,m.uid},e),e);
    check(!registry.resolve(scope,m.object,m.uid,key,e),"retired Gizmo still resolves");
    auto all_before=raw(),all_after=all_before;
    const auto extra=[](sc::Pool& p,bool saved){for(uint8_t family=3;family<6;++family){sc::Member row;row.family=sc::Family(family);row.object=p.managers[0]+10000+family;row.uid=(uint64_t(7)<<40)+row.object;
        if(saved)row.key=std::string(64,'a')+":world:zone-type:"+std::to_string(sc::zone_type(row.family))+":scene:67:3:-1:child:0";p.members.push_back(row);}};
    extra(all_before.pools[0],true);all_after=all_before;all_after.pools.push_back(pool(1));extra(all_after.pools[1],false);
    sc::Mapping complete;check(sc::plan(all_before,all_after,op,{},nullptr,complete,delta,e),e);
    check(complete.members.size()==12&&delta.births.size()==6&&complete.scenes[0].other_zones==std::array<uint32_t,3>{1,1,1},"All four Zone types omitted from complete scene mapping");
    check(sc::decode(sc::encode(complete),copy,e)&&copy==complete,"All-type complete native mapping codec differs");
    for(const auto& row:delta.births)check(row.key==sc::birth_key(op,row.family,row.ordinal),"All-type birth changed shared operation/ordered ordinal");
    auto bad_all=complete;bad_all.scenes[1].other_zones[2]=0;check(!sc::valid(bad_all,e),"Omitted Story native count accepted");
    bad_all=complete;bad_all.members.back().family=sc::Family::PuzzleZone;check(!sc::valid(bad_all,e),"Story member reclassified as Puzzle");
    auto all_wire=sc::encode(complete);all_wire["schema"]=1;check(!sc::decode(all_wire,copy,e),"Old incomplete scene-mapping schema accepted");
    all_wire=sc::encode(complete);all_wire["scenes"][0][5]=-1;check(!sc::decode(all_wire,copy,e),"Negative Puzzle count accepted");
    adopt(all_after,complete);auto all_departed=all_after;all_departed.pools.pop_back();sc::Mapping complete_retired;
    check(sc::plan(all_after,all_departed,op2,complete,nullptr,complete_retired,delta,e)&&delta.retired.size()==6,e);
    sc::Mapping just_retired;just_retired.retired=delta.retired;seen.clear();check(sc::retirements_ready(all_after,just_retired,[&](uint64_t uid,uintptr_t& found,std::string&){seen.push_back(uid);found=0;return true;},e),e);
    check(seen.size()==6&&seen.back()>UINT32_MAX,"Non-Monster retirement did not inspect complete full UIDs");
    const std::string prefix=std::string(64,'a')+":world:zone-type:";
    for(const auto type:{0,2,3})check(world_zone_key::valid(prefix+std::to_string(type)+":root:67:child:0"),"Original non-Monster saved root key rejected");
    for(const auto& bad:{prefix+"1:root:67:child:0",prefix+"2:record:0",prefix+"2:root:067:child:0",prefix+"3:scene:67:3:-2:child:0",prefix+"0:root:67:child:4096"})check(!world_zone_key::valid(bad),"Malformed or Monster extra-Zone key accepted");
    namespace dm=direct_monster_identity;
    dm::Registry direct_registry;
    const auto clone_scope=sc::direct_scope(after,after.pools[1]);
    const auto template_scope=sc::direct_scope(after,after.pools[0]);
    check(clone_scope.scene==after.pools[1].scene.owner.key&&!dm::root_owned(clone_scope)&&dm::valid_scene(clone_scope),"Clone direct Monster lost its actual Instance scene");
    check(template_scope.scene==after.pools[0].scene.owner.key&&!dm::root_owned(template_scope)&&dm::valid_scene(template_scope),"Template direct Monster lost its actual scene");
    check(clone_scope.snapshot==after.world.uuid&&clone_scope.epoch==after.world.world_epoch&&clone_scope.generation==after.world.generation&&
        clone_scope.root==uintptr_t(after.world.map)+0x1c0&&clone_scope.region==after.pools[1].scene.owner.region&&clone_scope.manager==after.pools[1].managers[2],"Clone scope lost current World/Region/manager identity");
    const auto& clone_monster=after.pools[1].members[2];
    check(direct_registry.stage(op,clone_scope,clone_monster.object,clone_monster.uid,e)&&direct_registry.confirm(op,e),e);
    check(direct_registry.resolve(clone_scope,clone_monster.object,clone_monster.uid,key,e)&&key==monster_target_key::canonical(op),"Clone direct alias did not resolve in its actual scene");
    auto wrong_scope=clone_scope;wrong_scope.scene="world";
    check(!direct_registry.resolve(wrong_scope,clone_monster.object,clone_monster.uid,key,e),"Clone direct alias leaked into root scope");
    wrong_scope=sc::direct_scope(after,after.pools[2]);
    check(!direct_registry.resolve(wrong_scope,clone_monster.object,clone_monster.uid,key,e),"Clone direct alias leaked into sibling Instance");
    check(!direct_registry.resolve(clone_scope,clone_monster.object,clone_monster.uid&0xffffffffULL,key,e),"Clone direct alias accepted truncated UID");
    dm::Entry direct_entry;
    check(direct_registry.lookup(op,clone_scope,direct_entry,e)&&direct_registry.retire(op,direct_entry,e),e);
    std::vector<std::pair<std::string,dm::Entry>> direct_rows;
    check(direct_registry.enumerate(clone_scope.snapshot,clone_scope.epoch,clone_scope.generation,clone_scope.root,direct_rows,e)&&
        direct_rows.size()==1&&direct_rows[0].second.retired&&!dm::root_owned(direct_rows[0].second.scope),"Retired clone lost its scene partition");
    check(!direct_registry.resolve(clone_scope,clone_monster.object,clone_monster.uid,key,e),"Retired clone remains live");
    scene_building_mapping_cases(check);
    scene_scenery_mapping_cases(check);
    scene_scenery_read_fixture::run(check);
    dungeon_held_template_cases(check);
    std::cout<<checks<<" scene clone checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"scene clone check "<<checks<<": "<<x.what()<<'\n';return 1;}}
