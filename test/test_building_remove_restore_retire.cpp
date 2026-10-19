#include "typed_action/action_session.h"
#include "multiplayer_world/world_state.h"
#include "building_history_slice/cash_effect.h"
#include <iostream>
#include <stdexcept>
namespace action=mt2mp::action_session;namespace world_state=mt2mp::world_state;
namespace building_history_slice=mt2mp::game::building_history_slice;
using json=nlohmann::json;
namespace {
unsigned checks=0,retire_calls=0,remember_calls=0;void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
const std::string snap="11111111-1111-4111-8111-111111111111",removed="22222222-2222-4222-8222-222222222222";
bool retire_ok=true,graph_ok=true,remember_ok=true;action::Ledger native_ledger;std::string pending;
bool read_ledger(action::Ledger& out,std::string&){out=native_ledger;return true;}
namespace typed_action_runtime {json status(){return {{"pending",true},{"action_uuid",pending},{"host_revision",uint64_t(2)}};}}
namespace remove_diagnostics {
bool retire(const json&,json& out,std::string& e){++retire_calls;out={{"confirmed",retire_ok}};if(!retire_ok)e="original private holder retirement failure";return retire_ok;}
bool confirmed_remove_graph(const std::string&,const std::string&,const std::string&,uint64_t,uintptr_t& graph,std::string& e){if(!graph_ok){e="original committed graph unavailable";return false;}graph=0x7000;return true;}
}
}
namespace mt2mp::game::building_history_slice {
bool remember_remove(const std::string&,const std::string&,uintptr_t,std::string& e){++remember_calls;if(!remember_ok)e="original graph is not Undo top";return remember_ok;}
}
namespace {
struct Harness {
#include "remove_operation.inc"
std::map<std::string,Operation> operations_;world_state::Table world_;action::Request fixture_request;
#include "remove_retire.inc"
Harness(){retire_calls=remember_calls=0;retire_ok=graph_ok=remember_ok=true;pending=removed;std::string e;
action::BuildingState state;state.type="SmallInn";state.variant="base";state.alive=true;
world_state::Origin origin{snap,std::string(64,'a'),1,2};const auto key=origin.sha256+":world:building:44";
need(world_.bind_snapshot(origin,{{key,key,state}},1,e),e);fixture_request.action=action::Action::RemoveSnapshotBuilding;fixture_request.action_uuid=removed;fixture_request.snapshot_uuid=snap;fixture_request.canonical_record_key=key;
Operation op;op.request=fixture_request;op.binding={1,2};op.local_uid=3176;op.object_before=state;op.before.record_present=1;op.before.cash=op.before.record_cash=94000;operations_[removed]=op;
need(building_history_slice::cash_effect(op.before,400,native_ledger,e),e);}
bool run(std::string& e){return retire_remove({1,2},fixture_request,e);}
};
}
int main(){try{std::string e;
{Harness h;need(h.run(e)&&retire_calls==1&&remember_calls==1&&h.operations_.at(removed).history_ready&&h.operations_.at(removed).refund==400&&!h.world_.rows().begin()->second.state.alive,"confirmed retirement did not register exact Remove inverse: "+e);need(!h.run(e)&&retire_calls==1&&e.find("no replay")!=std::string::npos,"retirement original replayed");}
{Harness h;retire_ok=false;need(!h.run(e)&&remember_calls==0&&!h.operations_.at(removed).history_ready&&e=="original private holder retirement failure","holder cause lost or graph registered before retirement");}
{Harness h;graph_ok=false;need(!h.run(e)&&remember_calls==0&&!h.operations_.at(removed).history_ready&&e=="original committed graph unavailable","missing graph became history ready");}
{Harness h;remember_ok=false;need(!h.run(e)&&remember_calls==1&&!h.operations_.at(removed).history_ready&&e=="original graph is not Undo top","non-top graph accepted");}
{Harness h;++native_ledger.line_income[19];need(!h.run(e)&&remember_calls==0&&!h.operations_.at(removed).history_ready,"cash-only refund comparison admitted wrong category");}
{Harness h;native_ledger=h.operations_.at(removed).before;need(h.run(e)&&h.operations_.at(removed).refund==0,"zero refund should have no category6 cash event");}
{Harness h;h.operations_.at(removed).binding.native_epoch++;need(!h.run(e)&&retire_calls==0,"foreign binding retired holders");}
{Harness h;h.operations_.at(removed).normal_scenery=true;need(h.run(e)&&remember_calls==1&&h.operations_.at(removed).history_ready,"Scenery confirmed Remove graph did not become shared history");}
std::cout<<"PASS "<<checks<<" actual Remove retirement tail assertions; native retirement is controlled\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
