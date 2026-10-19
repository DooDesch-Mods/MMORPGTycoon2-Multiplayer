#include "boss_current.h"
#include "native_open_runtime.h"
#include "dungeon_scene_identity_runtime.h"
#include "region_saved_identity.h"
#include "root_saved_building.h"
#include <Windows.h>
#include <algorithm>

namespace mt2mp::game::boss_current {
namespace {
using World=building_mixed_identity::opened_file::CurrentWorld;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Memory {template<class T>bool operator()(uintptr_t object,size_t offset,T& out,std::string& error)const{
    if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-sizeof out)return fail(error,"Boss current native read address null/overflow");
    return root_saved_building::bytes(reinterpret_cast<void*>(object),offset,&out,sizeof out,error);}};
uint32_t lookup(uintptr_t image,uint64_t uid,uintptr_t& found)noexcept{__try{found=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct Find {uintptr_t image;bool operator()(uint64_t uid,uintptr_t& found,std::string& error)const{live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);const auto exception=lookup(image,uid,found);return !exception||fail(error,"Boss current full UID lookup SEH="+std::to_string(exception));}};
bool same(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256;}
bool admitted_characters(const action_session::Context& context,const action_session::Binding& binding,World& world,
                         character_definition_identity::Census& definitions,character_membership_reader::Users& users,std::string& error){
    return npc_probe::character_actor_records::read_current(context,binding,definitions,users,error)&&native_open_runtime::read_current_world(world,error);
}
bool admitted(const action_session::Context& context,const action_session::Binding& binding,World& world,std::string& error){
    character_definition_identity::Census definitions;character_membership_reader::Users users;
    return admitted_characters(context,binding,world,definitions,users,error);
}
bool pools(const World& world,std::vector<Pool>& out,std::string& error){
    live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    out.clear();const auto image=uintptr_t(GetModuleHandleW(nullptr)),map=uintptr_t(world.map),root=map+0x1c0;Memory at;Find find{image};uintptr_t ap=0;
    if(!at(map,0,ap,error)||ap!=image+0x1742860||!at(root,0,ap,error)||ap!=image+0x174ad20||!at(map+0x230,0,ap,error)||ap!=image+0x16f04e0)return fail(error,error.empty()?"Boss current actual NativeOpen map/root classes differ":error);
    std::set<uintptr_t> managers,scene_regions;
    const auto add=[&](uintptr_t shard,uintptr_t region,const std::string& key){Pool pool;uintptr_t manager=0;
        if(!at(region,0x990,manager,error))return false;
        pool.scope={world.uuid,key,world.world_epoch,world.generation,root,shard,region,manager};
        if(!boss_identity::valid(pool.scope)||!managers.insert(manager).second||!scene_regions.insert(region).second)return fail(error,"Boss current scene/manager duplicated or outside completed NativeOpen");
        if(!([&]{live_host_state::apply_timing::PageReadScope boss_pool_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_boss_retirement_read::census(image,shard,region,at,find,pool.native,error);}()))return false;
        if(pool.native.manager!=manager)return fail(error,"Boss current reciprocal manager changed during census");out.push_back(std::move(pool));return true;};
    region_saved_identity::Index region_index;
    if(!region_saved_identity::capture(image,map,root,region_index,at,error))return false;
    for(const auto& row:region_index.saved){
        if(!add(root,row.second,"world"))return fail(error,error.empty()?"Boss current Root Region ID/owner differs":error);
    }
    if(!region_saved_identity::verify(image,map,root,region_index,at,error))return false;
    dungeon_scene_identity_runtime::Census scenes;if(!dungeon_scene_identity_runtime::read_current(scenes,error))return false;
    if(scenes.world.uuid!=world.uuid||scenes.world.sha256!=world.sha256||scenes.world.world!=world.world||scenes.world.map!=world.map||scenes.world.generation!=world.generation||scenes.world.world_epoch!=world.world_epoch)return fail(error,"Boss current Dungeon scenes differ from actual NativeOpen world");
    for(const auto& scene:scenes.pools)if(!add(scene.scene.owner.shard,scene.scene.owner.region,scene.scene.owner.key))return false;
    World after;if(!native_open_runtime::read_current_world(after,error)||!same(world,after))return fail(error,error.empty()?"Boss actual NativeOpen changed during complete scene census":error);
    error.clear();return true;
}
}
static bool read_impl(const action_session::Context& context,const action_session::Binding& binding,State& out,
                      character_definition_identity::Census& definitions,character_membership_reader::Users& users,std::string& error){
    out={};State next;if(!admitted_characters(context,binding,next.world,definitions,users,error)||!pools(next.world,next.pools,error))return false;
    boss_identity::Scope scope;scope.snapshot=next.world.uuid;scope.epoch=next.world.world_epoch;scope.generation=next.world.generation;scope.root=uintptr_t(next.world.map)+0x1c0;
    std::vector<std::pair<std::string,boss_identity::Entry>> entries;if(!boss_identity::registry.enumerate(scope,entries,error))return false;
    std::set<uintptr_t> objects;std::set<uint64_t> ids;std::map<uintptr_t,std::pair<std::string,boss_identity::Entry>> expected;
    for(const auto& [key,row]:entries)if(!row.retired){if(!expected.emplace(row.pointer,std::pair{key,row}).second)return fail(error,"Boss live canonical registry repeats a current native object");}
    for(const auto& pool:next.pools)for(const auto& member:pool.native.members){const auto found=expected.find(member.pointer);
        if(!objects.insert(member.pointer).second||!ids.insert(member.uid).second||found==expected.end()||found->second.second.scope!=pool.scope||found->second.second.uid!=member.uid)return fail(error,"Boss current complete scene member lacks its exact confirmed canonical full UID");
        next.live.push_back(found->second);}
    if(next.live.size()!=expected.size())return fail(error,"Boss confirmed canonical binding absent from complete actual scene census");
    std::sort(next.live.begin(),next.live.end(),[](const auto& a,const auto& b){return a.first<b.first;});World after;
    if(!admitted(context,binding,after,error)||!same(next.world,after))return fail(error,error.empty()?"Boss current bound NativeOpen changed before readback":error);out=std::move(next);error.clear();return true;
}
bool read(const action_session::Context& context,const action_session::Binding& binding,State& out,std::string& error){
    character_definition_identity::Census definitions;character_membership_reader::Users users;
    return read_impl(context,binding,out,definitions,users,error);
}
bool read_with_characters(const action_session::Context& context,const action_session::Binding& binding,State& out,
                          character_definition_identity::Census& definitions,character_membership_reader::Users& users,std::string& error){
    definitions={};users={};character_definition_identity::Census next_definitions;character_membership_reader::Users next_users;
    if(!read_impl(context,binding,out,next_definitions,next_users,error))return false;
    definitions=std::move(next_definitions);users=std::move(next_users);return true;
}
bool prepare_birth(const action_session::Context& context,const action_session::Binding& binding,uintptr_t manager,const std::string& action_uuid,uint32_t ordinal,Birth& out,std::string& error){
    out={};if(!manager||!boss_identity::uuid(action_uuid)||ordinal>uint32_t(INT32_MAX))return fail(error,"Boss original birth requires actual manager and Host action UUID/ordinal");
    Birth next;next.context=context;next.binding=binding;next.key="action:"+action_uuid+":boss:"+std::to_string(ordinal);
    if(!read(context,binding,next.before,error))return false;
    const auto selected=std::find_if(next.before.pools.begin(),next.before.pools.end(),[&](const auto& value){return value.scope.manager==manager;});
    if(selected==next.before.pools.end())return fail(error,"Boss original birth manager absent from current complete Root/Dungeon census");
    for(const auto& [key,row]:next.before.live)if(key==next.key)return fail(error,"Boss original birth Host action identity already confirmed");
    next.scope=selected->scope;out=std::move(next);error.clear();return true;
}
bool confirm_birth(Birth& receipt,uintptr_t returned,std::string& error){
    if(receipt.attempted)return fail(error,"Boss original birth confirmation already attempted; inspect retained receipt, no replay");
    receipt.attempted=true;const auto failed=[&](const std::string& cause){receipt.unknown=true;receipt.cause=cause;return fail(error,cause);};
    if(!boss_identity::valid(receipt.scope)||!boss_identity::valid_key(receipt.key)||!returned)return failed("Boss original birth confirmation missing its prepared actual transaction or original returned object");
    World world;if(!admitted(receipt.context,receipt.binding,world,error)||!same(world,receipt.before.world))return failed(error.empty()?"Boss original birth current bound world differs":error);
    std::vector<Pool> current;if(!pools(world,current,error))return failed(error);
    if(current.size()!=receipt.before.pools.size())return failed("Boss original birth changed complete native scene membership");
    boss_identity::Member created;bool selected=false;
    for(const auto& before:receipt.before.pools){const auto after=std::find_if(current.begin(),current.end(),[&](const auto& value){return value.scope==before.scope;});
        if(after==current.end())return failed("Boss original birth current scene lifetime changed");
        if(before.scope==receipt.scope){if(selected)return failed("Boss original birth selected manager repeats");if(!boss_identity::born(before.native,after->native,returned,created,error))return failed(error);selected=true;}
        else if(before.native!=after->native)return failed("Boss original birth changed an unrelated complete manager census");}
    if(!selected)return failed("Boss original birth selected manager absent after original call");
    auto next=boss_identity::registry;const boss_identity::Entry staged{receipt.scope,created.pointer,created.uid,false,false};
    if(!next.stage(receipt.key,staged,error)||!next.confirm(receipt.key,staged,error))return failed(error);
    World after;if(!admitted(receipt.context,receipt.binding,after,error)||!same(world,after))return failed(error.empty()?"Boss original birth bound world changed before atomic canonical confirmation":error);
    std::vector<Pool> final;if(!pools(after,final,error)||final.size()!=current.size())return failed(error.empty()?"Boss original birth complete manager census changed before confirmation":error);
    for(size_t i=0;i<final.size();++i)if(final[i].scope!=current[i].scope||final[i].native!=current[i].native)return failed("Boss original birth member changed before atomic canonical confirmation");
    boss_identity::registry=std::move(next);receipt.created=staged;receipt.created.confirmed=true;receipt.confirmed=true;receipt.unknown=false;receipt.cause.clear();error.clear();return true;
}
bool prepare_retirement(const action_session::Context& context,const action_session::Binding& binding,const std::vector<boss_identity::Member>& selected,Retirement& out,std::string& error){
    out={};if(selected.size()>4096)return fail(error,"Boss original retirement selected member budget exceeded");Retirement next;next.context=context;next.binding=binding;
    if(!read(context,binding,next.before,error))return false;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
    for(const auto& member:selected){if(!member.pointer||!member.uid||!pointers.insert(member.pointer).second||!uids.insert(member.uid).second)return fail(error,"Boss original retirement selected full UID/pointer invalid or duplicated");
        const auto entry=std::find_if(next.before.live.begin(),next.before.live.end(),[&](const auto& value){return value.second.pointer==member.pointer&&value.second.uid==member.uid;});
        if(entry==next.before.live.end())return fail(error,"Boss original retirement selected member has no current canonical identity");next.selected[entry->second.scope.manager].push_back(member);}
    out=std::move(next);error.clear();return true;
}
bool confirm_retirement(Retirement& receipt,std::string& error){
    if(receipt.confirmed)return fail(error,"Boss original retirement receipt already confirmed");World world;
    if(!admitted(receipt.context,receipt.binding,world,error)||!same(world,receipt.before.world))return fail(error,error.empty()?"Boss original retirement current bound world differs":error);
    std::vector<Pool> current;if(!pools(world,current,error))return false;
    if(current.size()!=receipt.before.pools.size())return fail(error,"Boss original retirement changed native scene membership");
    auto next=boss_identity::registry;Find find{uintptr_t(GetModuleHandleW(nullptr))};
    for(const auto& before:receipt.before.pools){const auto after=std::find_if(current.begin(),current.end(),[&](const auto& value){return value.scope==before.scope;});
        if(after==current.end())return fail(error,"Boss original retirement scene owner lifetime changed");const auto selected=receipt.selected.find(before.scope.manager);
        if(selected==receipt.selected.end()){if(before.native!=after->native)return fail(error,"Boss original retirement changed an unrelated complete manager census");continue;}
        if(!next.retire_removed(before.scope,before.native,after->native,selected->second,find,error))return false;}
    World after;if(!admitted(receipt.context,receipt.binding,after,error)||!same(world,after))return fail(error,error.empty()?"Boss original retirement bound world changed before atomic canonical retirement":error);
    boss_identity::registry=std::move(next);receipt.confirmed=true;error.clear();return true;
}
}
