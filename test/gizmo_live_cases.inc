#include "dungeon_gizmo_live_read.h"
namespace gizmo_live_fixture {
namespace live=mt2mp::game::dungeon_gizmo_live;
namespace identity=mt2mp::game::dungeon_gizmo_identity;
constexpr uintptr_t fixture_image=0x10000000,fixture_object=0x21000000,fixture_shard=0x22000000,fixture_region=0x23000000;
constexpr uintptr_t fixture_manager=0x24000000,fixture_definition=0x25000000,fixture_actor=0x26000000;
const std::string fixture_scene="action:11111111-1111-4111-8111-111111111111/template";
struct Bytes {
    std::map<uintptr_t,unsigned char> storage;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n)storage[address+n]=data[n];}
    template<class T>bool at(uintptr_t address,size_t offset,T& value,std::string& error){auto* data=reinterpret_cast<unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n){const auto found=storage.find(address+offset+n);if(found==storage.end()){error="Gizmo fixture native byte unavailable";return false;}data[n]=found->second;}return true;}
    Bytes(){put(fixture_object,fixture_image+0x1749ab0);put(fixture_object+0x18,uint64_t(0x100000077));
        put(fixture_object+0x170,fixture_shard);put(fixture_object+0x178,fixture_region);put(fixture_object+0x250,uint8_t(1));put(fixture_object+0x254,int32_t(2));
        put(fixture_object+0x328,int32_t(1));put(fixture_object+0x2d0,uint8_t(0));put(fixture_object+0x2d8,uintptr_t(0));
        put(fixture_object+0x2e0,fixture_actor);put(fixture_object+0x300,uintptr_t(0x27000000));put(fixture_object+0x278,fixture_definition);put(fixture_definition+0xa0,uint32_t(2));
        put(fixture_actor+0x20,int32_t(16));for(size_t n=0;n<3;++n)put(fixture_object+0x2e8+n*4,int32_t(7+n));}
};
template<class Check>void run(Check& check){
    identity::Member owner{fixture_scene,fixture_object,fixture_manager,fixture_shard,fixture_region,0x100000077};
    const auto key=identity::birth("22222222-2222-4222-8222-222222222222",0);std::string error;live::Row row;live::Local local;
    bool mutate=false;Bytes memory;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.at(object,offset,value,cause);};
    auto clip=[&](uintptr_t actor,int32_t index,live::Clip& value,std::string& cause){if(actor!=fixture_actor||index<7||index>9){cause="Gizmo fixture wrong native clip";return false;}
        value={std::bit_cast<uint32_t>(2.0f),std::bit_cast<uint32_t>(float(index)),false,std::bit_cast<uint32_t>(1.0f)};
        if(mutate){memory.put(fixture_object+0x2e0,uintptr_t(0));mutate=false;}return true;};
    const auto capture=[&]{return live::read_row(read,clip,fixture_image,owner,key,row,local,error);};
    error="previous unrelated failure";check(capture()&&error.empty(),"successful complete read clears stale error state");
    check(capture()&&!row.transition&&row.animation==-1&&row.from==0&&row.to==0&&row.clips[1].time==std::bit_cast<uint32_t>(8.0f),"inactive uninitialized transition cache must never be read");
    check(local.indexes[0]==7&&local.indexes[2]==9&&!row.clips[3].length,"Door semantic indexes are local and unrelated container cache stays unread");
    memory.put(fixture_object+0x2d0,uint8_t(1));memory.put(fixture_object+0x2c4,int32_t(8));memory.put(fixture_object+0x2c8,int32_t(1));memory.put(fixture_object+0x2cc,int32_t(2));
    check(capture()&&row.animation==1&&row.from==1&&row.to==2,"active native Door transition maps its exact local asset to semantic close");
    live::State state{{fixture_scene,0x80000001,{row}}},decoded;
    check(live::decode(live::encode(state),decoded,error)&&decoded==state,"populated Gizmo state retains float bits, lock mask and semantic clips");
    auto json=live::encode(state);json[0]["rows"][0]["triggered"]=1;
    check(!live::decode(json,decoded,error)&&decoded.empty(),"integer cannot replace native boolean");
    json=live::encode(state);json[0]["rows"][0]["uid"]=0x100000077;
    check(!live::decode(json,decoded,error),"pointer or UID wire fields rejected");
    json=live::encode(state);json[0]["rows"][0]["clips"][0][0]=uint32_t(0x7fc00000);
    check(!live::decode(json,decoded,error),"nonfinite semantic asset rejected");
    memory.put(fixture_object+0x18,uint64_t(0x200000077));check(!capture()&&!error.empty(),"matching low UID cannot replace actual full UID");
    memory=Bytes{};memory.put(fixture_object+0x178,fixture_shard);check(!capture(),"foreign reciprocal Region rejected");
    memory=Bytes{};mutate=true;check(!capture()&&error.find("render owner changed")!=std::string::npos,"render replacement during real clip read rejected");
    memory=Bytes{};memory.put(fixture_object+0x2d0,uint8_t(1));memory.put(fixture_object+0x2c4,int32_t(15));memory.put(fixture_object+0x2c8,int32_t(1));memory.put(fixture_object+0x2cc,int32_t(2));
    check(!capture()&&error.find("semantic asset")!=std::string::npos,"unmapped active local animation cannot enter Frame");
    memory=Bytes{};memory.storage.erase(fixture_object+0x328);check(!capture()&&error.find("native byte unavailable")!=std::string::npos,"original failed native read cause retained");
}
}
