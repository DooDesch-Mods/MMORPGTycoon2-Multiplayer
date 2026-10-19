#include "boss_snapshot_alias.h"
#include "monster_snapshot_companion.h"
#include <iostream>
#include <stdexcept>
namespace b=mt2mp::game::boss_identity;
namespace a=mt2mp::game::boss_snapshot_alias;
namespace r=mt2mp::game::character_boss_retirement_read;
namespace {
unsigned checks=0;const std::string uuid="11111111-2222-3333-4444-555555555555",sha(64,'a');
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
b::Scope scope(){return {uuid,"world",2,3,0x100,0x100,0x200,0x300};}
void registry(){b::Registry registry;std::string error;const auto current=scope();const auto key=sha+":world:boss:1",second=sha+":world:boss:2";const b::Entry one{current,0x400,0x100000001,true,false},two{current,0x500,0x200000001,true,false};
    need(registry.restore(current,{{key,one},{second,two}},error),error);std::string identified;need(registry.identify(current,one.pointer,one.uid,identified,error)&&identified==key,error);b::Entry lookup;need(registry.lookup(key,current,lookup,error)&&lookup==one,error);
    auto foreign=current;foreign.scene="foreign";need(!registry.lookup(key,foreign,lookup,error),"Boss identity crossed a native scene lifetime");need(!registry.identify(current,one.pointer,uint32_t(one.uid),identified,error),"truncated full UID resolved a Boss identity");
    const r::Pool before{current.shard,current.region,current.manager,{{one.pointer,one.uid},{two.pointer,two.uid}}},after{current.shard,current.region,current.manager,{}};
    auto absent=[](uint64_t,uintptr_t& pointer,std::string&){pointer=0;return true;};auto survivor=[&](uint64_t uid,uintptr_t& pointer,std::string&){pointer=uid==two.uid?two.pointer:0;return true;};
    need(!registry.retire_removed(current,before,after,{{one.pointer,one.uid},{two.pointer,two.uid}},survivor,error),"native registered Boss full UID was tombstoned");
    need(registry.lookup(key,current,lookup,error)&&registry.lookup(second,current,lookup,error),"partial retirement leaked after later selected UID failure");
    need(registry.retire_removed(current,before,after,{{one.pointer,one.uid},{two.pointer,two.uid}},absent,error),error);need(!registry.lookup(key,current,lookup,error)&&!registry.lookup(second,current,lookup,error),"original retired Boss key remains live");
    need(!registry.restore(current,{{key,one}},error),"original current tombstone was revived by snapshot restoration");
    const auto birth="action:"+uuid+":boss:0";auto pending=one;pending.uid=0x300000001;pending.pointer=0x600;pending.confirmed=false;
    need(registry.stage(birth,pending,error),error);need(!registry.lookup(birth,current,lookup,error),"unconfirmed original Boss birth was exposed");need(registry.confirm(birth,pending,error)&&registry.lookup(birth,current,lookup,error),error);
    need(!registry.stage(birth,pending,error),"original Boss action identity reused");std::string captured;need(registry.capture_key(current,pending.pointer,pending.uid,sha+":world:boss:9",captured,error)&&captured==birth,"snapshot lost canonical original Boss birth");
    need(!registry.retire_removed(current,before,before,{{one.pointer,one.uid}},absent,error),"unchanged manager was accepted as native Boss destruction");
}
void aliases(){std::string error;const auto saved=sha+":world:boss:1";a::Manifest manifest{true,{{saved,"action:"+uuid+":boss:0"}}},decoded;need(a::valid(manifest,sha,error),error);need(a::decode(a::encode(manifest),sha,decoded,error)&&decoded==manifest,error);
    auto wrong=manifest;wrong.rows[0].record=std::string(64,'b')+":world:boss:1";need(!a::valid(wrong,sha,error),"foreign native snapshot record admitted");wrong=manifest;wrong.rows.push_back(wrong.rows.front());need(!a::valid(wrong,sha,error),"duplicated native/canonical saved aliases admitted");
    auto wire=a::encode(manifest);wire[0]["local_uid"]=1;need(!a::decode(wire,sha,decoded,error),"native pointer/UID payload field admitted");wire=a::encode(manifest);wire[0]["key"]="1";need(!a::decode(wire,sha,decoded,error),"manager index was promoted to Boss canonical identity");
    need(a::valid({},sha,error),error);need(!a::valid({false,manifest.rows},sha,error),"absent aliases with stale rows admitted");
}
void companions(){namespace companion=mt2mp::game::monster_snapshot_companion;
    for(bool party:{false,true})for(bool boss:{false,true})for(bool network:{false,true})for(bool gizmo:{false,true}){
        companion::Manifest source;source.native_sha256=sha;source.native_bytes=123;source.parties.present=party;source.network.present=network;source.dungeon_gizmos.present=gizmo;
        if(boss)source.bosses={true,{{sha+":world:boss:1","action:"+uuid+":boss:0"}}};std::string bytes,error;companion::Manifest decoded;
        need(companion::encode(source,bytes,error)&&companion::decode(bytes,sha,123,decoded,error)&&decoded==source,error);
        const auto wire=nlohmann::json::parse(bytes);need(wire.at("revision")==int(party?5:boss?4:gizmo?3:network?2:1),"Boss companion4 replaced current Party5 priority or legacy companion format");
        auto bad=wire;bad["unowned_aliases"]=nlohmann::json::array();need(!companion::decode(bad.dump(),sha,123,decoded,error),"combined Party/Boss companion accepted an unknown field");
        if(party&&boss){bad=wire;bad["revision"]=4;need(!companion::decode(bad.dump(),sha,123,decoded,error),"Boss4 accepted a current Party5 component");bad=wire;bad["bosses"][0]["record"]=std::string(64,'b')+":world:boss:1";need(!companion::decode(bad.dump(),sha,123,decoded,error),"Party5 optional Boss alias ignored native file hash");}
    }
}
}
int main()try{registry();aliases();companions();std::cout<<checks<<" canonical Boss saved/birth/atomic original retirement and Party5 companion assertions passed\n";return 0;}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
