#include "typed_action/action_session.h"
#include "move_kiss/move_kiss.h"
#include "move_contract.h"
#include <atomic>
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace move_kiss=mt2mp::move_kiss;
namespace contract=mt2mp::game::ui_move_runtime::contract;
using json=nlohmann::json;
namespace {
const std::string source_id="11111111-1111-4111-8111-111111111111",snapshot="22222222-2222-4222-8222-222222222222";
const std::string undo_id="33333333-3333-4333-8333-333333333333",redo_id="44444444-4444-4444-8444-444444444444";
struct Runtime {bool ready=true;unsigned thread=7;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
unsigned GetCurrentThreadId(){return 7;}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
struct Operation {action::Binding binding;action::Request request;std::vector<move_kiss::LocalBinding> before;bool confirmed=true,retired=true;};
std::map<std::string,Operation> operations;
struct Actor {uint64_t uid;action::BuildingState live,registered;};
std::map<std::string,Actor> actors;
action::Ledger fixture_money;json controller;
unsigned native_calls=0,refresh_calls=0,current_calls=0;
bool native_failure=false,bad_pose=false,bad_ledger=false,fail_refresh=false,fail_inverse=false;
action::Action graph_direction=action::Action::UndoBuilding;
namespace typed_action_runtime {json controller_status(){return controller;}}
bool ledger(action::Ledger& out,std::string&){out=fixture_money;return true;}
namespace native {using BuildingState=action::BuildingState;}
namespace world_state {action::BuildingState building_state(const native::BuildingState& value){return value;}}
namespace scenery_kiss {
bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& e){
    for(const auto& [unused,row]:actors){(void)unused;if(row.uid==uid){out=row.live;return true;}}
    e="original full UID absent";return false;
}
}
bool refresh_targets(const action::Request& request,const std::vector<move_kiss::LocalBinding>& before,std::string& e){
    for(size_t i=0;i<before.size();++i){++refresh_calls;if(fail_refresh&&i==1){e="original history refresh failure";return false;}
        auto& row=actors.at(request.selection_move.rows[i].key);if(row.uid!=before[i].local_uid){e="refresh full UID differs";return false;}row.registered=row.live;}
    return true;
}
bool current(Runtime&,const action::Binding& binding,const action::Request& request,const std::vector<move_kiss::LocalBinding>& before,
             uintptr_t graph,action::SelectionMoveState& out,action::Ledger& cash,std::string& e){
    ++current_calls;out={};cash=fixture_money;
    if(binding!=action::Binding{2,3}||graph!=100||before.size()!=request.selection_move.rows.size()){e="current owner/graph/count differs";return false;}
    for(size_t i=0;i<before.size();++i){const auto& actor=actors.at(request.selection_move.rows[i].key);
        if(actor.uid!=before[i].local_uid||actor.registered!=actor.live||actor.live!=request.selection_move.rows[i].after||fixture_money!=before[i].ledger){e="current complete pose/UID/Ledger differs";return false;}
        out.rows.push_back(actor.live);}
    return true;
}
namespace building_history_slice {
bool graph_for(action::Action direction,const std::string& id,uintptr_t& out,std::string& e){
    out=0;if(id!=source_id||direction!=graph_direction||(native_calls&&fail_inverse)){e="original inverse graph missing";return false;}out=100;return true;
}
action::Outcome perform(action::Action direction,const std::string& id,std::string& e){
    ++native_calls;if(native_failure){e="original history failure";return action::Outcome::Unknown;}
    const auto& original=operations.at(id).request;
    for(const auto& row:original.selection_move.rows)actors.at(row.key).live=direction==action::Action::UndoBuilding?row.before:row.after;
    if(bad_pose)actors.begin()->second.live.x+=1;
    if(bad_ledger)++fixture_money.cash;
    graph_direction=direction==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    return action::Outcome::Confirmed;
}
}
#include "selection_history_runtime.inc"
void need(bool condition,const std::string& e){if(!condition)throw std::runtime_error(e);}
action::Request setup(){
    operations.clear();history_operations.clear();actors.clear();fixture_money={};fixture_money.cash=123456;
    native_calls=refresh_calls=current_calls=0;native_failure=bad_pose=bad_ledger=fail_refresh=fail_inverse=false;
    fixture_runtime={};published=&fixture_runtime;Config::instance().is_host=true;graph_direction=action::Action::UndoBuilding;
    Operation op;op.binding={2,3};op.request.action=action::Action::MoveSelection;op.request.action_uuid=source_id;op.request.snapshot_uuid=snapshot;
    for(unsigned i=0;i<3;++i){action::BuildingState before;before.type=i==1?"Cactus":"SmallInn";before.variant="base";before.alive=true;before.x=float(i*10);auto after=before;after.x+=5;
        const auto key="action:12345678-1234-4234-8234-00000000000"+std::to_string(i);
        op.request.selection_move.rows.push_back({key,before,after});move_kiss::LocalBinding local;local.local_uid=100+i;local.actual=before;local.ledger=fixture_money;op.before.push_back(local);
        actors.emplace(key,Actor{local.local_uid,after,after});}
    operations.emplace(source_id,std::move(op));
    action::Request r;r.action=action::Action::UndoBuilding;r.action_uuid=undo_id;r.target_action_uuid=source_id;r.snapshot_uuid=snapshot;r.canonical_record_key="selection:"+source_id;
    controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid",r.action_uuid},{"host_revision",uint64_t{5}}};return r;
}
}
int main(){try{unsigned cases=0;std::string e;const action::Binding binding{2,3};
    auto successful=[&](const action::Evidence* authority=nullptr){auto r=setup();Config::instance().is_host=!authority;
        need(perform_history(binding,r,authority,e)==action::Outcome::Confirmed,e);
        need(native_calls==1&&refresh_calls==3&&current_calls==2,"history did not run one graph/all target readbacks");
        action::Evidence out;need(observe_history(binding,r,out,e)&&out.undo_confirmed&&!out.selection_confirmed&&out.before==out.after,e);
        need(retire_history(binding,r,e),e);need(observe_history(binding,r,out,e)&&out.retirement_confirmed,e);++cases;return out;};
    const auto authority=successful();successful(&authority);
    {auto r=setup();need(perform_history(binding,r,nullptr,e)==action::Outcome::Confirmed&&retire_history(binding,r,e),e);
        auto redo=r;redo.action=action::Action::RedoBuilding;redo.action_uuid=redo_id;controller["action_uuid"]=redo_id;controller["host_revision"]=uint64_t{6};
        need(perform_history(binding,redo,nullptr,e)==action::Outcome::Confirmed&&retire_history(binding,redo,e),e);
        need(native_calls==2&&refresh_calls==6,"Redo split native transaction");++cases;}
    auto reject=[&](auto mutate){auto r=setup();mutate(r);need(perform_history(binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&native_calls==0,"invalid history reached original");++cases;};
    reject([](auto&){operations.at(source_id).retired=false;});
    reject([](auto&){operations.at(source_id).confirmed=false;});
    reject([](auto&){operations.at(source_id).binding.native_generation++;});
    reject([](auto& r){r.snapshot_uuid=source_id;});
    reject([](auto& r){r.canonical_record_key="selection:"+undo_id;});
    reject([](auto&){controller["locked"]=true;});
    reject([](auto&){graph_direction=action::Action::RedoBuilding;});
    reject([](auto&){actors.begin()->second.live.x+=1;});
    {auto r=setup();Config::instance().is_host=false;auto bad=authority;bad.selection_move_after->rows[1].x+=1;
        need(perform_history(binding,r,&bad,e)==action::Outcome::RejectedNoEffect&&native_calls==0,"wrong Host group accepted");++cases;}
    auto unknown=[&](bool& flag,const std::string& expected){auto r=setup();flag=true;
        need(perform_history(binding,r,nullptr,e)==action::Outcome::Unknown&&e.find(expected)!=std::string::npos,e);
        const auto original=e;need(!history_operations.at(r.action_uuid).confirmed&&native_calls==1,"partial native state confirmed");
        need(perform_history(binding,r,nullptr,e)==action::Outcome::Unknown&&native_calls==1,"uncertain graph was replayed");
        need(history_operations.at(r.action_uuid).cause==original,"original failure cause replaced");++cases;};
    unknown(native_failure,"original history failure");unknown(bad_pose,"full pose differs");unknown(bad_ledger,"Ledger differs");
    unknown(fail_refresh,"original history refresh failure");unknown(fail_inverse,"original inverse graph missing");
    {auto r=setup();need(perform_history(binding,r,nullptr,e)==action::Outcome::Confirmed,e);actors.begin()->second.live.x+=1;
        action::Evidence out;need(!observe_history(binding,r,out,e),"changed group acknowledged");++cases;}
    std::cout<<cases<<" actual Selection history orchestration cases passed\n";return 0;
}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}}
