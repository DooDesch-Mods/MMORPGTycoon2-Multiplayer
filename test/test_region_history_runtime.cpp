#include "typed_action/action_session.h"
#include "move_contract.h"
#include "building_history_slice/cash_effect.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>
namespace action=mt2mp::action_session;
namespace ui_move_runtime {namespace contract=mt2mp::game::ui_move_runtime::contract;}
using json=nlohmann::json;
namespace {
const std::string fixture_source_id="11111111-1111-4111-8111-111111111111",fixture_snapshot="22222222-2222-4222-8222-222222222222";
const std::string fixture_undo_id="33333333-3333-4333-8333-333333333333",fixture_redo_id="44444444-4444-4444-8444-444444444444";
const std::string fixture_empty_sha="4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945";
struct Runtime {bool ready=true;unsigned thread=7;uintptr_t image=0x100000;int memory=0;};
struct LocalView {uintptr_t region=0,random=0;std::vector<uint64_t> building_uids,scenery_uids,path_uids,path_node_uids,network_uids;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
unsigned GetCurrentThreadId(){return 7;}
struct Config {bool is_host=true;static Config& instance(){static Config config;return config;}};
struct PropertyOperation {action::Binding binding;action::Request request;action::Evidence evidence;bool confirmed=true,retired=true;uintptr_t level_graph=100,ownership_graph=100;std::optional<action::RegionPropertyState> ownership_before,ownership_after;};
struct PropertyRuntime {bool ready=true;std::map<std::string,PropertyOperation> operations;} properties;
alignas(8) std::array<unsigned char,1024> fixture_region_memory{};
std::array<uint64_t,4> fixture_regional_random{},fixture_global_random{};
action::RegionPropertyState fixture_region;
action::Ledger fixture_money;
std::vector<int32_t> fixture_actor_levels;
json fixture_controller;
unsigned fixture_calls=0;
action::Action fixture_graph_direction=action::Action::UndoBuilding;
std::array<int32_t,2> fixture_inverse_range{};
int32_t fixture_inverse_type=1,fixture_inverse_cash=0;
action::RegionPropertyState fixture_inverse_property;
bool fixture_native_failure=false,fixture_bad_effect=false,fixture_bad_ledger=false,fixture_bad_roster=false;
bool fixture_bad_inverse=false,fixture_fail_inverse=false,fixture_bad_owner=false;
uintptr_t fixture_region_pointer(){return reinterpret_cast<uintptr_t>(fixture_region_memory.data());}
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {json status(){return fixture_controller;}}
bool ledger(action::Ledger& out,std::string&){out=fixture_money;return true;}
bool sha256(const std::string& value,std::string& out,std::string& e){if(value!="[]"){e="unexpected hash port input";return false;}out=fixture_empty_sha;return true;}
bool bound(const LocalView&,const action::Binding& binding,const std::string& snapshot,std::string& e){
    if(binding!=action::Binding{2,3}||snapshot!=fixture_snapshot){e="original binding differs";return false;}return true;
}
bool property_read(Runtime&,int32_t id,uintptr_t,action::RegionPropertyState& out,LocalView& view,std::string& e,bool=false){
    if(id!=56){e="original Region ID missing";return false;}out=fixture_region;
    out.content.random=fixture_regional_random;std::memcpy(out.type_random.data(),fixture_region_memory.data()+0x268,32);
    view={};view.region=fixture_region_pointer()+(fixture_calls&&fixture_bad_owner?8:0);view.random=reinterpret_cast<uintptr_t>(fixture_regional_random.data());view.building_uids={101};view.scenery_uids={102};view.path_uids={103};view.path_node_uids={104};view.network_uids={105};return true;
}
std::string fixture_levels_hash(const std::vector<int32_t>& levels){
    unsigned sum=0;for(size_t i=0;i<levels.size();++i)sum+=unsigned(levels[i])*unsigned(i+1);
    char text[65];std::snprintf(text,sizeof text,"%064x",sum);return text;
}
bool level_state(Runtime&,const LocalView&,const std::array<int32_t,2>& range,action::RegionLevelState& out,uintptr_t& global,std::string&){
    out={};out.counts={0,fixture_actor_levels.size(),0};out.roster_sha256=std::string(64,fixture_calls&&fixture_bad_roster?'b':'a');
    out.levels_sha256=fixture_levels_hash(fixture_actor_levels);auto projected=fixture_actor_levels;
    for(auto& level:projected)level=std::clamp(level,range[0],range[1]);
    out.expected_sha256=fixture_levels_hash(projected);out.global_random=fixture_global_random;global=reinterpret_cast<uintptr_t>(fixture_global_random.data());return true;
}
bool property_same_before(action::RegionPropertyState left,action::RegionPropertyState right){
    left.content.random=right.content.random={};left.type_random=right.type_random={};
    if(left.actors&&right.actors)left.actors->global_random=right.actors->global_random={};return left==right;
}
namespace undo {
struct GraphWitness {uint32_t operations=1;bool locked=true,pending_delete=false;};
bool read_graph(int&,uintptr_t,uintptr_t graph,GraphWitness& out,std::string& e){
    if(graph!=100&&graph!=200){e="original graph read failed";return false;}out={};out.operations=fixture_inverse_cash?2:1;return true;
}
}
namespace sale_native=undo;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool appearance_read_native(Runtime&,uintptr_t,action::RegionAppearance& out,std::string&){out=fixture_region.appearance.value_or(action::RegionAppearance{});return true;}
bool sale_members(const LocalView& before,const LocalView& after,std::string& e){return (before.building_uids==after.building_uids&&before.scenery_uids==after.scenery_uids&&before.path_uids==after.path_uids&&before.path_node_uids==after.path_node_uids&&before.network_uids==after.network_uids)||fail(e,"native membership differs");}
bool text(Runtime&,uintptr_t address,std::string& out,std::string& e){for(size_t i=0;i<4;++i)if(address==0x4000+0xd0+i*0x20){out=fixture_inverse_property.content.styles[i];return true;}return fail(e,"unexpected native style read");}
template<class T>bool at(Runtime& runtime,uintptr_t address,size_t offset,T& out,std::string& e){
    if constexpr(std::is_same_v<T,uintptr_t>){
        if((address==100||address==200)&&offset==0x20){out=0x3000;return true;}
        if(address==0x3000&&offset==0){out=0x4000;return true;}
        if(address==0x3000&&offset==8){out=0x5000;return true;}
        if(address==0x5000&&offset==0){out=runtime.image+0x1786fb0;return true;}
        if(address==0x4000&&offset==0){out=runtime.image+0x1723230;return true;}
        if(address==0x4000&&offset==0xb8){out=fixture_region_pointer();return true;}
    }else if constexpr(std::is_same_v<T,int32_t>){
        if(address==0x4000&&offset==0xc0){out=fixture_inverse_type;return true;}
        if(address==0x5000&&offset==0xb8){out=fixture_inverse_cash;return true;}
    }else if constexpr(std::is_same_v<T,std::array<int32_t,2>>){
        if(address==0x4000&&offset==0xc4){out=fixture_inverse_range;if(fixture_bad_inverse)++out[1];return true;}
    }
    if constexpr(std::is_same_v<T,uint8_t>){if(address==0x4000&&offset==0x160){out=uint8_t(fixture_inverse_property.owned);return true;}}
    if constexpr(std::is_same_v<T,std::array<float,4>>){if(address==0x4000&&offset==0x150&&fixture_inverse_property.appearance){out=fixture_inverse_property.appearance->tint;return true;}}
    if constexpr(std::is_same_v<T,std::array<float,3>>){if(address==0x5000&&offset==0xbc){out={};return true;}}
    e="unexpected inverse native read";return false;
}
#include "region_ownership_native.inc"
namespace building_history_slice {
using mt2mp::game::building_history_slice::cash_effect;
bool graph_for(action::Action direction,const std::string& id,uintptr_t& out,std::string& e){
    out=0;if(id!=fixture_source_id||direction!=fixture_graph_direction||(fixture_calls&&fixture_fail_inverse)){e="original inverse graph unavailable";return false;}
    out=fixture_calls?200:100;e.clear();return true;
}
action::Outcome perform(action::Action direction,const std::string&,std::string& e){
    ++fixture_calls;if(fixture_native_failure){e="original native history failed";return action::Outcome::Unknown;}
    fixture_inverse_range={fixture_region.level,fixture_region.max_level};fixture_inverse_type=fixture_region.type;fixture_inverse_property=fixture_region;
    const auto& source=properties.operations.at(fixture_source_id);
    const bool ownership=source.ownership_before.has_value();
    const auto& destination=ownership?(direction==action::Action::UndoBuilding?*source.ownership_before:*source.ownership_after):
        (direction==action::Action::UndoBuilding?source.evidence.property_before:source.evidence.property_after);
    if(ownership){
        fixture_region.owned=destination.owned;fixture_region.status=destination.owned?1:0;
        action::Ledger next;if(!cash_effect(fixture_money,fixture_inverse_cash,next,e))return action::Outcome::Unknown;
        fixture_money=next;fixture_inverse_cash=-fixture_inverse_cash;
    }
    fixture_region.level=destination.level;fixture_region.max_level=destination.max_level;
    for(auto& level:fixture_actor_levels)level=std::clamp(level,destination.level,destination.max_level);
    fixture_regional_random[0]+=7;fixture_global_random[0]+=19;
    if(fixture_bad_effect)fixture_region.name="unexpected mutation";
    if(fixture_bad_ledger)++fixture_money.cash;
    fixture_graph_direction=direction==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    e.clear();return action::Outcome::Confirmed;
}
}
#include "region_history_runtime.inc"
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
action::Request setup(){
    properties={};region_history_operations.clear();fixture_runtime={};published=&fixture_runtime;fixture_calls=0;
    Config::instance().is_host=true;fixture_graph_direction=action::Action::UndoBuilding;
    fixture_native_failure=fixture_bad_effect=fixture_bad_ledger=fixture_bad_roster=fixture_bad_inverse=fixture_fail_inverse=fixture_bad_owner=false;
    fixture_region_memory={};fixture_regional_random={1,2,3,4};fixture_global_random={7,8,9,10};
    const std::array<uint64_t,4> type_random{11,12,13,14};std::memcpy(fixture_region_memory.data()+0x268,type_random.data(),32);
    fixture_region={};fixture_region.content.id=56;fixture_region.name="Test";fixture_region.type=1;fixture_region.status=0;fixture_region.level=4;fixture_region.max_level=6;fixture_region.owned=true;
    fixture_money={};fixture_money.cash=98765;fixture_inverse_cash=0;fixture_inverse_property={};fixture_actor_levels={4,6};
    PropertyOperation op;op.binding={2,3};op.request.action=action::Action::EditRegionProperty;op.request.action_uuid=fixture_source_id;
    op.request.snapshot_uuid=fixture_snapshot;op.request.canonical_record_key="region:56";op.request.region_property.id=56;op.request.region_property.kind=action::RegionPropertyKind::LevelRange;
    op.evidence.property_before=fixture_region;op.evidence.property_before.level=1;op.evidence.property_before.max_level=10;
    op.evidence.property_after=fixture_region;properties.operations.emplace(fixture_source_id,std::move(op));
    action::Request request;request.action=action::Action::UndoBuilding;request.action_uuid=fixture_undo_id;request.target_action_uuid=fixture_source_id;request.snapshot_uuid=fixture_snapshot;request.canonical_record_key="region:56";
    fixture_controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid",request.action_uuid},{"host_revision",uint64_t{5}}};return request;
}
}
int main(){try{unsigned cases=0;std::string e;const action::Binding binding{2,3};
    auto request=setup();need(region_history_perform(binding,request,nullptr,e)==action::Outcome::Confirmed,e);
    need(fixture_calls==1&&fixture_region.level==1&&fixture_region.max_level==10&&fixture_actor_levels==std::vector<int32_t>{4,6},"Undo did not use original range clamp");
    action::Evidence authority;need(region_history_observe(binding,request,authority,e)&&region_history_retire(binding,request,e),e);++cases;
    {request=setup();Config::instance().is_host=false;fixture_regional_random[0]=97;fixture_global_random[0]=79;
        need(region_history_perform(binding,request,&authority,e)==action::Outcome::Confirmed,e);action::Evidence out;
        need(region_history_observe(binding,request,out,e)&&out==authority,"Client did not adopt exact synchronous Host RNG/effect");++cases;}
    {request=setup();need(region_history_perform(binding,request,nullptr,e)==action::Outcome::Confirmed&&region_history_retire(binding,request,e),e);
        auto redo=request;redo.action=action::Action::RedoBuilding;redo.action_uuid=fixture_redo_id;fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t{6};
        need(region_history_perform(binding,redo,nullptr,e)==action::Outcome::Confirmed&&region_history_retire(binding,redo,e),e);
        need(fixture_calls==2&&fixture_region.level==4&&fixture_region.max_level==6&&fixture_actor_levels==std::vector<int32_t>{4,6},"Redo used synthetic actor restore");++cases;}
    auto reject=[&](auto mutate,const std::string& expected=""){auto req=setup();mutate(req);need(region_history_perform(binding,req,nullptr,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"invalid history reached original");need(expected.empty()||e.find(expected)!=std::string::npos,"distinct guard cause lost: "+e);++cases;};
    reject([](auto&){properties.operations.at(fixture_source_id).retired=false;});
    reject([](auto&){properties.operations.at(fixture_source_id).binding.native_generation++;});
    reject([](auto&){properties.operations.at(fixture_source_id).request.region_property.kind=action::RegionPropertyKind::Name;});
    reject([](auto& r){r.canonical_record_key="region:57";});
    reject([](auto& r){r.snapshot_uuid=fixture_source_id;},"snapshot differs");
    reject([](auto&){fixture_runtime.thread=8;},"Mainthread differs");
    reject([](auto&){Config::instance().is_host=false;},"authority/role differs");
    reject([](auto&){fixture_controller["locked"]=true;});
    reject([](auto&){fixture_graph_direction=action::Action::RedoBuilding;});
    {auto req=setup();Config::instance().is_host=false;auto bad=authority;bad.property_before.actors.reset();
        need(region_history_perform(binding,req,&bad,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"missing Host census reached seed/original");++cases;}
    auto unknown=[&](bool& flag,const std::string& cause){auto req=setup();flag=true;
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::Unknown&&e.find(cause)!=std::string::npos,e);
        const auto retained=e;need(!region_history_operations.at(req.action_uuid).confirmed&&fixture_calls==1,"partial history confirmed");
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1&&region_history_operations.at(req.action_uuid).cause==retained,"uncertain native history replayed/lost cause");++cases;};
    unknown(fixture_native_failure,"original native history failed");unknown(fixture_bad_effect,"consequence differs");
    unknown(fixture_bad_ledger,"consequence differs");unknown(fixture_bad_roster,"consequence differs");
    unknown(fixture_bad_inverse,"inverse owner/type/range differs");unknown(fixture_fail_inverse,"original inverse graph unavailable");unknown(fixture_bad_owner,"owner changed");
    {auto req=setup();need(region_history_perform(binding,req,nullptr,e)==action::Outcome::Confirmed,e);
        fixture_global_random[0]++;action::Evidence out;need(region_history_observe(binding,req,out,e),"later global draw changed synchronous receipt");
        fixture_actor_levels[0]++;need(!region_history_observe(binding,req,out,e),"changed actor levels acknowledged");++cases;}
    auto ownership_setup=[&](bool sale,action::Action direction,int32_t price=500){
        auto req=setup();req.action=direction;fixture_graph_direction=direction;
        auto& source=properties.operations.at(fixture_source_id);
        source.request.region_property.kind=sale?action::RegionPropertyKind::Sale:action::RegionPropertyKind::Purchase;
        source.evidence.before.cash=5000;source.evidence.after.cash=5000+(sale?price:-price);
        auto unowned=fixture_region;unowned.owned=false;unowned.status=0;unowned.level=unowned.max_level=0;unowned.appearance=action::RegionAppearance{};
        auto owned=unowned;owned.owned=true;owned.status=1;owned.level=2;owned.max_level=7;
        source.ownership_before=sale?owned:unowned;source.ownership_after=sale?unowned:owned;
        fixture_region=direction==action::Action::UndoBuilding?*source.ownership_after:*source.ownership_before;
        fixture_actor_levels=fixture_region.owned?std::vector<int32_t>{3,6}:std::vector<int32_t>{0,0};
        fixture_money.record_present=true;fixture_money.record_cash=fixture_money.cash;fixture_money.flat_day=fixture_money.record_day=7;
        fixture_inverse_property=direction==action::Action::UndoBuilding?*source.ownership_before:*source.ownership_after;
        fixture_inverse_range={fixture_inverse_property.level,fixture_inverse_property.max_level};
        fixture_inverse_cash=(sale?-price:price)*(direction==action::Action::UndoBuilding?1:-1);return req;
    };
    for(bool sale:{false,true})for(auto direction:{action::Action::UndoBuilding,action::Action::RedoBuilding})for(int32_t price:{0,500}){
        auto req=ownership_setup(sale,direction,price);const auto initial=fixture_money;const auto amount=fixture_inverse_cash;
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::Confirmed,e);
        action::Evidence host;need(region_history_observe(binding,req,host,e)&&region_history_retire(binding,req,e),e);
        need(fixture_calls==1&&host.after.record_income-initial.record_income==amount&&host.after.record_expense==initial.record_expense,
            "ownership history did not use original signed AddCash category6");
        req=ownership_setup(sale,direction,price);Config::instance().is_host=false;fixture_global_random[0]+=91;fixture_regional_random[0]+=71;
        need(region_history_perform(binding,req,&host,e)==action::Outcome::Confirmed,e);
        action::Evidence actual;need(region_history_observe(binding,req,actual,e)&&actual==host,"ownership Client receipt differs from Host");++cases;
    }
    {auto req=ownership_setup(false,action::Action::UndoBuilding);fixture_inverse_cash=-500;
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"wrong native signed Cash reached original");++cases;}
    {auto req=ownership_setup(true,action::Action::UndoBuilding);fixture_inverse_property.content.styles[0]="wrong";
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"wrong native style reached original");++cases;}
    {auto req=ownership_setup(false,action::Action::UndoBuilding);fixture_bad_ledger=true;
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1,"partial ownership cash confirmed");
        need(region_history_perform(binding,req,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1,"uncertain ownership history replayed");++cases;}
    std::cout<<cases<<" actual Region history original-clamp/RNG/inverse/ACK cases passed\n";return 0;
}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}}
