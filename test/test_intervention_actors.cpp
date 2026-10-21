#include "intervention_actor_saved.h"
#include "intervention_actor_saved_adopt.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace a=mt2mp::game::interventions::actors;
namespace i=mt2mp::game::interventions;
namespace {
unsigned checks=0;const std::string uuid="11111111-2222-3333-4444-555555555555",sha(64,'a');
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n)bytes[address+n]=source[n];}
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){auto* target=static_cast<unsigned char*>(out);for(size_t n=0;n<size;++n){const auto found=bytes.find(address+n);if(found==bytes.end()){error="fixture unmapped native byte address="+std::to_string(address+n);return false;}target[n]=found->second;}return true;}
    template<class T>bool at(uintptr_t fixture_object,size_t offset,T& value,std::string& error){return copy(fixture_object+offset,&value,sizeof value,error);}
};
constexpr uintptr_t image=0x1000000,fixture_object=0x3000000,fixture_character=0x4000,actor=0x5000,skeleton=0x6000;
constexpr uintptr_t fixture_item=0xb000,prototype=0xc000,fixture_proxy=0xd000,item_data=0xe000;
struct Fixture {
    Memory memory;a::Local owner;
    Fixture(i::Family family){owner.target={family,family==i::Family::Boss?sha+":world:boss:1":family==i::Family::Npc?"action:"+uuid:sha+":world:zone:1:monster:0"};owner.object=fixture_object;owner.uid=0x100000001;owner.scope={uuid,"world",2,3,0x1000,0x1000,0x2000,0x3000};
        const uintptr_t ap=image+(family==i::Family::Boss?0x1743040:family==i::Family::Npc?0x1742980:0x16c0770);memory.put(fixture_object,ap);memory.put(fixture_object+0x18,owner.uid);
        memory.put(fixture_object+0x170,owner.scope.shard);memory.put(fixture_object+0x178,owner.scope.region);memory.put(ap+0x290,image+uintptr_t(family==i::Family::Npc?0x3c2a00:0x39a920));
        memory.put(fixture_object+0x4a8,int32_t(1));memory.put(fixture_object+0x4a4,int32_t(2));memory.put(fixture_object+0x6f8,int32_t(3));memory.put(owner.scope.region+0x834,int32_t(4));
        memory.put(fixture_object+0x100,std::array<float,4>{0,0,0,1});memory.put(fixture_object+0x110,std::array<float,3>{1,2,3});memory.put(fixture_object+0x11c,std::array<float,3>{1,1,1});
        memory.put(fixture_object+0x4ac,uint8_t(0));memory.put(fixture_object+0x368,uint8_t(0));memory.put(fixture_object+0x7f8,int32_t(-1));
        const uintptr_t transform_ap=image+uintptr_t(family==i::Family::Npc?0x1742e58:family==i::Family::Boss?0x1743530:0x16c0c58);
        memory.put(fixture_object+0x30,transform_ap);memory.put(transform_ap+0x38,image+uintptr_t(family==i::Family::Npc?0x3c2de0:0x39e750));
        memory.put(fixture_object+0x6f0,fixture_character);memory.put(fixture_character,actor);memory.put(actor+0x40,skeleton);memory.put(actor+0x20,int32_t(1));std::array<int32_t,a::animation_slots> clips;clips.fill(-1);clips[46]=0;memory.put(fixture_character+8,clips);
        for(size_t n=0;n<3;++n){memory.put(fixture_object+0x730+n*0x28,image+uintptr_t(0x16dd550));memory.put(fixture_object+0x750+n*0x28,std::bit_cast<uint32_t>(100.0f));memory.put(fixture_object+0x754+n*0x28,uint32_t(0));}
        auto links=std::make_shared<a::Links>();links->actors.emplace(fixture_object,a::ActorLink{owner.target,fixture_object,owner.uid});links->actor_keys.emplace(std::pair{uint8_t(family),owner.target.key},fixture_object);
        const std::string ability="character:1:2/ability/0";links->abilities.emplace(prototype,a::AbilityLink{ability,prototype,0x300000001});links->ability_keys.emplace(ability,prototype);owner.links=links;
        for(size_t n=0;n<4;++n){const auto address=fixture_object+0x4e8+n*0x40;memory.put(address,image+uintptr_t(0x1720600));memory.put(address+0x18,int32_t(n));memory.put(address+0x1c,int32_t(10+n));memory.put(address+0x20,float(1+n));memory.put(address+0x24,float(2+n));memory.put(address+0x28,float(3+n));memory.put(address+0x30,n==0?fixture_object:uintptr_t(0));memory.put(address+0x38,n==0?prototype:uintptr_t(0));}
        memory.put(fixture_object+0x7a8,image+uintptr_t(0x16e3c30));memory.put(fixture_object+0x7b0,item_data);memory.put(fixture_object+0x7b8,int32_t(1));memory.put(fixture_object+0x7bc,int32_t(2));memory.put(fixture_object+0x7d8,fixture_object);memory.put(item_data,fixture_item);
        memory.put(fixture_item,image+uintptr_t(0x17437f0));memory.put(fixture_item+0x18,uint64_t(0x400000001));memory.put(fixture_item+0x30,prototype);memory.put(fixture_item+0x38,int32_t(2));memory.put(fixture_item+0x3c,3.5f);memory.put(fixture_item+0x40,image+uintptr_t(0x17117b0));memory.put(fixture_item+0x68,prototype);memory.put(fixture_item+0x70,fixture_proxy);memory.put(fixture_item+0x80,fixture_object);memory.put(fixture_proxy,prototype+0x38);memory.put(fixture_proxy+8,int32_t(1));memory.put(prototype,image+uintptr_t(0x171c930));memory.put(prototype+0x18,uint64_t(0x300000001));
        if(family==i::Family::Npc){memory.put(fixture_object+0x80c,int32_t(1));memory.put(fixture_object+0x838,int32_t(2));memory.put(fixture_object+0x834,int32_t(2));memory.put(fixture_object+0x828,std::bit_cast<uint32_t>(60.0f));memory.put(fixture_object+0x818,std::array<float,3>{4,5,6});memory.put(fixture_object+0x824,2.0f);memory.put(fixture_object+0x82c,3.0f);memory.put(fixture_object+0x830,int32_t(4));memory.put(fixture_object+0x85c,12.0f);}
        if(family==i::Family::Boss){memory.put(fixture_object+0x968,int32_t(3));memory.put(fixture_object+0x96c,int32_t(3));memory.put(fixture_object+0x9ac,std::bit_cast<uint32_t>(600.0f));memory.put(fixture_object+0x970,std::array<float,3>{4,5,6});}
        if(family==i::Family::Monster){memory.put(fixture_object+0x824,int32_t(2));memory.put(fixture_object+0x828,int32_t(3));memory.put(fixture_object+0x910,std::bit_cast<uint32_t>(60.0f));memory.put(fixture_object+0x82c,std::array<float,3>{4,5,6});memory.put(owner.scope.region+0x988,uintptr_t(0x7000));memory.put(0x7000,image+uintptr_t(0x1714100));memory.put(0x7080,owner.scope.region);memory.put(0x7070,uintptr_t(0x8000));memory.put(0x7078,int32_t(1));memory.put(0x707c,int32_t(1));memory.put(0x8000,fixture_object);
            owner.zone=0x9000;owner.zone_uid=0x200000001;owner.zone_index=0;memory.put(fixture_object+0x8d0,owner.zone);memory.put(owner.zone,image+uintptr_t(0x1745ce0));memory.put(owner.zone+0x18,owner.zone_uid);memory.put(owner.zone+0x170,owner.scope.shard);memory.put(owner.zone+0x178,owner.scope.region);memory.put(owner.zone+0x388,uintptr_t(0xa000));memory.put(owner.zone+0x390,int32_t(1));memory.put(owner.zone+0x394,int32_t(1));memory.put(0xa000,fixture_object);}
    }
    bool read(a::Row& row,a::Local& local,std::string& error){auto at=[&](uintptr_t p,size_t o,auto& v,std::string& e){return memory.at(p,o,v,e);};auto copy=[&](uintptr_t p,void* v,size_t n,std::string& e){return memory.copy(p,v,n,e);};
        auto maximum=[](uintptr_t,int32_t,float& value,std::string&){value=100;return true;};
        auto clip=[](const a::native::Render& render,int32_t semantic,a::Clip& value,std::string&){value={};if(render.indexes[size_t(semantic)]>=0){value.length=std::bit_cast<uint32_t>(1.0f);value.time=std::bit_cast<uint32_t>(-0.25f);value.paused=true;value.speed=std::bit_cast<uint32_t>(0.5f);}return true;};
        auto find=[&](uint64_t uid,uintptr_t& out,std::string&){out=uid==owner.uid?fixture_object:uid==0x300000001?prototype:uid==0x400000001?fixture_item:0;return true;};
        return a::read_row(at,copy,maximum,clip,image,owner,row,local,error)&&a::read_saved(at,copy,find,image,owner,row,local,error);
    }
};
void read_native(){for(auto family:{i::Family::Boss,i::Family::Npc,i::Family::Monster}){Fixture f(family);a::Row row;a::Local local;std::string error;const auto before=f.memory.bytes;
        need(f.read(row,local,error),error);need(f.memory.bytes==before,"Actor native reader wrote simulation/LOD/renderer bytes");need(row.target==f.owner.target&&local.uid==0x100000001&&local.object==fixture_object,"Actor native read lost complete canonical owner/full UID");
        need(row.clips[46].time==std::bit_cast<uint32_t>(-0.25f)&&row.clips[46].paused&&row.clips[46].speed==std::bit_cast<uint32_t>(0.5f),"native signed semantic clip phase was changed");
        need(!row.dead&&!row.on_map&&row.values[0]==std::bit_cast<uint32_t>(100.0f),"Actor independent dead/onMap/attributes were inferred from one another");
        need(row.status_effects[0].user==f.owner.target&&row.status_effects[0].ability=="character:1:2/ability/0"&&row.status_effects[3].parameter_int==13,"Actor original StatusEffectData lost current canonical links or values");
        need(row.abilities==std::vector<a::Item>{{"character:1:2/ability/0",2,std::bit_cast<uint32_t>(3.5f)}}&&local.items.size()==1&&local.items[0].uid==0x400000001,"Actor native AbilitySet lost Item order/state/cooldown/full UID");
        if(family==i::Family::Boss)need(std::get<a::Boss>(row.lifecycle).state==3,"waiting Boss independent lifecycle state was lost");
        if(family==i::Family::Npc){float accumulator=0;need(f.memory.at(fixture_object,0x85c,accumulator,error)&&accumulator==12,"NPC local LOD accumulator was consumed");}
        if(family==i::Family::Monster)need(std::get<a::ZoneMonster>(row.lifecycle).ready,"Zone Monster actual ready membership was lost");
        if(family!=i::Family::Monster){const auto original=f.memory.bytes;f.memory.put(fixture_object+0x30,image+uintptr_t(0x16c0c58));f.memory.put(image+0x16c0c58+0x38,image+uintptr_t(0x39e750));need(!f.read(row,local,error)&&error.find("transform")!=std::string::npos,"Monster embedded transform class was accepted for another concrete family");f.memory.bytes=original;}
        error.clear();f.memory.put(fixture_object+0x18,uint64_t(1));need(!f.read(row,local,error)&&error.find("UID")!=std::string::npos,"Actor truncated/stale UID accepted");
    }}
void codec(){std::string error;Fixture npc(i::Family::Npc),boss(i::Family::Boss),monster(i::Family::Monster);a::State state;for(auto* f:{&npc,&boss,&monster}){a::Row row;a::Local local;need(f->read(row,local,error),error);state.push_back(row);}std::sort(state.begin(),state.end(),[](const auto& a,const auto& b){return a.target.key<b.target.key;});
    need(a::valid(state,error),error);const auto wire=a::encode(state);need(a::codec::state(wire)==state,"populated Actor Frame roundtrip changed actual native values");need(a::preflight(state,state,error),error);
    auto rejected=[](const nlohmann::json& value){try{(void)a::codec::state(value);return false;}catch(const std::exception&){return true;}};
    for(const auto& value:{nlohmann::json(true),nlohmann::json(-1),nlohmann::json(1.25),nlohmann::json(uint64_t(UINT32_MAX)+1)}){auto bad=wire;bad[0]["values"][0]=value;need(rejected(bad),"Actor malformed raw float bits admitted");}
    for(const char* key:{"target","character","region","position","rotation","scale","values","recovery","maxima","dead","on_map","action","clips","lifecycle","status_effects","abilities"}){auto bad=wire;bad[0].erase(key);need(rejected(bad),"Actor partial native state admitted");}
    auto bad=wire;bad[0]["local_uid"]=1;need(rejected(bad),"Actor native local UID was accepted on wire");bad=wire;bad[0]["dead"]=0;need(rejected(bad),"Actor nonboolean death accepted");bad=wire;bad[0]["clips"][46][1]=std::bit_cast<uint32_t>(std::numeric_limits<float>::infinity());need(rejected(bad),"Actor nonfinite semantic time accepted");
    bad=wire;bad[0]["lifecycle"].push_back(0);need(rejected(bad),"Actor wrong family lifecycle shape admitted");
    auto changed=state;changed[0].clips[46].length=std::bit_cast<uint32_t>(2.0f);need(!a::preflight(state,changed,error),"Actor local asset mismatch accepted");
    changed=state;changed[0].maxima[0]=std::bit_cast<uint32_t>(200.0f);need(!a::preflight(state,changed,error),"Actor derived maxima mismatch accepted");
    changed[0].status_effects[0].parameter_int+=1;need(a::preflight(state,changed,error),"Actor real changed StatusEffectData could not proceed to native maxima adoption/readback");
    changed=state;changed[0].abilities[0].prototype="character:1:2/ability/1";need(!a::preflight(state,changed,error),"Actor Item membership replacement bypassed original transaction");
    bad=wire;bad[0]["status_effects"][0][5]={{"family",0},{"key","pointer:1234"}};need(rejected(bad),"Actor effect raw peer pointer admitted");
    bad=wire;bad[0]["abilities"][0][0]="character:1:2/ability/00";need(rejected(bad),"Actor ability noncanonical slot admitted");
    bad=wire;bad[0]["abilities"].push_back(bad[0]["abilities"][0]);need(rejected(bad),"Actor duplicate Ability prototype admitted");
    changed=state;changed.push_back(state.back());need(!a::valid(changed,error),"Actor duplicate canonical row accepted");
    changed=state;changed[0].target={i::Family::Toon,"subscriber:1"};need(!a::valid(changed,error),"Economy Toon was duplicated in Actor24 state");
    changed=state;const auto natural=std::find_if(changed.begin(),changed.end(),[](const auto& row){return row.target.family==i::Family::Monster;});natural->target.key="action:"+uuid;need(!a::valid(changed,error),"existing direct Monster was duplicated in Actor24 state");
}
void dungeon_direct(){
    Fixture f(i::Family::Monster);const auto target=i::Target{i::Family::Monster,"action:"+uuid};
    f.owner.target=target;f.owner.scope.scene="action:"+uuid+"/template";f.owner.scope.shard=0x1800;
    f.owner.zone=0;f.owner.zone_uid=0;f.owner.zone_index=-1;
    f.memory.put(fixture_object+0x170,f.owner.scope.shard);f.memory.put(fixture_object+0x8d0,uintptr_t(0));f.memory.put(fixture_object+0x8d8,uintptr_t(0));
    auto links=std::make_shared<a::Links>(*f.owner.links);links->actors.at(fixture_object).target=target;
    links->actor_keys.clear();links->actor_keys.emplace(std::pair{uint8_t(i::Family::Monster),target.key},fixture_object);f.owner.links=links;
    a::Row row;a::Local local;std::string error;const auto before=f.memory.bytes;
    need(f.read(row,local,error),error);need(row.dungeon_scene==f.owner.scope.scene&&row.target==target,"direct Dungeon Monster lost actual current scene/canonical key");
    need(f.memory.bytes==before,"direct Dungeon Monster reader wrote native state");
    need(std::get<a::ZoneMonster>(row.lifecycle).ready,"direct Dungeon Monster ready membership omitted");
    const auto wire=a::encode(a::State{row});need(a::codec::state(wire)==a::State{row},"direct Dungeon populated Actor Frame changed native state");
    auto moving=row;moving.pose.position={6,7,8};moving.pose.orientation={0,1,0,0};
    need(a::preflight(a::State{row},a::State{moving},error),"direct Dungeon motion was treated as stale identity");
    need(a::codec::state(a::encode(a::State{moving}))[0].pose==moving.pose,"fresh Dungeon motion was removed from authoritative publication");
    auto changed=moving;changed.dungeon_scene="action:"+uuid+"/instance/0";
    need(!a::preflight(a::State{row},a::State{changed},error),"direct Dungeon motion crossed current scene owner");
    const auto original=f.owner;
    f.owner.scope.scene="world";need(!f.read(row,local,error),"root direct Monster duplicated Actor24/MonsterLive ownership");f.owner=original;
    f.owner.scope.shard=f.owner.scope.root;need(!a::direct_dungeon_scope(f.owner.target,f.owner.scope),"Dungeon key admitted root physical Shard");f.owner=original;
    f.owner.scope.scene="pointer:1234";need(!a::direct_dungeon_scope(f.owner.target,f.owner.scope),"unconfirmed scene alias admitted");f.owner=original;
    f.memory.put(fixture_object+0x8d8,uintptr_t(1));error.clear();need(!f.read(row,local,error)&&error.find("weak proxy")!=std::string::npos,"direct Dungeon stale Zone proxy admitted");f.memory.bytes=before;
    f.memory.put(fixture_object+0x8d0,uintptr_t(1));need(!f.read(row,local,error),"direct Dungeon foreign Zone receiver admitted");f.memory.bytes=before;
    f.owner.zone_index=0;need(!f.read(row,local,error),"direct Dungeon actor guessed a generated Zone ordinal");f.owner=original;
    f.memory.bytes.erase(fixture_object+0x8d8);error.clear();need(!f.read(row,local,error)&&error.find("fixture unmapped native byte")!=std::string::npos,"Dungeon direct reader erased native ReadMemory cause");
    auto malformed=wire;malformed[0].erase("dungeon_scene");bool rejected=false;try{(void)a::codec::state(malformed);}catch(const std::exception&){rejected=true;}
    need(rejected,"direct Monster wire row omitted current Dungeon scope");
    malformed=wire;malformed[0]["dungeon_scene"]="world";rejected=false;try{(void)a::codec::state(malformed);}catch(const std::exception&){rejected=true;}
    need(rejected,"root direct Monster bypassed its existing live owner through wire scene");
}
void saved_native_guards(){Fixture f(i::Family::Boss);a::Row row;a::Local local;std::string error;const auto before=f.memory.bytes;
    const auto rejected=[&](auto mutate,const char* cause){f.memory.bytes=before;error.clear();mutate();need(!f.read(row,local,error),cause);need(!error.empty(),"Actor saved native guard lost original cause");};
    rejected([&]{f.memory.put(fixture_object+0x4e8,image+uintptr_t(0x1720608));},"Actor effect foreign native class accepted");
    rejected([&]{f.memory.put(fixture_object+0x518,uintptr_t(0x1234));},"Actor effect unknown raw user accepted");
    rejected([&]{f.memory.put(fixture_object+0x520,uintptr_t(0x1234));},"Actor effect unknown raw ability accepted");
    rejected([&]{f.memory.put(fixture_object+0x7d8,uintptr_t(1));},"AbilitySet foreign Actor owner accepted");
    rejected([&]{f.memory.put(fixture_object+0x7bc,int32_t(0));},"AbilitySet truncated physical capacity accepted");
    rejected([&]{f.memory.put(fixture_item+0x18,uint64_t(1));},"Item stale full UID accepted");
    rejected([&]{f.memory.put(fixture_item+0x80,uintptr_t(1));},"Item foreign Actor owner accepted");
    rejected([&]{f.memory.put(fixture_item+0x30,uintptr_t(1));},"Item foreign prototype cast accepted");
    rejected([&]{f.memory.put(fixture_proxy,prototype+0x30);},"Item proxy names another lifetime accepted");
    rejected([&]{f.memory.put(fixture_proxy+8,int32_t(0));},"Item expired weak proxy accepted");
    rejected([&]{f.memory.put(prototype+0x18,uint64_t(1));},"Item stale prototype full UID accepted");
    f.memory.bytes=before;need(f.read(row,local,error),error);need(f.memory.bytes==before,"Actor saved reader changed original physical bytes");
}
void saved_adoption_order(){Fixture f(i::Family::Boss);a::Row row;a::Local local;std::string error;need(f.read(row,local,error),error);
    struct Receipt{bool attempted=false;};struct Backend{
        std::vector<std::string> order;size_t fail=SIZE_MAX;bool step(const std::string& value,std::string& error){order.push_back(value);if(order.size()==fail){error="fixture retained physical CAS failure";return false;}return true;}
        bool status_effect(const a::Row&,size_t n,const a::StatusEffect&,Receipt&,std::string& error){return step("status"+std::to_string(n),error);}
        bool maxima(const a::Row&,const std::array<uint32_t,3>&,std::string& error){return step("maxima",error);}
        bool item(const a::Row&,size_t n,const a::Item&,Receipt&,std::string& error){return step("item"+std::to_string(n),error);}
    } backend;Receipt receipt;
    need(a::adopt_saved(backend,row,row,receipt,error),error);need(backend.order==std::vector<std::string>{"status0","status1","status2","status3","maxima","item0"},"Actor saved adoption did not finish native derived maxima before Item/current scalar writes");
    backend={};backend.fail=3;need(!a::adopt_saved(backend,row,row,receipt,error)&&backend.order.size()==3&&error=="fixture retained physical CAS failure","Actor saved adoption continued after failed original scalar write");
}
void births(){namespace b=mt2mp::game::boss_identity;namespace r=mt2mp::game::character_boss_retirement_read;const r::Pool before{1,2,3,{{4,0x100000001}}};auto after=before;after.members.push_back({5,0x200000001});b::Member created;std::string error;
    need(b::born(before,after,5,created,error)&&created==after.members.back(),error);need(!b::born(before,after,4,created,error),"Boss returned an existing object but birth confirmed");auto wrong=after;wrong.members[0].uid=1;need(!b::born(before,wrong,5,created,error),"Boss birth changed prior full UID");wrong=after;wrong.members.push_back({6,3});need(!b::born(before,wrong,5,created,error),"Boss multiple native births accepted as one");wrong=after;wrong.manager=8;need(!b::born(before,wrong,5,created,error),"Boss birth changed actual owning manager");wrong=after;wrong.members.back().uid=before.members.front().uid;need(!b::born(before,wrong,5,created,error),"Boss birth reused existing full UID");}
}
int main()try{read_native();codec();dungeon_direct();saved_native_guards();saved_adoption_order();births();std::cout<<checks<<" Actor24 original native reader, saved effects/items, independent lifecycle, strict codec and Boss birth assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
