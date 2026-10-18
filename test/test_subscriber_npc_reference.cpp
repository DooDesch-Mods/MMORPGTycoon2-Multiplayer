#include "subscriber_lifecycle/reference_map.h"
#include "subscriber_lifecycle/payload.h"
#include "npc_census.h"
#include "npc_quest_records.h"
#include "region_saved_identity.h"
#include "mt2mp/core/hash.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using Json=nlohmann::json;
namespace ref=mt2mp::subscriber_lifecycle::first_append;
size_t checks=0;
void need(bool value,const std::string& label){++checks;if(!value)throw std::runtime_error(label);}
constexpr uintptr_t fixture_image=0x140000000,fixture_state=0x200000,fixture_map=0x210000,fixture_region=0x300000,fixture_manager=0x400000,fixture_npc=0x500000;
struct Memory:mt2mp::subscriber_lifecycle::Memory {
    std::map<uintptr_t,uint8_t> bytes;uintptr_t failure=0;
    template<class T>void put(uintptr_t p,const T& value){const auto* b=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=b[i];}
    bool read(uintptr_t p,void* value,size_t size,std::string& e)override{
        auto* out=static_cast<uint8_t*>(value);for(size_t i=0;i<size;++i){if(p+i==failure||!bytes.contains(p+i)){e="injected fixture_memory read failure";return false;}out[i]=bytes.at(p+i);}return true;
    }
} fixture_memory;
uint32_t thread=1;uintptr_t module=fixture_image;int saves=0,owner_reads=0;bool owner_failure=false,world_failure=false,session_failure=false,lookup_failure=false;
std::map<uint64_t,uintptr_t> registry;
std::string snapshot="11111111-1111-4111-8111-111111111111";uint64_t epoch=9;
namespace mt2mp::action_session {
struct Context {uint64_t session=7;std::string snapshot_uuid=snapshot;bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=0,native_generation=0;};
Context context;
struct Session{bool current(Context& out,std::string& e){if(session_failure){e="original Session failure";return false;}out=context;return true;}};
Session& selected_session(){static Session value;return value;}
}
namespace mt2mp::building_mixed_identity::opened_file {
struct CurrentWorld {void* game=reinterpret_cast<void*>(0x100);void* mode=reinterpret_cast<void*>(0x200);void* world=reinterpret_cast<void*>(0x300);void* map=reinterpret_cast<void*>(::fixture_map);void* state=reinterpret_cast<void*>(::fixture_state);uint64_t world_epoch=5,generation=6,bytes=100;std::string uuid="22222222-2222-4222-8222-222222222222",sha256=std::string(64,'b');};
}
using World=mt2mp::building_mixed_identity::opened_file::CurrentWorld;World fixture_world;
namespace mt2mp::game::native_open_runtime {bool read_current_world(World& out,std::string& e){if(world_failure){e="original NativeOpen failure";return false;}out=fixture_world;return true;}}
namespace mt2mp::net {std::string current_snapshot_id(){return snapshot;}uint64_t current_world_epoch(){return epoch;}}
namespace mt2mp::game::npc_probe::action_runtime {
struct StateOwner {std::string key;uintptr_t npc=0;std::vector<std::pair<int32_t,uintptr_t>> quests;};
std::vector<StateOwner> owners;
bool state_owners(const action_session::Context&,const action_session::Binding& binding,std::vector<StateOwner>& out,std::string& e){
    ++owner_reads;if(saves||owner_failure){e="original SaveIdle owner failure";return false;}if(binding.native_epoch!=fixture_world.world_epoch||binding.native_generation!=fixture_world.generation){e="original binding failure";return false;}out=owners;return true;
}
}
namespace mt2mp::subscriber_lifecycle::first_append {bool registry_lookup(uintptr_t,uint64_t uid,uintptr_t& out,std::string& e){if(lookup_failure){e="original UID lookup failure";return false;}out=registry.contains(uid)?registry.at(uid):0;return true;}}
// Only the platform providers are fake; the complete Pins class is extracted.
uint32_t fake_thread(){return thread;}
void* fake_module(const void*){return reinterpret_cast<void*>(module);}
void* fake_process(){return nullptr;}
bool fake_read(void*,void* p,void* out,size_t size,size_t* copied){std::string e;const bool okay=fixture_memory.read(reinterpret_cast<uintptr_t>(p),out,size,e);*copied=okay?size:0;return okay;}
uint32_t fake_error(){return 299;}
#define GetCurrentThreadId fake_thread
#define GetModuleHandleW fake_module
#define GetCurrentProcess fake_process
#define ReadProcessMemory fake_read
#define GetLastError fake_error
#include "subscriber_quest_owners.inc"
#include "subscriber_npc_pins.inc"
#undef GetCurrentThreadId
#undef GetModuleHandleW
#undef GetCurrentProcess
#undef ReadProcessMemory
#undef GetLastError
using Pins=mt2mp::game::subscriber_npc_references::Pins;
const std::string saved_key=std::string(64,'a')+":world:npc:2";
const std::string action_key="action:33333333-3333-4333-8333-333333333333";
void setup(const std::string& key=saved_key,uintptr_t object=fixture_npc,uint64_t uid=71){
    fixture_memory={};registry={{uid,object}};thread=1;module=fixture_image;saves=owner_reads=0;owner_failure=world_failure=session_failure=lookup_failure=false;
    snapshot="11111111-1111-4111-8111-111111111111";epoch=9;fixture_world={};mt2mp::action_session::context={};
    mt2mp::game::npc_probe::action_runtime::owners={{key,object,{}}};
    fixture_memory.put(fixture_state,fixture_image+0x16de920);fixture_memory.put(fixture_map,fixture_image+0x1742860);fixture_memory.put(fixture_map+0x1c0,fixture_image+0x174ad20);fixture_memory.put(fixture_map+0x208,int32_t(0));
    fixture_memory.put(fixture_map+0x230,fixture_image+0x16f04e0);fixture_memory.put(fixture_map+0x238,uintptr_t(0x610000));fixture_memory.put(fixture_map+0x240,int32_t(1));fixture_memory.put(fixture_map+0x244,int32_t(1));fixture_memory.put(0x610000,fixture_region);
    fixture_memory.put(fixture_map+0x358,uintptr_t(0x620000));fixture_memory.put(fixture_map+0x360,int32_t(1));fixture_memory.put(fixture_map+0x364,int32_t(1));fixture_memory.put(0x620000,fixture_region);
    fixture_memory.put(fixture_region,fixture_image+0x1787930);fixture_memory.put(fixture_region+0x70,fixture_map+0x1c0);fixture_memory.put(fixture_region+0x834,int32_t(67));fixture_memory.put(fixture_region+0x998,fixture_manager);
    fixture_memory.put(fixture_image+0x164b250,uintptr_t(0x630000));fixture_memory.put(0x630000,std::array<uint64_t,4>{1,2,3,4});fixture_memory.put(fixture_manager+0xc8,fixture_region);fixture_memory.put(fixture_manager+0xdc,uint32_t(1));
    for(int i=0;i<3;++i){const size_t offset=size_t(i)*0x30;fixture_memory.put(fixture_manager+0x40+offset,i?uintptr_t(0):uintptr_t(0x640000));fixture_memory.put(fixture_manager+0x48+offset,int32_t(i?0:1));fixture_memory.put(fixture_manager+0x4c+offset,int32_t(i?0:1));}
    fixture_memory.put(0x640000,object);fixture_memory.put(object,fixture_image+0x1742980);fixture_memory.put(object+0x18,uid);fixture_memory.put(object+0x178,fixture_region);fixture_memory.put(object+0x80c,int32_t(0));fixture_memory.put(object+0x830,int32_t(0));fixture_memory.put(object+0x4a8,int32_t(0));fixture_memory.put(object+0x4a4,int32_t(2));fixture_memory.put(object+0x6f8,int32_t(1));fixture_memory.put(object+0x870,int32_t(0));
    fixture_memory.put(object+0x100,std::array<float,4>{0,0,0,1});fixture_memory.put(object+0x110,std::array<float,3>{1,2,3});fixture_memory.put(object+0x11c,std::array<float,3>{1,1,1});fixture_memory.put(object+0x38,uintptr_t(0x650000));fixture_memory.put(object+0x40,size_t(1));fixture_memory.put(0x650000,char('A'));
}
std::string digest(const Json& value){const auto text=value.dump();return mt2mp::sha256(std::vector<uint8_t>(text.begin(),text.end()));}
Json payload(const std::string& key){
    Json records=Json::array();std::map<size_t,size_t> children;
    const auto add=[&](size_t parent,const char* name,Json tokens){
        const auto index=records.size();records.push_back({{"parent",parent==SIZE_MAX?Json(nullptr):Json(parent)},{"child_index",parent==SIZE_MAX?0:children[parent]++},{"label",{{"type",1},{"text",name}}},{"tokens",tokens}});return index;
    };
    add(SIZE_MAX,"mmoSubscriber",Json::array());add(0,"id",Json::array({{{"type",4},{"bits",uint32_t(122)}}}));
    auto brain=add(0,"brain",Json::array());brain=add(brain,"mmoToonBrainAI",Json::array());brain=add(brain,"needs",Json::array());
    for(const auto* name:{"Health","Interact","Advance","Danger","GetLoot","Scenery","InventorySpace"}){auto need=add(brain,"need",Json::array());add(need,name,Json::array({{{"type",3},{"bits",uint32_t(0)}}}));}
    Json rows=Json::array({{{"saved_link_id",2},{"inline_definition",false},{"class_addresspoint_rva",24390016},{"portable_owner",{{"kind","shared_npc"},{"npc_key",key},{"region_id",-1},{"character_kind",-1},{"character_id",-1},{"ability_index",-1},{"source_snapshot_sha256",std::string(64,'b')}}}}});
    return {{"schema",1},{"origin_sha256",std::string(64,'b')},{"source_record_sha256",digest(records)},{"references_sha256",digest(rows)},{"external_references_sha256",digest(rows)},{"serialized_subscriber_id",122},{"next_subscriber_id",123},{"online",true},{"records",records},{"references",rows}};
}
void rehash(Json& j){j["references_sha256"]=digest(j["references"]);j["external_references_sha256"]=j["references_sha256"];}
constexpr uintptr_t quest0=0x700000,quest_manager=0x710000,quest_table=0x720000,quest_nodes=0x730000;
void setup_quests(const std::string& key=saved_key,uintptr_t npc_object=fixture_npc,uintptr_t quest_object=quest0){
    setup(key,npc_object,npc_object==fixture_npc?71:991);
    const uintptr_t npc_proxy=0x740000,entries=0x750000;
    fixture_memory.put(npc_object+0x28,npc_proxy);fixture_memory.put(npc_proxy,npc_object+0x20);fixture_memory.put(npc_proxy+8,int32_t(3));
    fixture_memory.put(npc_object+0x868,entries);fixture_memory.put(npc_object+0x870,int32_t(2));fixture_memory.put(npc_object+0x874,int32_t(2));
    fixture_memory.put(fixture_image+0x13df410,quest_manager);fixture_memory.put(quest_manager,fixture_image+0x17020b0);
    fixture_memory.put(quest_manager+0x18,quest_table);fixture_memory.put(quest_manager+0x20,int32_t(2));fixture_memory.put(quest_manager+0x24,int32_t(31));
    for(size_t bucket=0;bucket<2;++bucket)fixture_memory.put(quest_table+bucket*24+0x10,uintptr_t(0));
    auto& owner=mt2mp::game::npc_probe::action_runtime::owners[0];
    for(int32_t i=0;i<2;++i){
        const uintptr_t q=quest_object+uintptr_t(i)*0x1000,proxy=0x760000+uintptr_t(i)*0x100,node=quest_nodes+uintptr_t(i)*0x100;
        owner.quests.emplace_back(i,q);fixture_memory.put(entries+size_t(i)*8,q);
        fixture_memory.put(q,fixture_image+0x174ac10);fixture_memory.put(q+0x20,proxy);fixture_memory.put(proxy,q+0x18);fixture_memory.put(proxy+8,int32_t(1));
        fixture_memory.put(q+0x84,i);fixture_memory.put(q+0xb8,npc_object);fixture_memory.put(q+0xc0,npc_proxy);
        const uint32_t hash=uint32_t(q);const size_t bucket=size_t((hash*0x9e3779ffu)>>31);
        uintptr_t previous=0;std::string error;need(fixture_memory.read(quest_table+bucket*24+0x10,&previous,sizeof previous,error),"test Quest hash bucket initialized");
        fixture_memory.put(node,q);fixture_memory.put(node+8,hash);fixture_memory.put(node+0x10,previous);fixture_memory.put(quest_table+bucket*24+0x10,node);
    }
}
Json quest_payload(const std::string& key,int32_t sequence=0){
    auto j=payload(key);auto& row=j["references"][0];row["class_addresspoint_rva"]=uint32_t(0x174ac10);
    row["portable_owner"]["kind"]="shared_quest";row["portable_owner"]["sequence_id"]=sequence;rehash(j);return j;
}
void quest_proxy_checks(){
    std::string e,key;int32_t sequence=-1;uintptr_t object=0;
    for(bool absent:{false,true}){
        setup_quests();
        if(absent)fixture_memory.put(quest0+0x20,uintptr_t(0));
        else fixture_memory.put(0x760008,int32_t(0));
        Pins pin;need(pin.prepare(fixture_image,fixture_world,e),"unreferenced Quest preparation accepts absent/live-zero proxy: "+e);saves=1;
        need(pin.identify_quest(quest0+0x1000,key,sequence,e),"unreferenced sibling does not block Host referenced Quest: "+e);
        need(!pin.identify_quest(quest0,key,sequence,e)&&e.find("required>0")!=std::string::npos,"Host selected Quest still requires actual Weak holder");
        need(pin.resolve_quest(saved_key,0,object,e)&&object==quest0&&pin.verify_used(e),"Client borrowed target may precede original Weak assignment: "+e);
        need(!pin.verify_quest_references(e),"post-Resolve acceptance cannot succeed before holder exists");
        if(absent)fixture_memory.put(quest0+0x20,uintptr_t(0x760000));
        fixture_memory.put(0x760008,int32_t(1));
        need(pin.verify_quest_references(e)&&pin.verify_used(e),"original lazy proxy allocation and first reference are accepted: "+e);
        fixture_memory.put(0x760008,int32_t(0));
        need(pin.verify_used(e)&&pin.resolve_quest(saved_key,0,object,e),"native cached live proxy at zero remains owned: "+e);
        need(!pin.verify_quest_references(e),"positive post-import holder requirement remains separate");
        fixture_memory.put(quest0+0x20,uintptr_t(0x770000));fixture_memory.put(0x770000,quest0+0x18);fixture_memory.put(0x770008,int32_t(1));
        need(!pin.resolve_quest(saved_key,0,object,e)&&!object&&!pin.verify_used(e),"newly allocated proxy cannot later be replaced");
    }
    for(int invalid=0;invalid<3;++invalid){
        setup_quests();
        if(invalid==0)fixture_memory.put(0x760000,uintptr_t(0));
        if(invalid==1)fixture_memory.put(0x760000,quest0+0x20);
        if(invalid==2)fixture_memory.put(0x760008,int32_t(-1));
        Pins pin;need(!pin.prepare(fixture_image,fixture_world,e),"expired, foreign or negative-count proxy is not an unused live proxy");
        if(invalid==0)need(e.find("target_present=0 target_matches_owner=0")!=std::string::npos,"expired target diagnosis preserves condition without process addresses");
        if(invalid==2)need(e.find("refcount invalid actual=-1 required>=0")!=std::string::npos,"negative refcount has its measured distinct cause");
    }
}
void quest_checks(){
    quest_proxy_checks();
    std::string e,key;int32_t sequence=-1;uintptr_t object=0;ref::ReferenceKey wire;
    for(const auto& canonical:{saved_key,action_key}){
        setup_quests(canonical);Pins host;need(host.prepare(fixture_image,fixture_world,e),"Quest Host prepare: "+e);saves=1;
        ref::ReferenceWorld w{fixture_image,fixture_state,fixture_map,nullptr,&host};
        need(ref::identify_reference(fixture_memory,w,quest0,0,2,wire,e)&&wire.kind==ref::ReferenceKind::SharedQuest&&wire.npc_key==canonical&&wire.sequence_id==0,"Quest AP maps existing NPC/sequence identity: "+e);
        need(ref::resolve_reference(fixture_memory,w,wire,object,e)&&object==quest0&&host.verify_used(e)&&owner_reads==1,"Quest held-scope identity roundtrip without SaveIdle call: "+e);
        ref::ReferenceKey second;need(ref::identify_reference(fixture_memory,w,quest0+0x1000,0,2,second,e)&&second.sequence_id==1&&second!=wire,"two quests retain distinct portable keys");
        auto j=quest_payload(canonical);ref::Payload parsed;need(ref::parse_payload(j,parsed,e)&&parsed.external.size()==1&&parsed.external[0].key==wire,"Quest payload parser roundtrip: "+e);
        setup_quests(canonical,fixture_npc+0x10000,quest0+0x100000);Pins client;need(client.prepare(fixture_image,fixture_world,e),"Quest Client prepare: "+e);saves=1;w.npcs=&client;
        need(ref::resolve_reference(fixture_memory,w,wire,object,e)&&object==quest0+0x100000&&client.verify_used(e),"Quest same wire identity maps distinct Client native objects: "+e);
        need(!client.resolve_quest(canonical,9,object,e)&&!object,"absent Quest sequence never falls back");
    }
    for(int bad=0;bad<16;++bad){
        setup_quests();Pins pin;need(pin.prepare(fixture_image,fixture_world,e),"Quest negative prepare: "+e);saves=1;
        need(pin.identify_quest(quest0,key,sequence,e),"Quest negative selects used pin: "+e);
        switch(bad){
        case 0:fixture_memory.put(quest0+0x84,int32_t(2));break;
        case 1:fixture_memory.put(0x750000,quest0+0x1000);fixture_memory.put(0x750008,quest0);fixture_memory.put(quest0+0x84,int32_t(1));fixture_memory.put(quest0+0x1084,int32_t(0));break;
        case 2:fixture_memory.put(0x750000,quest0+0x2000);break;
        case 3:fixture_memory.put(quest0+0xb8,fixture_npc+1);break;
        case 4:fixture_memory.put(0x740000,uintptr_t(0));break;
        case 5:fixture_memory.put(0x760000,uintptr_t(0));break;
        case 6:fixture_memory.put(quest0+0x20,uintptr_t(0x770000));fixture_memory.put(0x770000,quest0+0x18);fixture_memory.put(0x770008,int32_t(1));break;
        case 7:for(size_t bucket=0;bucket<2;++bucket)fixture_memory.put(quest_table+bucket*24+0x10,uintptr_t(0));break;
        case 8:fixture_memory.put(fixture_image+0x13df410,quest_manager+0x10000);break;
        case 9:fixture_memory.put(quest0+0x1084,int32_t(0));break;
        case 10:fixture_memory.put(0x750008,quest0);break;
        case 11:fixture_memory.failure=quest0+0x84;break;
        case 12:fixture_world.generation++;break;
        case 13:fixture_memory.put(quest0,fixture_image+0x1742980);break;
        case 14:fixture_memory.put(0x760008,int32_t(-1));break;
        case 15:fixture_memory.put(fixture_npc+0x870,int32_t(1));break;
        }
        need(!pin.resolve_quest(saved_key,0,object,e)&&!object&&!e.empty(),"Quest changed owner fails before native borrowed lookup");
        need(!pin.verify_used(e),"Quest used-pin scope exit rejects change");saves=0;need(!pin.verify_used(e),"Quest closed scope rejects change");
    }
    for(int bad=0;bad<12;++bad){
        auto j=quest_payload(saved_key);auto& owner=j["references"][0]["portable_owner"];
        switch(bad){case 0:owner.erase("sequence_id");break;case 1:owner["sequence_id"]=-1;break;case 2:owner["sequence_id"]=uint64_t(2147483648);break;case 3:owner["sequence_id"]=true;break;case 4:owner["sequence_id"]=1.0;break;case 5:owner["npc_key"]=action_key+":0";break;case 6:owner["subscriber_id"]=2;break;case 7:owner["ability_index"]=0;break;case 8:owner["building_key"]=action_key;break;case 9:owner["kind"]="shared_npc";break;case 10:owner["kind"]="root_shard";break;case 11:owner["source_snapshot_sha256"]=std::string(64,'c');break;}
        rehash(j);ref::Payload parsed;need(!ref::parse_payload(j,parsed,e)&&!e.empty(),"Quest malformed or foreign payload rejected");
    }
    setup_quests();Pins pin;need(pin.prepare(fixture_image,fixture_world,e),"Quest final prepare");ref::ReferenceWorld w{fixture_image,fixture_state,fixture_map,nullptr,&pin};
    need(ref::identify_reference(fixture_memory,w,quest0,0,2,wire,e),"Quest final identify");wire.subscriber_id=5;need(!ref::resolve_reference(fixture_memory,w,wire,object,e),"Quest resolver rejects foreign Subscriber field");
    wire.subscriber_id=-1;wire.kind=ref::ReferenceKind::SharedNPC;need(!ref::resolve_reference(fixture_memory,w,wire,object,e),"NPC resolver rejects foreign Quest sequence");
    w.npcs=nullptr;need(!ref::identify_reference(fixture_memory,w,quest0,0,2,wire,e)&&e=="Quest reference has no pinned shared bindings","Quest missing bindings explicit");
    fixture_memory.put(quest0,fixture_image+0x1234);need(!ref::identify_reference(fixture_memory,w,quest0,0,2,wire,e)&&e=="external reference has no supported portable owner key","unknown native type remains unsupported");
}
int main(){try{
    quest_checks();
    std::string e,key;uintptr_t object=0;ref::ReferenceKey wire;
    for(const auto& canonical:{saved_key,action_key}){
        setup(canonical);Pins host;need(host.prepare(fixture_image,fixture_world,e),"prepare Host: "+e);saves=1;
        ref::ReferenceWorld w{fixture_image,fixture_state,fixture_map,nullptr,&host};
        need(ref::identify_reference(fixture_memory,w,fixture_npc,0,2,wire,e)&&wire.kind==ref::ReferenceKind::SharedNPC&&wire.npc_key==canonical,"real AP maps canonical NPC key: "+e);
        need(ref::resolve_reference(fixture_memory,w,wire,object,e)&&object==fixture_npc&&host.verify_used(e)&&owner_reads==1,"held SaveScope uses fresh pins without SaveIdle owner reader: "+e);
        fixture_memory.put(fixture_npc+0x110,std::array<float,3>{9,8,7});need(host.resolve(canonical,object,e),"pose does not invent a new NPC identity");
        setup(canonical,fixture_npc+0x10000,991);Pins client;need(client.prepare(fixture_image,fixture_world,e),"prepare Client: "+e);saves=1;w.npcs=&client;
        need(ref::resolve_reference(fixture_memory,w,wire,object,e)&&object==fixture_npc+0x10000,"same wire key resolves different local receiver/UID: "+e);
        ref::Payload parsed;auto j=payload(canonical);need(ref::parse_payload(j,parsed,e)&&parsed.external.size()==1&&parsed.external[0].key==wire,"actual payload canonical NPC roundtrip: "+e);
        need(j.dump().find("991")==std::string::npos,"local UID absent from wire");
    }
    for(int bad=0;bad<15;++bad){setup();Pins p;need(p.prepare(fixture_image,fixture_world,e),"negative prepare: "+e);saves=1;need(p.identify(fixture_npc,key,e),"negative selected pin: "+e);
        switch(bad){case 0:registry[71]=0;break;case 1:registry[71]=fixture_npc+1;break;case 2:fixture_memory.put(fixture_npc,fixture_image+0x1744e90);break;case 3:fixture_memory.put(fixture_npc+0x18,uint64_t(72));break;case 4:fixture_memory.put(fixture_npc+0x178,fixture_region+1);break;case 5:fixture_memory.put(fixture_manager+0xc8,fixture_region+1);break;case 6:fixture_memory.put(fixture_manager+0x48,int32_t(0));break;case 7:fixture_memory.put(fixture_npc+0x4a4,int32_t(3));break;case 8:fixture_world.generation++;break;case 9:mt2mp::action_session::context.session++;break;case 10:thread++;break;case 11:fixture_memory.failure=fixture_npc+0x18;break;case 12:lookup_failure=true;break;}
        if(bad==13)fixture_memory.put(fixture_npc+0x80c,int32_t(1));
        if(bad==14){for(uintptr_t n=0;n<0xe0;++n)if(fixture_memory.bytes.contains(fixture_manager+n))fixture_memory.bytes[fixture_manager+0x1000+n]=fixture_memory.bytes.at(fixture_manager+n);fixture_memory.put(fixture_region+0x998,fixture_manager+0x1000);}
        need(!p.resolve(saved_key,object,e)&&object==0&&!e.empty(),"changed owner must fail before borrowed lookup");need(!p.verify_used(e),"used owner closure detects same failure");
        saves=0;need(!p.verify_used(e),"used owner post-SaveScope closure retains failure");
        if(bad==12)need(e=="original UID lookup failure","preserve original lookup cause");
    }
    setup();Pins p;owner_failure=true;need(!p.prepare(fixture_image,fixture_world,e)&&e=="original SaveIdle owner failure","original prepare failure retained");
    setup();saves=1;need(!p.prepare(fixture_image,fixture_world,e),"prepare never weakens SaveIdle");
    setup();mt2mp::game::npc_probe::action_runtime::owners.push_back({action_key,fixture_npc,{}});need(!p.prepare(fixture_image,fixture_world,e),"duplicate canonical ownership rejected");
    setup();need(p.prepare(fixture_image,fixture_world,e),"unknown pin preparation");need(!p.identify(fixture_npc+1,key,e)&&!p.resolve(action_key,object,e),"unknown NPC key/receiver has no fallback");
    ref::ReferenceWorld no_pins{fixture_image,fixture_state,fixture_map};need(!ref::identify_reference(fixture_memory,no_pins,fixture_npc,0,2,wire,e)&&e=="NPC reference has no pinned shared bindings","exact missing NPC owner cause");
    for(int bad=0;bad<8;++bad){auto j=payload(saved_key);auto& owner=j["references"][0]["portable_owner"];
        switch(bad){case 0:owner.erase("npc_key");break;case 1:owner["npc_key"]=std::string(64,'a')+":world:building:2";break;case 2:owner["region_id"]=67;break;case 3:owner["character_kind"]=0;break;case 4:owner["subscriber_id"]=1;break;case 5:owner["building_key"]=action_key;break;case 6:owner["kind"]="root_shard";break;case 7:owner["source_snapshot_sha256"]=std::string(64,'c');break;}
        rehash(j);ref::Payload parsed;need(!ref::parse_payload(j,parsed,e)&&!e.empty(),"actual malformed/foreign NPC key payload rejected");
    }
    setup();need(p.prepare(fixture_image,fixture_world,e),"final prepare");ref::ReferenceWorld w{fixture_image,fixture_state,fixture_map,nullptr,&p};need(ref::identify_reference(fixture_memory,w,fixture_npc,0,2,wire,e),"final identify");wire.subscriber_id=5;need(!ref::resolve_reference(fixture_memory,w,wire,object,e),"NPC borrowed resolve rejects foreign Subscriber ID");
    std::cout<<checks<<" actual Subscriber NPC reference checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failed after "<<checks<<": "<<x.what()<<'\n';return 1;}}
