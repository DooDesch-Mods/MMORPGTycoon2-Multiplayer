#include "network_object_identity.h"
#include "monster_snapshot_companion.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
namespace n=network_object_identity;
namespace a=network_snapshot_alias;
int checks=0;
void require(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
int main()try{
    const std::string id="aa111111-1111-4111-8111-111111111111",digest(64,'a');
    const auto node=n::birth(id,n::Kind::Node),pipe=n::birth(id,n::Kind::Pipe);
    const auto saved=n::saved(digest,n::Kind::Node,37);
    require(n::key(node)&&n::key(pipe)&&n::key(saved),"canonical birth and saved keys");
    for(const auto& bad:{saved+"x",digest+":world:network:node:0",digest+":world:network:node:01",digest+":world:network:node:2147483648",node+":x"})require(!n::key(bad),"invalid key rejected");
    n::Registry registry;std::string e,key;n::Entry out;
    n::Scope origin{id,3,7,0x1000};n::Entry first{n::Kind::Node,0x2000,0x3000,0x4000,701,63,false};
    n::Entry second{n::Kind::Pipe,0x2010,0x3000,0x4000,702,63,false};
    require(registry.seal(origin,{{node,first}},{},e),"complete initial seal");
    auto other=origin;++other.generation;
    require(!registry.lookup(other,node,out,e)&&out==n::Entry{},"wrong current generation returns no pointer");
    require(registry.resolve(origin,first.pointer,first.uid,key,e)&&key==node,"full UID and pointer resolve");
    require(!registry.resolve(origin,first.pointer,first.uid+1,key,e)&&key.empty(),"stale UID rejected");
    require(!registry.append(origin,pipe,first,e),"kind cannot relabel node as pipe");
    auto invalid=second;invalid.kind=n::Kind(3);require(!registry.append(origin,pipe,invalid,e),"unknown native family rejected");
    invalid=second;invalid.uid=first.uid;require(!registry.append(origin,pipe,invalid,e),"full UID alias rejected");
    require(registry.append(origin,pipe,second,e),"same action can create a node and pipe");
    require(!registry.retire(origin,node,first.pointer,first.uid+1,e),"retirement UID remains strict");
    require(registry.retire(origin,node,first.pointer,first.uid,e),"confirmed absence can retire exact identity");
    require(!registry.lookup(origin,node,out,e)&&!registry.append(origin,node,first,e),"no tombstone resurrection");
    std::vector<std::pair<std::string,n::Entry>> live;std::set<std::string> retired;
    require(registry.enumerate(origin,live,retired,e)&&live.size()==1&&retired==std::set<std::string>{node},"complete active and tombstone set");
    n::Registry reloaded;auto local=origin;local.native_origin="bb111111-1111-4111-8111-111111111111";++local.epoch;
    second.pointer+=0x100;second.uid=990;
    require(reloaded.seal(local,{{pipe,second}},retired,e)&&reloaded.lookup(local,pipe,out,e)&&out.uid==990,"new native origin preserves canonical key with local full UID");
    a::Manifest aliases{true,{{n::Kind::Node,37,node},{n::Kind::Pipe,38,pipe}},{}};
    require(a::valid(aliases,e),"complete sorted alias manifest");
    a::Manifest decoded;require(a::decode(a::encode(aliases),decoded,e)&&decoded==aliases,"actual alias codec roundtrip");
    auto bad=aliases;bad.rows[1].link=37;require(!a::valid(bad,e),"SaveContext link unique across native kinds");
    bad=aliases;bad.rows[1].key=node;require(!a::valid(bad,e),"kind and canonical key unique");
    bad=aliases;bad.retired.insert(node);require(!a::valid(bad,e),"manifest cannot promise live retired birth");
    auto j=a::encode(aliases);j["rows"][0]["extra"]=0;require(!a::decode(j,decoded,e),"extra row member rejected");
    j=a::encode(aliases);j["retired"]={node,node};require(!a::decode(j,decoded,e),"duplicate tombstones rejected");
    monster_snapshot_companion::Manifest companion{digest,123,{},{}};std::string bytes;
    require(monster_snapshot_companion::encode(companion,bytes,e)&&nlohmann::json::parse(bytes)["revision"]==1,"legacy companion preserved");
    monster_snapshot_companion::Manifest read;
    require(monster_snapshot_companion::decode(bytes,digest,123,read,e)&&read==companion,"legacy companion still loads");
    companion.network=aliases;
    require(monster_snapshot_companion::encode(companion,bytes,e)&&nlohmann::json::parse(bytes)["revision"]==2,"same acknowledged attachment carries network aliases");
    require(monster_snapshot_companion::decode(bytes,digest,123,read,e)&&read==companion,"revision2 exact native and canonical aliases");
    require(!monster_snapshot_companion::decode(bytes,std::string(64,'b'),123,read,e),"different native file rejected");
    require(!monster_snapshot_companion::decode(bytes,digest,124,read,e),"different native bytes rejected");
    registry.invalidate();require(!registry.lookup(origin,pipe,out,e),"retired world invalidates all local native pointers");
    std::cout<<checks<<" Network canonical identity/alias assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"Network identity failure: "<<e.what()<<'\n';return 1;}
