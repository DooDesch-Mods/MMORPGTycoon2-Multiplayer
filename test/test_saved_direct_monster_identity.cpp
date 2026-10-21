#include "monster_saved_identity.h"
#include "direct_monster_identity.h"
#include "monster_snapshot_companion.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace fixture {
namespace saved=mt2mp::game::monster_saved_identity;
namespace identity=mt2mp::game::direct_monster_identity;
namespace side=mt2mp::game::monster_snapshot_companion;
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* output,size_t size,std::string& e){auto* p=static_cast<unsigned char*>(output);for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){e="fixture read absent";return false;}p[i]=found->second;}return true;}
};
constexpr uintptr_t image=0x140000000,manager=0x200000,buffer=0x300000,first=0x400000,second=0x500000,context=0x600000;
constexpr uint64_t first_uid=0x100000007,second_uid=0x200000007;
const std::string action="11111111-1111-4111-8111-111111111111",digest(64,'a'),older(64,'b');
void observer(){
    Memory m;std::string e;const auto prop=image+0x13bf500;
    m.put(prop,image+0x170bf10);m.put(prop+0x38,uint64_t(0x38));m.put(manager,image+0x1714100);
    m.put(manager+0x38,image+0x16e8b60);m.put(manager+0x40,buffer);m.put(manager+0x48,int32_t(2));m.put(manager+0x4c,int32_t(2));
    m.put(buffer,first);m.put(buffer+8,second);
    for(const auto object:{first,second}){m.put(object,image+0x16c0770);m.put(object+8,context);}
    m.put(first+0x18,first_uid);m.put(second+0x18,second_uid);
    auto links=[](uintptr_t p,int32_t& id,std::string&){id=p==first?17:0;return true;};
    saved::SaveFrame frame;saved::Pool out;
    need(saved::begin_save(m,image,prop,manager,0x700000,context,links,frame,e),e);
    need(saved::finish_save(m,image,frame,prop,manager,0x700000,context,true,links,out,e),e);
    need(out.records.size()==2&&out.records[1].child==1,"original ordinal excludes a preceding member");
    need(saved::key(digest,67,out.records[0])==digest+":world:monster:17","linked record key");
    need(saved::key(digest,67,out.records[1])==digest+":world:monster:inline:region{67}/monsterManager/monster[1]","inline original record key");
    m.put(buffer,second);m.put(buffer+8,first);
    need(!saved::finish_save(m,image,frame,prop,manager,0x700000,context,true,links,out,e),"save reorder accepted");
    m.put(buffer,first);m.put(buffer+8,second);
    m.put(second+0x18,first_uid);need(!saved::observe(m,image,manager,links,out,e),"duplicate full UID accepted");m.put(second+0x18,second_uid);
    need(!saved::finish_save(m,image,frame,prop,manager,0x700001,context,true,links,out,e),"foreign writer accepted");
    m.put(manager+0x48,int32_t(0));saved::LoadFrame load;
    need(saved::begin_load(m,image,prop,manager,0x700000,context,load,e),e);
    need(!load.children(saved::load_children_return_rva+1,load.reader,"monster",2,2,e),"foreign caller accepted");
    need(load.children(saved::load_children_return_rva,load.reader,"monster",2,2,e),e);
    need(saved::registered(m,image,load,saved::load_link_return_rva,context,17,first,e),e);
    need(!saved::registered(m,image,load,saved::load_inline_return_rva,context+1,0,second,e),"foreign context accepted");
    need(!load.finish(m,image,out,e),"missing original registration accepted");
    need(saved::registered(m,image,load,saved::load_inline_return_rva,context,0,second,e),e);
    m.put(manager+0x48,int32_t(2));need(load.finish(m,image,out,e),e);
    need(out.records==frame.before.records,"save/load original records disagree");
    m.put(buffer,second);m.put(buffer+8,first);need(!load.finish(m,image,out,e),"load reorder accepted");
}
void restoration(){
    std::string e,key;identity::Registry registry;
    identity::Scope host{"host-origin",1,2,10,20,30},client{"client-origin",4,9,100,200,300};
    const auto record=digest+":world:monster:17",legacy=older+":world:monster:inline:region{67}/monsterManager/monster[2]";
    need(registry.stage(action,host,40,first_uid,e),e);
    need(!registry.capture_key(host,40,first_uid,record,key,e),"pending birth became saved key");
    need(registry.confirm(action,e),e);need(registry.capture_key(host,40,first_uid,record,key,e)&&key=="action:"+action,"old action key lost on save");
    std::vector<std::pair<std::string,identity::Entry>> rows{{key,{client,400,second_uid,true,false}},{legacy,{client,500,0x300000007,true,false}}};
    need(registry.restore(client,rows,e),e);identity::Entry value;
    need(!registry.lookup(action,host,value,e),"old generation remains bound");
    need(registry.lookup(action,client,value,e)&&value.uid==second_uid&&value.pointer==400,"local full UID not restored");
    need(registry.resolve(client,500,0x300000007,key,e)&&key==legacy,"legacy saved canonical key lost");
    need(!registry.resolve(client,500,7,key,e),"truncated UID admitted");
    auto bad=rows;bad[1].second.uid=bad[0].second.uid;need(!registry.restore(client,bad,e),"duplicate restore committed");
    need(registry.lookup(action,client,value,e)&&value.uid==second_uid,"failed restore changed prior registry");
    need(registry.retire(action,value,e),e);need(!registry.restore(client,rows,e),"same-world tombstone revived");
    auto reloaded=client;++reloaded.generation;for(auto& row:rows){row.second.scope=reloaded;row.second.pointer+=1000;row.second.uid+=0x100000000;}
    need(registry.restore(reloaded,rows,e),e);need(registry.lookup(action,reloaded,value,e),"explicit old snapshot reload did not restore old key in new generation");
}
void scenes(){
    std::string e,key;identity::Registry registry;identity::Entry value;
    identity::Scope root{"origin",1,2,10,20,30};
    need(root.scene=="world"&&identity::root_owned(root)&&identity::valid_scene(root),"existing Root aggregate lost its default scene owner");
    auto dungeon=root;dungeon.region=21;dungeon.manager=31;dungeon.scene="action:"+action+"/instance/0";
    const auto canonical=digest+":world:monster:17";
    need(registry.restore(root,{{canonical,{dungeon,400,first_uid,true,false}}},e),e);
    need(registry.lookup(canonical,dungeon,value,e)&&value.scope.scene==dungeon.scene,"restored canonical Dungeon scope was lost");
    auto foreign=dungeon;foreign.scene="world";need(!registry.lookup(canonical,foreign,value,e),"Root scope borrowed an existing Dungeon receiver/full UID");
    foreign=dungeon;foreign.scene="action:"+action+"/instance/1";need(!registry.resolve(foreign,400,first_uid,key,e),"different current Instance borrowed a canonical Monster");
    need(registry.lookup(canonical,dungeon,value,e)&&registry.retire(canonical,value,e),e);
    need(registry.restore(root,{},e),e);std::vector<std::pair<std::string,identity::Entry>> rows;
    need(registry.enumerate(root.snapshot,root.epoch,root.generation,root.root,rows,e)&&rows.size()==1&&rows[0].second.retired&&rows[0].second.scope.scene==dungeon.scene,
        "same-world retirement forgot the original Dungeon owner or read a replacement actor");
    auto invalid=dungeon;invalid.scene="pointer:1234";need(!identity::valid_scene(invalid)&&!registry.stage(action,invalid,500,second_uid,e),"unconfirmed scene alias entered the retained identity registry");
    auto reload=dungeon;++reload.generation;auto bad=reload;bad.scene.clear();
    need(!registry.restore(reload,{{canonical,{bad,600,second_uid,true,false}}},e),"empty scene restored as a successful native lifetime");
    need(registry.restore(reload,{{canonical,{reload,600,second_uid,true,false}}},e)&&registry.lookup(canonical,reload,value,e)&&value.uid==second_uid,
        "original explicit snapshot reload failed to rebind the same canonical key in its new proven scene/generation");
}
void companion(){
    std::string e,bytes;side::Manifest out;
    side::Manifest m{digest,241170,{{67,digest+":world:monster:17","action:"+action},{67,digest+":world:monster:18",older+":world:monster:9"}}};
    need(side::encode(m,bytes,e)&&side::decode(bytes,digest,m.native_bytes,out,e)&&out==m,e);
    need(!side::decode(bytes,older,m.native_bytes,out,e),"wrong native SHA accepted");
    need(!side::decode(bytes,digest,m.native_bytes+1,out,e),"wrong native length accepted");
    auto json=nlohmann::json::parse(bytes);json["rows"][0]["pointer"]=123;
    need(!side::decode(json.dump(),digest,m.native_bytes,out,e),"pointer field accepted");
    json=nlohmann::json::parse(bytes);json["revision"]=1.0;
    need(!side::decode(json.dump(),digest,m.native_bytes,out,e),"float revision accepted");
    auto invalid=m;invalid.rows[1].key=invalid.rows[0].key;need(!side::encode(invalid,bytes,e),"canonical alias accepted");
    invalid=m;std::swap(invalid.rows[0],invalid.rows[1]);need(!side::encode(invalid,bytes,e),"noncanonical order accepted");
    m.rows.clear();need(side::encode(m,bytes,e)&&side::decode(bytes,digest,m.native_bytes,out,e)&&out.rows.empty(),"explicit zero-direct companion rejected");
}
}
int main()try{fixture::observer();fixture::restoration();fixture::scenes();fixture::companion();std::cout<<"saved Monster original observer, full UID restoration and companion cases passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
