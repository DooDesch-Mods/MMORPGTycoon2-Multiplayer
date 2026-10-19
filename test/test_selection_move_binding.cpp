#include "multiplayer_world/world_state.h"
#include "move_kiss/move_kiss.h"
#include "move_contract.h"
#include "vendor/nlohmann/json.hpp"
#include <iostream>
#include <stdexcept>
#include <set>
namespace action=mt2mp::action_session;
namespace world_state=mt2mp::world_state;
namespace move_kiss=mt2mp::move_kiss;
using json=nlohmann::json;
struct Actor {uint64_t uid=0;action::BuildingState live,registered;};
std::map<std::string,Actor> actors;
std::vector<std::string> refreshes;
std::string fail_refresh;
bool change_uid=false;
unsigned native_calls=0,resolves=0;
world_state::Origin origin{"12345678-1234-4234-8234-123456789012",std::string(64,'a'),2,3};
json controller;
namespace native {using BuildingState=action::BuildingState;}
namespace remove_diagnostics {
struct LocalTarget {uint64_t uid=0;std::string key;};
bool refresh_moved_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& e){
    refreshes.push_back(key);
    if(key==fail_refresh){e="original refresh cause";return false;}
    const auto found=actors.find(key);
    if(snapshot!=origin.snapshot_uuid||found==actors.end()||found->second.uid!=uid){e="refresh actual full UID differs";return false;}
    found->second.registered=found->second.live;return true;
}
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& e){
    ++resolves;
    for(const auto& [unused,row]:actors){(void)unused;if(row.registered!=row.live){e="full registry census metadata differs";return false;}}
    const auto found=actors.find(key);if(snapshot!=origin.snapshot_uuid||found==actors.end()){e="registry target absent";return false;}
    out={found->second.uid,key};return true;
}
}
namespace scenery_kiss {
bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& e){
    for(const auto& [unused,row]:actors){(void)unused;if(row.uid==uid){out=row.live;return true;}}e="full UID absent";return false;
}
}
namespace mt2mp::world_state {
bool bind_root_readback(const Origin& owner,const std::string& key,const remove_diagnostics::LocalTarget& target,
                        const native::BuildingState& actual,LocalBinding& out,std::string& e){
    if(owner!=::origin||target.key!=key){e="binding owner/key differs";return false;}out={owner,key,target.uid,actual};return true;
}
}
namespace typed_action_runtime {json status(){return controller;}}
namespace ui_move_runtime {
namespace contract=mt2mp::game::ui_move_runtime::contract;
namespace selection_move {
#include "selection_move_refresh_actual.inc"
action::Outcome perform(const action::Binding&,const action::Request& request,const action::Evidence*,std::string& e){
    ++native_calls;std::vector<move_kiss::LocalBinding> before;
    for(const auto& row:request.selection_move.rows){move_kiss::LocalBinding local;local.local_uid=actors.at(row.key).uid;before.push_back(local);actors.at(row.key).live=row.after;}
    if(change_uid)++actors.at(request.selection_move.rows.back().key).uid;
    return refresh_targets(request,before,e)?action::Outcome::Confirmed:action::Outcome::Unknown;
}
}
}
struct Harness {
    world_state::Table world_;
    bool current(const action::Binding& binding,const std::string& snapshot,std::string& e){
        if(binding.native_epoch!=origin.world_epoch||binding.native_generation!=origin.generation||snapshot!=origin.snapshot_uuid){e="current owner differs";return false;}return true;
    }
#include "selection_move_shared_actual.inc"
};
void need(bool condition,const std::string& e){if(!condition)throw std::runtime_error(e);}
action::Request setup(Harness& h){
    actors.clear();refreshes.clear();fail_refresh.clear();native_calls=resolves=0;change_uid=false;
    action::Request request;request.action=action::Action::MoveSelection;request.action_uuid="23456789-1234-4234-8234-123456789012";
    request.snapshot_uuid=origin.snapshot_uuid;request.canonical_record_key="selection:"+request.action_uuid;
    std::vector<world_state::SnapshotObject> saved;
    for(unsigned i=1;i<=3;++i){action::BuildingState before;before.alive=true;before.type="SmallInn";before.variant="inn";before.x=float(i*10);before.z=20;
        const auto key=origin.sha256+":world:building:"+std::to_string(i);auto after=before;after.x+=3;after.z+=7;
        request.selection_move.rows.push_back({key,before,after});actors[key]={100+i,before,before};saved.push_back({key,key,before});}
    std::string e;need(h.world_.bind_snapshot(origin,saved,4,e),e);
    controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid",request.action_uuid},{"host_revision",uint64_t{5}}};return request;
}
int main(){try{unsigned cases=0;std::string e;const action::Binding binding{2,3};
    {Harness h;auto request=setup(h);need(h.perform_selection_move(binding,request,nullptr,e)==action::Outcome::Confirmed,e);
        need(native_calls==1&&refreshes.size()==3&&resolves==6&&h.world_.revision()==5,"one native transaction/all-refresh-before-resolve/single revision");
        for(const auto& row:request.selection_move.rows)need(h.world_.rows().at(row.key).state==row.after,"shared complete pose stale");
        auto next=request.selection_move.rows[0].after;next.x+=4;
        world_state::Change change{world_state::Kind::Move,6,"34567890-1234-4234-8234-123456789012",request.selection_move.rows[0].key,request.selection_move.rows[0].after,next};
        need(h.world_.prepare(origin,change,e)==world_state::Admission::New,e);
        auto fresh=origin;fresh.generation=4;std::vector<world_state::SnapshotObject> saved;
        for(const auto& row:request.selection_move.rows)saved.push_back({row.key,row.key,row.after});
        need(h.world_.rebind_snapshot(fresh,saved,e),e);++cases;}
    {Harness h;auto request=setup(h);const auto before=h.world_.rows();request.selection_move.rows[1].before.x+=1;
        need(h.perform_selection_move(binding,request,nullptr,e)==action::Outcome::RejectedNoEffect&&native_calls==0&&h.world_.rows()==before,"bad preflight reached original");++cases;}
    {Harness h;auto request=setup(h);const auto before=h.world_.rows();fail_refresh=request.selection_move.rows[1].key;
        need(h.perform_selection_move(binding,request,nullptr,e)==action::Outcome::Unknown&&e=="original refresh cause"&&refreshes.size()==2&&resolves==3&&h.world_.rows()==before&&h.world_.revision()==4,"partial refresh lost cause or committed table");++cases;}
    {Harness h;auto request=setup(h);change_uid=true;need(h.perform_selection_move(binding,request,nullptr,e)==action::Outcome::Unknown&&h.world_.revision()==4,"changed full UID accepted");++cases;}
    {Harness h;auto request=setup(h);controller["locked"]=true;need(h.perform_selection_move(binding,request,nullptr,e)==action::Outcome::RejectedNoEffect&&native_calls==0,"pending lock ignored");++cases;}
    auto table_reject=[&](auto mutate){Harness h;auto request=setup(h);const auto rows=h.world_.rows();std::vector<world_state::Change> changes;std::vector<world_state::LocalBinding> after;
        for(const auto& row:request.selection_move.rows){changes.push_back({world_state::Kind::Move,5,request.action_uuid,row.key,row.before,row.after});after.push_back({origin,row.key,actors.at(row.key).uid,row.after});}
        mutate(changes,after);need(h.world_.commit_moves(origin,changes,after,e)==world_state::Admission::Rejected&&h.world_.rows()==rows&&h.world_.revision()==4,"invalid batch partially changed table");++cases;};
    table_reject([](auto& c,auto&){c[1].revision=6;});table_reject([](auto& c,auto&){c[1].action_uuid="34567890-1234-4234-8234-123456789012";});
    table_reject([](auto& c,auto&){c[1].object_key=c[0].object_key;});table_reject([](auto&,auto& a){a[1].local_uid=a[0].local_uid;});
    table_reject([](auto&,auto& a){a[2].state.x+=1;});table_reject([](auto&,auto& a){a[2].origin.generation+=1;});
    table_reject([](auto& c,auto&){c[2].before.x+=1;});table_reject([](auto& c,auto&){c[2].kind=world_state::Kind::Remove;});
    table_reject([](auto&,auto& a){a.pop_back();});
    std::cout<<cases<<" actual Selection registry/shared batch cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
