#include "mixed_path_graph.h"
#include "mixed_path_identity.h"
#include "mixed_path_remove.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
using namespace mt2mp::game::mixed_path_graph;
void need(bool condition,const std::string& cause){if(!condition)throw std::runtime_error(cause);}
int main(){try{
    unsigned cases=0;std::string error;
    mixed_path_native::Census census;
    census.regions={{1,0x1000},{2,0x2000}};
    mixed_path_native::Path wall,pass;
    wall.member={0x3000,11,{1,4,0}};wall.owner=0x1000;wall.start=0x5000;wall.end=0x6000;
    wall.points={{0,0,0},{1,0,0}};wall.distances={0,1};
    pass.member={0x4000,12,{1,5,0}};pass.owner=0x1000;pass.second=0x2000;
    pass.start=0x6000;pass.end=0x7000;pass.points={{1,0,0},{2,0,0}};pass.distances={0,1};
    census.paths={wall,pass};
    mixed_path_native::Node a,b,c;
    a.member={0x5000,21,{1,0,0}};a.owner=0x1000;a.position={0,0,0};a.edges={0x3000};
    b.member={0x6000,22,{1,0,1}};b.owner=0x1000;b.position={1,0,0};b.edges={0x3000,0x4000};
    c.member={0x7000,23,{2,0,0}};c.owner=0x2000;c.position={2,0,0};c.edges={0x4000};
    census.nodes={a,b,c};census.remote={{2,5,0,0x4000}};
    const std::map<uint64_t,std::string> keys{{11,"action:w:wall:0"},{12,"action:p:pass:0"},
        {21,"action:w:node:0"},{22,"action:w:node:1"},{23,"action:p:node:0"}};
    Snapshot source;need(build(census,keys,source,error),error);++cases;
    need(source.rows["paths"][1][3]==2&&source.rows["remote"].size()==1&&
         source.rows["paths"][1][4][1]=="action:p:node:0", "cross Region Pass reference lost");++cases;
    auto remaining=census;
    remaining.paths.erase(remaining.paths.begin()+1);remaining.nodes.erase(remaining.nodes.begin()+2);
    remaining.nodes[1].edges.pop_back();remaining.remote.clear();
    Snapshot removed;need(build(remaining,keys,removed,error),error);++cases;
    std::set<std::string> retired;
    need(mixed_path_remove::delta(source,removed,"action:p:pass:0",retired,error)&&
         retired==std::set<std::string>{"action:p:pass:0","action:p:node:0"},error);++cases;
    auto reject_remove=[&](Snapshot invalid_after,const char* why){
        const auto previous=retired;
        need(!mixed_path_remove::delta(source,invalid_after,"action:p:pass:0",retired,error)&&
             !error.empty()&&retired==previous,why);++cases;
    };
    auto changed=removed;changed.paths[0].local_uid+=uint64_t(1)<<32;
    reject_remove(changed,"partial full UID survivor accepted");
    changed=removed;changed.rows["paths"][0][5][0][0]=std::bit_cast<uint32_t>(42.f);
    reject_remove(changed,"unrelated Wall geometry changed");
    changed=removed;changed.rows["nodes"][1][4].push_back("action:p:pass:0");
    reject_remove(changed,"retired Path reciprocal edge retained");
    changed=removed;changed.rows["remote"]=source.rows["remote"];
    reject_remove(changed,"retired Path remote membership retained");
    reject_remove(source,"target Path remains live");
    auto shuffled=removed;std::swap(shuffled.nodes[0],shuffled.nodes[1]);
    std::swap(shuffled.rows["nodes"][0],shuffled.rows["nodes"][1]);
    need(mixed_path_remove::delta(source,shuffled,"action:p:pass:0",retired,error),error);++cases;
    auto wall_only=remaining;wall_only.paths.clear();wall_only.nodes.clear();
    Snapshot empty;need(build(wall_only,keys,empty,error),error);++cases;
    need(mixed_path_remove::delta(removed,empty,"action:w:wall:0",retired,error)&&retired.size()==3,error);++cases;
    for(int32_t type=1;type<6;++type){
        auto typed=census;typed.nodes[2].member.ref.type=type;
        Snapshot typed_before,typed_after;
        need(build(typed,keys,typed_before,error),error);++cases;
        typed.paths.pop_back();typed.remote.clear();typed.nodes[1].edges.pop_back();
        if(type<=1)typed.nodes.pop_back();else typed.nodes[2].edges.clear();
        need(build(typed,keys,typed_after,error),error);++cases;
        need(mixed_path_remove::delta(typed_before,typed_after,"action:p:pass:0",retired,error)&&
             retired.size()==size_t(type<=1?2:1),"original ShouldDestroyNow node-type rule differs: "+error);++cases;
    }
    Json expected,actual;need(rebase(source,source,expected,error),error);++cases;
    auto reordered=source;
    std::swap(reordered.paths[0],reordered.paths[1]);
    std::swap(reordered.rows["paths"][0],reordered.rows["paths"][1]);
    std::swap(reordered.nodes[0],reordered.nodes[2]);
    std::swap(reordered.rows["nodes"][0],reordered.rows["nodes"][2]);
    for(auto& row:reordered.paths)row.local_uid+=100;
    for(auto& row:reordered.nodes)row.local_uid+=100;
    need(rebase(reordered,source,actual,error)&&actual==expected,error);++cases;
    auto invalid=census;invalid.nodes[1].edges.pop_back();
    Snapshot unchanged=source;
    need(!build(invalid,keys,unchanged,error)&&unchanged.rows==source.rows,"nonreciprocal edge accepted");++cases;
    invalid=census;invalid.remote[0].path=0xdead;
    need(!build(invalid,keys,unchanged,error)&&unchanged.rows==source.rows,"foreign remote Path accepted");++cases;
    invalid=census;invalid.paths[1].end=0xdead;
    need(!build(invalid,keys,unchanged,error)&&unchanged.rows==source.rows,"foreign endpoint accepted");++cases;
    auto missing=keys;missing.erase(23);
    need(!build(census,missing,unchanged,error)&&unchanged.rows==source.rows,"missing canonical key accepted");++cases;
    auto duplicate=keys;duplicate[23]=duplicate.at(22);
    need(!build(census,duplicate,unchanged,error)&&unchanged.rows==source.rows,"duplicate canonical key accepted");++cases;
    auto before=census;before.paths.pop_back();before.nodes.pop_back();before.remote.clear();
    mixed_path_identity::Registry registry;
    const std::string snapshot="00000000-0000-0000-0000-000000000001";
    const std::string action="00000000-0000-0000-0000-000000000002";
    need(registry.seed(snapshot,before,error),error);++cases;
    std::vector<mixed_path_identity::Birth> births;
    need(registry.births(action,before,census,births,error)&&births.size()==2&&
         registry.keys().at(12)=="action:"+action+":path:5:0"&&
         registry.keys().at(23)=="action:"+action+":node:0:0",error);++cases;
    need(!registry.births(action,before,census,births,error),"duplicate action Birth accepted");++cases;
    Snapshot history_source;need(build(census,registry.keys(),history_source,error),error);++cases;
    auto restored=census;
    std::swap(restored.nodes[0],restored.nodes[1]);
    restored.nodes[0].member.ref.index=0;restored.nodes[1].member.ref.index=1;
    need(registry.restore(history_source,restored,error),"original restored full UIDs rejected after bucket reorder: "+error);++cases;
    auto changed_uid=restored;changed_uid.nodes[0].member.uid+=uint64_t(1)<<32;
    const auto old_keys=registry.keys();
    need(!registry.restore(history_source,changed_uid,error)&&registry.keys()==old_keys,
         "history invented alias from same low UID or bucket slot");++cases;
    auto wrong_owner=restored;wrong_owner.nodes[0].member.ref.region=2;
    need(!registry.restore(history_source,wrong_owner,error)&&registry.keys()==old_keys,
         "history retained key admitted a different Region");++cases;
    reordered.paths[0].key="foreign";actual={{"unchanged",true}};
    need(!rebase(reordered,source,actual,error)&&actual==Json({{"unchanged",true}}),"foreign history key accepted");++cases;
    std::cout<<cases<<" mixed Path/Intersection canonical topology cases passed\n";
    return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
