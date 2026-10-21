#include "party_live.h"
#include "npc_quest_targets.h"
#include "remove_diagnostics.h"
#include <iostream>
#include <algorithm>
#include <compare>
#include <stdexcept>

namespace typed=mt2mp::action_session;
namespace action=typed;
namespace action_session=typed;
namespace dungeon_scene_identity_runtime=mt2mp::game::dungeon_scene_identity_runtime;
namespace q=mt2mp::game::npc_quest_targets;
using World=mt2mp::game::dungeon_scene_identity_runtime::World;
using Raw=mt2mp::game::party_live::Raw;
using Scope=mt2mp::game::party_identity::Scope;
using SceneCensus=mt2mp::game::dungeon_scene_identity_runtime::Census;
using Target=mt2mp::game::remove_diagnostics::LocalTarget;
using json=nlohmann::json;
struct Census {std::map<uintptr_t,std::string> owners;};
struct Sample {struct {World origin;uintptr_t root=0;} world;};
struct ScenePool {mt2mp::game::dungeon_scene_roster::Scene scene;};
struct Fresh {q::SavedPool saved;};
struct Exterior {std::string record;};
struct Memory {};
struct GraphSample {SceneCensus scenes;int palette=0;};
namespace opened {using CurrentWorld=World;}

namespace fixture {
const std::string shared="dfdd2875-e3b0-4998-945f-20ec003ca540";
const std::string local="53aebc01-fd57-45c5-8b76-b45c69d59e9d";
const std::string shared_sha(64,'a'),local_sha(64,'b');
std::string selected=shared,seen_snapshot;
typed::Binding admitted{1,1};bool binding_ok=true,uid_ok=true,read_ok=true,owners_ok=true;
unsigned lookups=0,checks=0;
World actual;
void check(bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);}
}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool fixture_add(Census& out,uintptr_t object,const std::string& key,std::string& error){
    if(!object||key.empty())return fail(error,"Intervention canonical external owner is empty");
    const auto [found,inserted]=out.owners.emplace(object,key);
    return inserted||found->second==key||fail(error,"Intervention native external owner aliases two canonical keys");
}
uint32_t lookup(uintptr_t,uint64_t uid,uintptr_t& found){++fixture::lookups;found=fixture::uid_ok?uintptr_t(uid*16):0;return 0;}
namespace net {std::string current_snapshot_id(){return fixture::selected;}}
namespace typed_action_runtime {
bool network_binding(const std::string& snapshot,typed::Binding& binding,std::string& error){
    if(!fixture::binding_ok||snapshot!=fixture::shared)return fail(error,"original Controller binding failure");
    binding=fixture::admitted;return true;
}
}
namespace native_open_runtime {
bool read_current_world(World& out,std::string& error){if(!fixture::read_ok)return fail(error,"original NativeOpen read failure");out=fixture::actual;return true;}
}
namespace remove_diagnostics {
bool resolve_current_building_key(const std::string& snapshot,uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    fixture::seen_snapshot=snapshot;
    if(snapshot!=fixture::shared||!object||!uid)return fail(error,"original unrevoked shared Building identity absent");key="building:1";return true;
}
}
bool external_owners(Memory&,const typed::Binding& binding,const std::string& snapshot,int,std::map<uintptr_t,std::string>&,std::string& error){
    fixture::seen_snapshot=snapshot;
    return fixture::owners_ok&&snapshot==fixture::shared&&binding==fixture::admitted||fail(error,"original shared external owners failure");
}
namespace cases::quest {
std::string uuid=fixture::shared,sha=fixture::shared_sha;World origin;
uint32_t main_thread=7,current_thread=7;bool host_active=false,client_active=false,healthy_value=true;
uint32_t GetCurrentThreadId(){return current_thread;}
bool healthy(std::string& error){return healthy_value||fail(error,"original Quest owner locked");}
}
namespace fixture {json prop_input;bool prop_read_ok=true;unsigned prop_read_calls=0;}
namespace remove_diagnostics {
bool identity_bindings(json& out,std::string& error){++fixture::prop_read_calls;if(!fixture::prop_read_ok)return fail(error,"original prop hydration failed");out=fixture::prop_input;return true;}
}
namespace live_host_state::apply_timing {
enum class PageBoundary {OriginalCall};
struct PageReadScope {explicit PageReadScope(PageBoundary){}};
}
#include "domain_bodies.inc"
#include "actor_census_scope.inc"

namespace {
World native(const std::string& uuid){World value;value.uuid=uuid;value.sha256=uuid==fixture::shared?fixture::shared_sha:fixture::local_sha;
    value.world_epoch=1;value.generation=1;value.map=reinterpret_cast<void*>(0x1000);value.state=reinterpret_cast<void*>(0x2000);return value;}
typed::Context context(typed::Role role){typed::Context value;value.session=1;value.host_peer=1;value.peer_mask=3;value.role=role;
    value.local_peer=role==typed::Role::Host?1:2;value.snapshot_uuid=fixture::shared;return value;}
json props(){return {{"world_epoch",1},{"world_generation",1},{"bindings",json::array({{
    {"snapshot_id",fixture::shared},{"record_key","respawn:1"},{"local_uid",1097},{"unknown",false},{"revoked",false}}})}};}
Target target(){Target value;value.snapshot_id=fixture::shared;value.record_key="respawn:1";value.world_epoch=1;value.world_generation=1;
    value.uid=1097;value.object=0x4000;value.normal_building=true;return value;}
void has(const std::string& error,const std::string& value){fixture::check(error.find(value)!=std::string::npos,"Diagnostic lost measured identity/lifetime");}
}
void actor_census_checks(){
    const auto ctx=context(typed::Role::Host);const typed::Binding bound{1,1};auto world=native(fixture::local);Census next;std::string error;
    fixture::prop_input=props();fixture::prop_read_calls=0;fixture::prop_read_ok=false;
    fixture::check(cases::actor_only::props(false,ctx,bound,world,next,error)&&fixture::prop_read_calls==0&&next.owners.empty(),"Actor-only current census hydrated unused props");
    ++world.world_epoch;
    fixture::check(!cases::actor_only::props(false,ctx,bound,world,next,error)&&fixture::prop_read_calls==0&&error.find("native lifetime differs")!=std::string::npos,"Actor-only census skipped independent NativeWorld-vs-Binding guard");
    world=native(fixture::local);error.clear();
    fixture::check(!cases::actor_only::props(true,ctx,bound,world,next,error)&&error=="original prop hydration failed"&&fixture::prop_read_calls==1,"default complete census lost original prop failure");
    fixture::prop_read_ok=true;error.clear();
    fixture::check(cases::actor_only::props(true,ctx,bound,world,next,error)&&fixture::prop_read_calls==2&&next.owners.size()==1,"default complete census omitted native prop owner");
    fixture::prop_input["bindings"][0]["unknown"]=true;error.clear();
    fixture::check(!cases::actor_only::props(true,ctx,bound,world,next,error)&&error.find("unknown or foreign")!=std::string::npos,"default complete census suppressed Unknown prop alias");
    fixture::prop_input=props();fixture::prop_input["world_generation"]=2;error.clear();
    fixture::check(!cases::actor_only::props(true,ctx,bound,world,next,error)&&error.find("native lifetime differs")!=std::string::npos,"default complete census lost prop generation guard");
    fixture::prop_input=props();fixture::prop_read_calls=0;
}
namespace actor_order_fixture {
#include "actor_order_comparators.inc"
template<class Key>struct Row {struct Target {uint8_t family=0;Key key;bool operator==(const Target&)const=default;};Target target;bool operator==(const Row&)const=default;};
struct TrackedKey {
    std::string text;inline static unsigned copies=0;
    explicit TrackedKey(std::string value):text(std::move(value)){}
    TrackedKey(const TrackedKey& other):text(other.text){++copies;}
    TrackedKey& operator=(const TrackedKey& other){text=other.text;++copies;return *this;}
    TrackedKey(TrackedKey&&)=default;TrackedKey& operator=(TrackedKey&&)=default;
    auto operator<=>(const TrackedKey&)const=default;
};
void run(){
    using fixture::check;
    const std::vector<std::string> keys{"", "a", "aa", "subscriber:1", "subscriber:10", "subscriber:2", "action:"+fixture::shared,
        fixture::shared_sha+":world:zone:1:monster:0", std::string(4096,'x'), std::string("a\0b",3), std::string("\x80",1)};
    std::vector<Row<std::string>> rows;
    for(uint8_t family=0;family<4;++family)for(const auto& key:keys)rows.push_back({{family,key}});
    for(const auto& a:rows)for(const auto& b:rows)
        check(actual_actor_less(a,b)==previous_actor_less(a,b),"Actor canonical family/key ordering differs from actual previous comparator");
    auto prior=rows,actual=rows;std::reverse(actual.begin(),actual.end());prior=actual;
    std::sort(prior.begin(),prior.end(),previous_actor_less);std::sort(actual.begin(),actual.end(),actual_actor_less);
    check(actual==prior,"Actor census output order changed");
    const Row<TrackedKey> a{{0,TrackedKey(std::string(128,'a'))}},b{{0,TrackedKey(std::string(128,'b'))}};
    TrackedKey::copies=0;check(actual_actor_less(a,b),"Actor comparator reversed same-family keys");
    check(TrackedKey::copies==0,"Actor comparator copied canonical keys");
    TrackedKey::copies=0;check(previous_actor_less(a,b),"Previous comparator fixture reversed keys");
    check(TrackedKey::copies==2,"Previous comparator fixture did not observe its two owning key copies");
}
}
int main(){try{actor_census_checks();actor_order_fixture::run();using fixture::check;std::string error;const typed::Binding binding{1,1};
    const auto host=context(typed::Role::Host);const auto host_world=native(fixture::local);
    for(const auto role:{typed::Role::Host,typed::Role::Client}){
        const auto ctx=context(role);const auto world=native(role==typed::Role::Host?fixture::local:fixture::shared);Census owners;
        fixture::lookups=0;check(cases::props_read(props(),world,ctx,binding,owners,error),"Actual peer shared prop alias rejected");
        check(fixture::lookups==1&&owners.owners.at(1097*16)=="respawn:1","Actual full UID/key path was bypassed");
        owners={};check(cases::release({target()},world,ctx,binding,owners,error),"Release shared prop alias rejected");
        check(cases::instance(ctx,binding,world,error),"Instance rejected distinct local/shared UUID");
        Raw raw;raw.world=world;check(cases::purchase(raw,error),"UI Purchase rejected admitted peer");
        Sample sample;sample.world.origin=world;sample.world.root=0x11c0;SceneCensus scenes;scenes.world=world;
        check(cases::character(scenes,sample,binding,error),"Character rejected actual native scene");
        check(cases::party_scope(sample,binding).snapshot==world.uuid&&cases::monster_scope(sample,binding).snapshot==world.uuid,"Character native identity scopes use shared UUID");
        cases::quest::origin=world;fixture::actual=world;
        check(cases::quest::clone_current(world,error),"Quest clone rejected native/shared split");
        const auto saved=cases::quest::saved_origin(raw);
        check(saved.uuid==fixture::shared&&saved.sha256==fixture::shared_sha&&saved.world_epoch==1&&saved.world_generation==1,"New Quest clone saved origin used local file identity");
    }
    for(unsigned field=0;field<6;++field){auto rows=props();auto world=host_world;Census owners;
        if(field==0)rows["bindings"][0]["snapshot_id"]=fixture::local;
        if(field==1)rows["bindings"][0]["unknown"]=true;
        if(field==2)rows["world_epoch"]=2;
        if(field==3)rows["world_generation"]=2;
        if(field==4)++world.world_epoch;
        if(field==5)++world.generation;
        fixture::lookups=0;error.clear();check(!cases::props_read(rows,world,host,binding,owners,error),"Foreign or unknown prop admitted");
        check(fixture::lookups==0&&owners.owners.empty(),"Rejected prop reached native lookup/publication");
        has(error,"session_snapshot="+fixture::shared);has(error,"native_snapshot="+fixture::local);
    }
    auto revoked=props();revoked["bindings"][0]["revoked"]=true;revoked["bindings"][0]["unknown"]=true;revoked["bindings"][0]["snapshot_id"]=fixture::local;
    Census owners;fixture::lookups=0;check(cases::props_read(revoked,host_world,host,binding,owners,error)&&owners.owners.empty()&&fixture::lookups==0,"Revoked alias regained identity");
    fixture::uid_ok=false;check(!cases::props_read(props(),host_world,host,binding,owners,error),"Missing native full UID accepted");has(error,"external prop full UID lookup differs");fixture::uid_ok=true;
    auto empty_key=props();empty_key["bindings"][0]["record_key"]="";check(!cases::props_read(empty_key,host_world,host,binding,owners,error),"Empty canonical prop key accepted");
    for(unsigned field=0;field<5;++field){auto value=target();
        if(field==0)value.snapshot_id=fixture::local;
        if(field==1)++value.world_epoch;
        if(field==2)++value.world_generation;
        if(field==3)value.uid=0;
        if(field==4)value.normal_building=false;
        error.clear();check(!cases::release({value},host_world,host,binding,owners,error),"Foreign Release target admitted");has(error,"session_snapshot="+fixture::shared);
    }
    for(unsigned field=0;field<3;++field){auto world=host_world;
        if(field==0)world.uuid.clear();if(field==1)++world.world_epoch;if(field==2)++world.generation;
        check(!cases::instance(host,binding,world,error),"Foreign Instance native binding admitted");Raw raw;raw.world=world;
        check(!cases::purchase(raw,error),"Foreign UI native binding admitted");
    }
    Raw raw;raw.world=host_world;fixture::binding_ok=false;error.clear();check(!cases::purchase(raw,error),"UI Controller rejection bypassed");has(error,"original Controller binding failure");fixture::binding_ok=true;
    Sample sample;sample.world.origin=host_world;sample.world.root=0x11c0;SceneCensus scenes;scenes.world=host_world;
    for(unsigned field=0;field<5;++field){auto changed=scenes;
        if(field==0)changed.world.uuid=fixture::shared;if(field==1)++changed.world.world_epoch;if(field==2)++changed.world.generation;
        if(field==3)changed.world.map=nullptr;if(field==4)changed.world.state=nullptr;
        check(!cases::character(changed,sample,binding,error),"Foreign Character native scene admitted");
    }
    cases::quest::origin=host_world;fixture::actual=host_world;
    for(unsigned field=0;field<10;++field){auto world=host_world;fixture::actual=host_world;
        fixture::selected=fixture::shared;cases::quest::current_thread=7;cases::quest::host_active=false;cases::quest::client_active=false;
        if(field==0)world.uuid=fixture::shared;if(field==1)world.sha256=fixture::shared_sha;if(field==2)++world.generation;
        if(field==3)fixture::actual.uuid=fixture::shared;if(field==4)fixture::actual.sha256=fixture::shared_sha;
        if(field==5)fixture::selected=fixture::local;if(field==6)cases::quest::current_thread=8;
        if(field==7)cases::quest::host_active=true;if(field==8)cases::quest::client_active=true;if(field==9)fixture::actual.map=nullptr;
        check(!cases::quest::clone_current(world,error),"Foreign/active Quest clone owner admitted");
    }
    fixture::selected=fixture::shared;fixture::actual=host_world;cases::quest::current_thread=7;cases::quest::host_active=false;cases::quest::client_active=false;
    fixture::read_ok=false;error.clear();check(!cases::quest::clone_current(host_world,error),"Quest native read failure bypassed");has(error,"original NativeOpen read failure");fixture::read_ok=true;
    scenes.world=host_world;scenes.pools.emplace_back();ScenePool scene;scene.scene=scenes.pools.front().scene;
    check(cases::quest::destination(scenes,host_world,scene,error),"Quest scene rejected actual native world");
    for(unsigned field=0;field<4;++field){auto changed=scenes;auto actual=host_world;
        if(field==0)changed.world.uuid=fixture::shared;if(field==1)changed.world.sha256=fixture::shared_sha;
        if(field==2)actual.uuid=fixture::shared;if(field==3)changed.pools.front().scene.owner.exterior_uid=99;
        check(!cases::quest::destination(changed,actual,scene,error),"Foreign Quest scene identity admitted");
    }
    typed::Request request;request.snapshot_uuid=fixture::shared;request.action_uuid="aaaaaaaa-bbbb-cccc-dddd-123456789abc";
    request.action=typed::Action::SetDungeonPalette;request.dungeon_palette.edit=typed::DungeonPaletteEdit::Cells;request.dungeon_palette.stroke.mode=typed::DungeonCellMode::Close;
    json controller={{"pending",true},{"locked",false},{"action_uuid",request.action_uuid}};
    check(cases::cascade(binding,request,raw,controller,error),"Host cell cascade shared lease rejected");
    for(unsigned field=0;field<5;++field){auto changed=request;auto ctl=controller;auto before=raw;
        if(field==0)changed.snapshot_uuid=fixture::local;if(field==1)ctl["pending"]=false;if(field==2)ctl["locked"]=true;
        if(field==3)ctl["action_uuid"]="foreign";if(field==4)++before.world.generation;
        check(!cases::cascade(binding,changed,before,ctl,error),"Foreign or unowned cell cascade admitted");
    }
    request.action=typed::Action::UndoBuilding;request.target_action_uuid="bbbbbbbb-cccc-dddd-eeee-123456789abc";request.canonical_record_key="dungeon-tiles:"+request.target_action_uuid;
    check(cases::restoration(binding,request,raw,controller,error),"Host cell restoration shared lease rejected");
    request.snapshot_uuid=fixture::local;check(!cases::restoration(binding,request,raw,controller,error),"Foreign restoration snapshot admitted");
    Exterior exterior;check(cases::exterior(0x4000,1097,exterior,error)&&fixture::seen_snapshot==fixture::shared,"Dungeon exterior sent local UUID to shared registry");
    Memory memory;GraphSample graph;graph.scenes.world=host_world;
    check(cases::graph(memory,graph,error)&&fixture::seen_snapshot==fixture::shared,"Dungeon graph sent local UUID to shared owners");
    fixture::owners_ok=false;error.clear();check(!cases::graph(memory,graph,error),"Dungeon graph ignored original owner failure");has(error,"original shared external owners failure");
    std::cout<<"PASS "<<fixture::checks<<" native/shared alias checks\n";return 0;
}catch(const std::exception& exception){std::cerr<<"FAIL: "<<exception.what()<<'\n';return 1;}}
