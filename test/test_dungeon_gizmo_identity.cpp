#include "dungeon_gizmo_identity.h"
#include "monster_snapshot_companion.h"
#include "snapshot_zone_key.h"
#include <iostream>
#include <stdexcept>
namespace gizmo=mt2mp::game::dungeon_gizmo_identity;
namespace companion=mt2mp::game::monster_snapshot_companion;
constexpr const char* first_uuid="11111111-1111-4111-8111-111111111111";
constexpr const char* second_uuid="22222222-2222-4222-8222-222222222222";
constexpr const char* birth_uuid="33333333-3333-4333-8333-333333333333";
int main()try{
    size_t checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison="+std::to_string(checks));};
    const std::string sha(64,'a'),next_sha(64,'b');std::string error,id;
    const auto record=[&](int32_t instance,int32_t child){return gizmo::record(sha,67,3,instance,child);};
    for(int32_t instance:{-1,0,9})for(int32_t child:{0,1,INT32_MAX})check(gizmo::saved(record(instance,child)),"Valid native template/Instance child key rejected");
    for(const auto& bad:std::vector<std::string>{gizmo::record(sha,-1,3,-1,0),gizmo::record(sha,67,-1,-1,0),gizmo::record(sha,67,3,-1,-1),
        record(-1,0)+"tail",record(0,0).substr(1),record(-1,0).replace(0,1,"A"),sha+":world:dungeon:gizmo:region{067}/building[3]/template/gizmo[0]",
        sha+":world:dungeon:gizmo:region{67}/building[3]/instance[2147483648]/gizmo[0]"})check(!gizmo::saved(bad),"Invalid native saved scene key admitted");
    check(gizmo::key(gizmo::birth(birth_uuid,0))&&!gizmo::key(std::string("action:")+birth_uuid+":dungeon:gizmo:00"),"Birth grammar does not preserve exact UUID/ordinal");
    const std::string exterior=sha+":world:building:17",scene=exterior+"/template";
    gizmo::Scope scope{first_uuid,sha,2,5,0x1000};
    gizmo::Member a{scene,0x2000,0x3000,0x4000,0x5000,0x100000012},b{scene,0x2100,0x3000,0x4000,0x5000,0x200000012};
    std::vector<gizmo::Saved> saved{{record(-1,0),a},{record(-1,1),b}};gizmo::Registry registry;
    check(registry.seal(scope,saved,nullptr,error),error.c_str());
    check(registry.resolve(scope,a.object,a.uid,id,error)&&id==saved[0].record,"Bare native record was replaced with an invented historical birth");
    check(!registry.resolve(scope,a.object,b.uid,id,error),"Same low32 UID resolved as complete identity");
    auto foreign=scope;foreign.generation++;gizmo::Member target;
    check(!registry.lookup(foreign,saved[0].record,target,error),"Old NativeOpen generation admitted");
    auto duplicate=saved;duplicate[1].member.uid=a.uid;
    check(!registry.seal(scope,duplicate,nullptr,error),"Duplicate full UID accepted at snapshot seal");
    check(registry.lookup(scope,saved[0].record,target,error)&&target==a,"Rejected seal partially replaced the existing registry");
    check(registry.verify_roster(scope,{a,b},error),"Exact current saved roster rejected");
    check(!registry.verify_roster(scope,{a},error),"Missing live current Gizmo was accepted");
    auto changed=b;changed.manager++;check(!registry.verify_roster(scope,{a,changed},error),"Foreign current scene owner accepted");
    duplicate=saved;duplicate[1].record=duplicate[0].record;check(!registry.seal(scope,duplicate,nullptr,error),"Repeated saved child admitted");
    auto foreign_member=a;foreign_member.scene="world";check(!gizmo::valid(foreign_member),"Root view accepted as Dungeon owner");
    const auto born=gizmo::birth(birth_uuid,0);gizmo::Member c{scene,0x2200,0x3000,0x4000,0x5000,0x300000012};
    check(registry.append(scope,born,c,error),error.c_str());check(!registry.append(scope,born,c,error),"Original-once confirmed birth replayed");
    auto aliased=c;aliased.object=a.object;check(!registry.append(scope,gizmo::birth(birth_uuid,1),aliased,error),"New UUID aliased live native pointer");
    std::vector<gizmo::Alias> aliases;
    auto capture=saved;capture.push_back({gizmo::record(next_sha,67,3,-1,2),c});capture[0].record=gizmo::record(next_sha,67,3,-1,0);capture[1].record=gizmo::record(next_sha,67,3,-1,1);
    for(auto& row:capture)row.member.scene=next_sha+":world:building:19/template";
    check(registry.capture(scope,next_sha,capture,aliases,error)&&aliases.size()==3,error.c_str());
    check(aliases[2].key==born,"Snapshot capture replaced a confirmed birth UUID with a record index");
    auto incomplete=capture;incomplete.pop_back();check(!registry.capture(scope,next_sha,incomplete,aliases,error)&&aliases.empty(),"Snapshot omitted a live confirmed member");
    check(registry.capture(scope,next_sha,capture,aliases,error),error.c_str());
    auto wrong_owner=capture;wrong_owner[0].member.manager++;check(!registry.capture(scope,next_sha,wrong_owner,aliases,error),"Cross-scene manager migration accepted");
    check(registry.capture(scope,next_sha,capture,aliases,error),error.c_str());
    // Native load constructs new peers' pointers and full UIDs in saved order.
    auto loaded=capture;for(size_t i=0;i<loaded.size();++i){loaded[i].member.object+=0x10000;loaded[i].member.uid+=0x400000000;loaded[i].member.manager+=0x10000;loaded[i].member.region+=0x10000;loaded[i].member.shard+=0x10000;}
    gizmo::Scope next{second_uuid,next_sha,3,6,0x11000};gizmo::Registry peer;
    check(peer.seal(next,loaded,&aliases,error),error.c_str());
    check(peer.resolve(next,loaded[2].member.object,loaded[2].member.uid,id,error)&&id==born,"Confirmed birth identity did not survive actual saved-slot rebinding");
    check(!peer.lookup(scope,born,target,error),"Old snapshot scope remained usable after load");
    const auto removed=loaded[0];check(peer.retire(next,aliases[0].key,removed.member,error),error.c_str());
    check(!peer.lookup(next,aliases[0].key,target,error),"Retired record remained a live target");
    check(!peer.retire(next,aliases[0].key,removed.member,error),"Retirement was replayed");
    check(peer.verify_roster(next,{loaded[1].member,loaded[2].member},error),"Retired record still required in live census");
    // Native swap-last changes array positions, not survivors' canonical identity.
    auto swapped=loaded;swapped[0]=swapped.back();swapped.pop_back();const std::string third_sha(64,'c');
    for(size_t i=0;i<swapped.size();++i)swapped[i].record=gizmo::record(third_sha,67,3,-1,int32_t(i));
    check(peer.capture(next,third_sha,swapped,aliases,error)&&aliases[0].key==born,"Swap-last retargeted an existing birth key");
    std::set<std::string> tombstones;check(peer.tombstones(next,tombstones,error)&&tombstones.contains(saved[0].record),"Retirement tombstone lost");
    check(!peer.append(next,saved[0].record,removed.member,error),"Retired saved key reused as a birth");
    auto wrong_aliases=aliases;wrong_aliases[0].key=wrong_aliases[1].key;
    gizmo::Scope third{first_uuid,third_sha,4,7,0x11000};
    check(!registry.seal(third,swapped,&wrong_aliases,error,tombstones),"Two records aliased the same canonical target");
    check(registry.seal(third,swapped,&aliases,error,tombstones),error.c_str());
    check(!registry.lookup(third,saved[0].record,target,error),"Retired identity resurrected after snapshot");
    companion::Manifest manifest;manifest.native_sha256=third_sha;manifest.native_bytes=100;manifest.dungeon_gizmos={true,aliases,tombstones};
    std::string bytes;check(companion::encode(manifest,bytes,error),error.c_str());companion::Manifest decoded;
    check(companion::decode(bytes,third_sha,100,decoded,error)&&decoded==manifest,"Gizmo aliases/retirement companion round trip changed identity");
    auto packet=nlohmann::json::parse(bytes);packet["dungeon_gizmos"]["retired"].push_back(saved[0].record);
    check(!companion::decode(packet.dump(),third_sha,100,decoded,error),"Duplicate tombstone accepted");
    packet=nlohmann::json::parse(bytes);packet["revision"]=4;check(!companion::decode(packet.dump(),third_sha,100,decoded,error),"Unknown companion revision accepted");
    for(bool network:{false,true}){companion::Manifest legacy;legacy.native_sha256=sha;legacy.native_bytes=100;legacy.network.present=network;
        check(companion::encode(legacy,bytes,error)&&companion::decode(bytes,sha,100,decoded,error)&&decoded==legacy,"Existing companion revision changed");}
    const auto monster=sha+":world:monster:"+mt2mp::game::snapshot_monster_key::scene_suffix(67,3,9,2);
    check(mt2mp::game::snapshot_monster_key::valid(monster),"Dungeon saved Monster child path rejected");
    check(!mt2mp::game::snapshot_monster_key::valid(monster+"/x")&&!mt2mp::game::snapshot_monster_key::valid(sha+":world:monster:inline:dungeon:region{67}/building[3]/template/monster[-1]"),"Invalid Dungeon Monster path admitted");
    for(int32_t instance:{-1,0,9})check(mt2mp::game::snapshot_zone_key::valid(sha+":world:zone:"+mt2mp::game::snapshot_zone_key::scene_suffix(67,3,instance,1)),"Original Dungeon Monster Zone child key rejected");
    check(!mt2mp::game::snapshot_zone_key::valid(sha+":world:zone:"+mt2mp::game::snapshot_zone_key::scene_suffix(67,3,9,-1)),"Negative Dungeon Monster Zone child admitted");
    registry.invalidate();check(!registry.lookup(third,born,target,error),"Title/world reload retained usable scene identity");
    std::cout<<checks<<" Dungeon Gizmo saved/birth/load/retirement/companion comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
