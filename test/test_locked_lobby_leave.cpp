#include "bridge.h"
#include "economic_fence.h"
#include "mt2mp/game/native_engine.h"
#include <iostream>
#include <stdexcept>
#include <limits>
namespace live=mt2mp::game::live_host_state;
namespace action=mt2mp::action_session;
namespace fixture {
uint32_t thread=7;unsigned world_reads=0,route_reads=0,checks=0;
int mode=3,saves=0;bool transition=false,world_ok=true;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void check(bool ok,const std::string& why){++checks;need(ok,why);}
void reset(){thread=7;world_reads=route_reads=0;mode=3;saves=0;transition=false;world_ok=true;}
}
uint32_t FixtureCurrentThreadId(){return fixture::thread;}
// Keep extracted methods deterministic without redefining the Windows import.
#define GetCurrentThreadId FixtureCurrentThreadId
namespace cash_fixture {
live::Pin current;int edits=0,saves=0,last_expected=-1;bool owner=true;unsigned reads=0;
void reset(){current={};edits=saves=0;last_expected=-1;owner=true;reads=0;}
}
namespace mt2mp::game::native {
bool read_world_status(WorldStatus& out,std::string& e){++fixture::world_reads;if(!fixture::world_ok){e="original native Title read failed";return false;}out={};out.active_mode_index=fixture::mode;out.title_transition_pending=fixture::transition;out.saves_in_progress=fixture::saves;return true;}
}
namespace mt2mp::game::live_host_state {
namespace {bool fail(std::string& e,const std::string& cause){e=cause;return false;}}
bool EconomicFence::routes_ready(std::string&){++fixture::route_reads;return true;}
bool EconomicFence::read_current_fence(const Pin&,std::string& e){return fail(e,"fixture must not perform native stores");}
bool read_time_authority(const Context&,uint64_t&,std::string& e){return fail(e,"fixture must not rebind old Time authority");}
bool day::Sync::suspend_snapshot(std::string& e){return fail(e,"fixture Day snapshot called during Leave");}
bool day::Sync::rebind_snapshot(const Context&,const Binding&,std::string& e){return fail(e,"fixture Day rebind called during Leave");}
bool day::economy::Coupling::rebind_snapshot(std::string& e){return fail(e,"fixture Economy rebind called during Leave");}
#include "bridge_leave.inc"
#include "fence_suspend.inc"
#include "fence_retire.inc"
namespace {
using Lookup=void*(*)(void*,int32_t);
template<class T>bool read(uintptr_t,T&,std::string& e){return fail(e,"cash fixture unexpectedly read a Subscriber");}
uint32_t call_lookup(Lookup,void*,int32_t,void**){throw std::runtime_error("cash fixture unexpectedly called a native lookup");}
}
bool NativePort::current_owners(const Pin&,std::string& e,int32_t expected_edits){
    cash_fixture::last_expected=expected_edits;
    return native_activity(cash_fixture::saves,cash_fixture::edits,expected_edits,e)&&
        (cash_fixture::owner||fail(e,"actual World/owner changed"));
}
bool NativePort::read(Pin& out,std::string& e){++cash_fixture::reads;
    if(!current_owners(cash_fixture::current,e,0))return false;out=cash_fixture::current;return true;}
bool NativePort::store(const Pin&,size_t,int32_t,int32_t,std::string& e){return fail(e,"unexpected native store");}
bool NativePort::publication_ready(uint64_t,bool&,std::string& e){return fail(e,"unexpected publication read");}
#include "fence_cash.inc"
}
#undef GetCurrentThreadId
namespace {
struct Port:live::Port {
    unsigned reads=0,stores=0;
    bool read(live::Pin&,std::string& e)override{++reads;e="unexpected physical old-World read";return false;}
    bool store(const live::Pin&,size_t,int32_t,int32_t,std::string& e)override{++stores;e="unexpected old-World store";return false;}
};
struct Transport:action::Transport {
    unsigned sends=0;
    bool claim(Handler,std::string& e)override{e="unexpected transport claim";return false;}
    bool send(uint64_t,const std::vector<uint8_t>&,std::string& e)override{++sends;e="unexpected send";return false;}
};
live::Context context(){return {41,2,1,3,action::Role::Client,"11111111-1111-4111-8111-111111111111"};}
live::EconomicFence fence(){live::EconomicFence f;f.prepared_=f.armed_=true;f.pin_.image=0x140000000;f.pin_.thread=7;f.context_=context();f.binding_={7,9};return f;}
void bridge_cases(){
    fixture::reset();Port port;Transport transport;std::string e;unsigned clocks=0;
    live::Bridge bridge(context(),{7,9},7,port,transport,[&](uint64_t&,std::string&){++clocks;return false;});
    bridge.locked_=true;bridge.queued_=live::Frame{};bridge.queued_->revision=19;
    bridge.receipt_.attempted=bridge.receipt_.unknown=true;bridge.receipt_.cause="actual Economy partial NPC pose mismatch";
    bridge.network_activation_receipt_.attempted_events=3;const auto receipt=bridge.receipt_;
    fixture::check(!bridge.suspend_snapshot(e),"normal snapshot suspension accepted a retained Unknown");
    fixture::check(bridge.suspend_lobby(e)&&bridge.suspended_&&bridge.lobby_retiring_&&bridge.locked_,"explicit idle Leave could not suspend retained Unknown");
    fixture::check(bridge.queued_&&bridge.queued_->revision==19&&bridge.receipt_.cause==receipt.cause&&bridge.receipt_.unknown&&bridge.receipt_.attempted&&bridge.network_activation_receipt_.attempted_events==3,"Leave erased or confirmed the failed frame/receipts");
    fixture::check(!bridge.rebind_snapshot(context(),{8,10},8,e)&&!port.reads&&!clocks&&!transport.sends,"retiring Unknown rebound or touched the old native pin");
    fixture::check(!bridge.suspend_lobby(e)&&bridge.receipt_.cause==receipt.cause,"repeated Leave suspension overwrote original Unknown");
    live::Bridge active(context(),{7,9},7,port,transport,{});active.host_before_=live::Frame{};
    fixture::check(!active.suspend_lobby(e)&&!active.suspended_&&!active.lobby_retiring_,"active Host-before action was retired");
    live::Bridge ordinary(context(),{7,9},7,port,transport,{});ordinary.queued_=live::Frame{};
    fixture::check(ordinary.suspend_lobby(e)&&ordinary.queued_.has_value()&&!ordinary.locked_,"queued but unexecuted frame could not be retained during Leave");
    fixture::check(!ordinary.rebind_snapshot(context(),{8,10},8,e)&&!port.reads&&!port.stores,"healthy Leave was mistaken for snapshot rebind");
}
void fence_cases(){
    fixture::reset();std::string e;auto f=fence();f.locked_=true;f.cause_="original economic native failure";std::vector<std::string> logs;
    f.logger_=[&](const char* level,const std::string& cause){logs.push_back(std::string(level)+" "+cause);};
    fixture::check(!f.suspend_snapshot(e)&&f.locked_&&f.cause_=="original economic native failure","snapshot path cleared economic Unknown");
    fixture::check(f.suspend_lobby(e)&&f.armed_&&f.suspended_&&f.lobby_retiring_&&f.locked_&&f.cause_=="original economic native failure","explicit Leave failed to preserve locked native owner");
    fixture::check(!f.retire_lobby(e)&&f.armed_&&f.locked_&&f.retired_cause_.empty(),"active World retired economic owner");
    fixture::mode=2;fixture::transition=true;fixture::check(!f.retire_lobby(e)&&f.armed_,"transitioning Title retired owner");
    fixture::transition=false;fixture::saves=1;fixture::check(!f.retire_lobby(e)&&f.armed_,"active save retired owner");
    fixture::saves=0;fixture::world_ok=false;fixture::check(!f.retire_lobby(e)&&e=="original native Title read failed"&&f.cause_=="original economic native failure","native read failure was hidden");
    fixture::world_ok=true;fixture::check(f.retire_lobby(e)&&!f.armed_&&!f.locked_&&!f.suspended_&&!f.lobby_retiring_&&f.retired_cause()=="original economic native failure"&&f.pin_.image==0x140000000,"actual idle Title did not retire only the old binding and retain original failure");
    fixture::check(logs.size()==1&&logs[0].find("warn ")==0&&logs[0].find("original economic native failure")!=std::string::npos,"retired original cause was not visibly retained");
    fixture::check(f.can_arm(e)&&fixture::route_reads==1&&f.retired_cause()=="original economic native failure","retired binding prevented new session or erased historical cause");
    const auto reads=fixture::world_reads;fixture::check(f.retire_lobby(e)&&fixture::world_reads==reads&&logs.size()==1&&f.retired_cause()=="original economic native failure","already retired fence repeated native work or lost receipt");
    for(unsigned kind=0;kind<5;++kind){auto blocked=fence();if(kind==0)blocked.armed_=false;if(kind==1)blocked.action_=action::Message{};if(kind==2)blocked.frame_depth_=2;if(kind==3)fixture::thread=8;if(kind==4)blocked.suspended_=true;
        fixture::check(!blocked.suspend_lobby(e)&&!blocked.lobby_retiring_,"unbound/action/reentry/thread/repeated owner entered Leave suspension");fixture::thread=7;}
    for(unsigned depth:{0u,1u}){auto idle=fence();idle.frame_depth_=depth;fixture::check(idle.suspend_lobby(e),"actual top-level UI frame could not request Leave");if(depth){fixture::check(!idle.retire_lobby(e)&&idle.armed_,"active native frame released fence before unwinding");}}
    auto unscoped=fence();unscoped.suspended_=unscoped.locked_=true;fixture::check(!unscoped.retire_lobby(e)&&unscoped.armed_,"ordinary snapshot suspension bypassed Unknown retirement scope");
}
void eraser_cash_cases(){
    fixture::reset();std::string error;
    const auto prepared=[&](){
        cash_fixture::reset();auto value=fence();value.frame_depth_=1;value.pin_.business=0x5000;
        value.action_pin_=value.pin_;value.action_=action::Message{};
        value.action_->request.action=action::Action::SceneryEraseStroke;
        auto& before=value.action_->evidence.before;before.record_present=1;before.cash=before.record_cash=94550;
        for(size_t i=0;i<20;++i){before.line_counts[i]=int32_t(i);before.line_income[i]=int32_t(i*10);}
        fixture::need(live::predicted_cash(before,false,10,6,value.action_->evidence.after,error),error);
        value.action_cash_ledger_=before;
        cash_fixture::edits=1;return value;
    };
    const auto refund=[](auto& value,int amount=10,int category=6){return value.allow(live::EconomicLeaf::AddCash,reinterpret_cast<void*>(0x5000),amount,category);};
    auto value=prepared();fixture::check(refund(value)&&cash_fixture::last_expected==1&&value.action_cash_calls_==1&&!value.locked_,
        "Original Eraser refund inside one native edit was suppressed");
    fixture::check(!refund(value)&&value.action_cash_calls_==1&&value.locked_,"Repeated Eraser refund reached the original");
    const auto original=value.cause_;const auto reads=cash_fixture::reads;
    fixture::check(!value.end_action(error)&&error==original&&cash_fixture::reads==reads&&!value.action_,"Sticky first cash failure lost its cause or read again");
    for(int edits:{0,2}){value=prepared();cash_fixture::edits=edits;
        fixture::check(!refund(value)&&value.locked_&&!value.action_cash_calls_,"Eraser admitted the wrong native edit depth");}
    value=prepared();cash_fixture::saves=1;fixture::check(!refund(value)&&value.locked_,"Eraser refund admitted active Save");
    value=prepared();cash_fixture::owner=false;fixture::check(!refund(value)&&value.cause_=="actual World/owner changed","Eraser ignored fresh owner proof");
    value=prepared();fixture::check(!value.allow(live::EconomicLeaf::AddCash,reinterpret_cast<void*>(0x5001),10,6)&&value.locked_,"Foreign Business received refund");
    value=prepared();fixture::check(!refund(value,11)&&value.locked_,"Refund amount differing from Host was admitted");
    value=prepared();fixture::check(!refund(value,10,5)&&value.locked_&&cash_fixture::last_expected==0,"Another cash category inherited Eraser edit scope");
    value=prepared();fixture::check(!value.allow(live::EconomicLeaf::SubtractCash,reinterpret_cast<void*>(0x5000),10,6)&&value.locked_,"Eraser scope admitted SubtractCash");
    value=prepared();value.action_->request.action=action::Action::SceneryBrushStroke;
    fixture::check(refund(value)&&cash_fixture::last_expected==1&&value.action_cash_calls_==1,"Brush replacement refund lost its native edit scope");
    fixture::check(!refund(value)&&value.action_cash_calls_==1,"Brush exceeded its authoritative one-refund budget");
    value=prepared();value.action_->request.action=action::Action::SceneryBrushStroke;
    fixture::need(live::predicted_cash(value.action_->evidence.before,true,10,6,value.action_->evidence.after,error),error);
    fixture::check(value.allow(live::EconomicLeaf::SubtractCash,reinterpret_cast<void*>(0x5000),10,6)&&cash_fixture::last_expected==1,"Brush purchase lost its native edit scope");
    value=prepared();value.action_->request.action=action::Action::EditModularBuilding;cash_fixture::edits=0;
    fixture::need(live::predicted_cash(value.action_->evidence.before,true,10,6,value.action_->evidence.after,error),error);
    fixture::check(value.allow(live::EconomicLeaf::SubtractCash,reinterpret_cast<void*>(0x5000),10,6)&&cash_fixture::last_expected==0,"Modular purchase before BeginEdit changed");
    const auto brush=[&](std::initializer_list<bool> expenses){
        auto selected=prepared();selected.action_->request.action=action::Action::SceneryBrushStroke;
        auto running=selected.action_->evidence.before;
        for(bool expense:expenses){action::Ledger next;
            fixture::need(live::predicted_cash(running,expense,10,6,next,error),error);running=next;}
        selected.action_->evidence.after=running;return selected;
    };
    const auto leaf=[](auto& selected,bool expense,int amount=10,int category=6){
        return selected.allow(expense?live::EconomicLeaf::SubtractCash:live::EconomicLeaf::AddCash,
            reinterpret_cast<void*>(0x5000),amount,category);
    };
    const auto actual_ledger=[](const action::Ledger& expected){
        auto& observed=cash_fixture::current.values;observed.cash=expected.cash;observed.calendar[3]=expected.flat_day;
        observed.record={expected.record_day,expected.record_cash,0,expected.record_income,expected.record_expense};
        for(size_t i=0;i<20;++i)observed.lines[i]={expected.line_counts[i],expected.line_income[i]};
    };
    value=brush({false,false,true});unsigned originals=0;
    for(bool expense:{false,false,true}){if(leaf(value,expense))++originals;else throw std::runtime_error(value.cause_);}
    fixture::check(originals==3&&value.action_cash_calls_==3&&value.action_cash_ledger_==value.action_->evidence.after,
        "Complete replacement Brush did not forward exactly its authorized refund/refund/purchase sequence");
    actual_ledger(value.action_->evidence.after);cash_fixture::edits=0;
    fixture::check(value.end_action(error)&&!value.locked_,"Complete native Brush ledger did not pass idle action exit");
    value=brush({false,true});fixture::check(leaf(value,true)&&leaf(value,false),"Valid mixed-kind order lost its bounded Host totals");
    fixture::check(value.action_cash_ledger_.cash==value.action_->evidence.before.cash&&value.action_cash_calls_==2,
        "Net-zero cash Brush lost two real category entries");
    actual_ledger(value.action_->evidence.after);cash_fixture::edits=0;
    fixture::check(value.end_action(error),"Net-zero complete Brush ledger rejected");
    value=brush({false,false,true});fixture::check(leaf(value,false),"First authorized Brush leaf rejected");cash_fixture::edits=0;
    const auto before_missing_reads=cash_fixture::reads;
    fixture::check(!value.end_action(error)&&value.locked_&&cash_fixture::reads==before_missing_reads&&error.find("required=3")!=std::string::npos,
        "Missing Brush leaves were mistaken for complete native execution");
    value=brush({false,true});fixture::need(leaf(value,false)&&leaf(value,true),"Valid two-leaf Brush failed");
    fixture::check(!leaf(value,false)&&value.action_cash_calls_==2&&value.locked_,"Extra Brush refund exceeded Host count/totals");
    value=brush({false,false});fixture::check(!leaf(value,true)&&!value.action_cash_calls_,"Wrong Brush leaf kind used the refund budget");
    for(int amount:{-10,0,9,20}){value=brush({false,false});
        fixture::check(!leaf(value,false,amount)&&!value.action_cash_calls_,"Brush admitted an amount outside original GetTypeCost");}
    value=brush({false,false});fixture::check(!leaf(value,false,10,5)&&cash_fixture::last_expected==0,"Brush admitted a foreign category scope");
    value=brush({false,false});fixture::need(leaf(value,false),"Initial Brush refund failed");cash_fixture::owner=false;
    fixture::check(!leaf(value,false)&&value.action_cash_calls_==1&&value.cause_=="actual World/owner changed","Later Brush leaf skipped fresh owner proof");
    value=brush({false,false});fixture::need(leaf(value,false),"Initial Brush refund failed");fixture::thread=8;
    fixture::check(!leaf(value,false)&&value.action_cash_calls_==1,"Later Brush leaf skipped thread proof");fixture::thread=7;
    value=brush({false,false});fixture::need(leaf(value,false),"Initial Brush refund failed");cash_fixture::edits=2;
    fixture::check(!leaf(value,false)&&value.action_cash_calls_==1,"Later Brush leaf admitted nested edit depth");
    value=brush({false,false});fixture::need(leaf(value,false),"Initial Brush refund failed");
    value.original_failed(live::EconomicLeaf::AddCash,0xc0000005u);const auto failure=value.cause_;
    fixture::check(!leaf(value,false)&&!value.end_action(error)&&error==failure&&value.action_cash_calls_==1,
        "Original Brush fault replayed or lost its first cause");
    for(size_t index=0;index<20;++index){value=brush({false,true});++value.action_->evidence.after.line_counts[index];
        fixture::check(!leaf(value,false)&&!value.action_cash_calls_,"Host full category count mismatch admitted before original");}
    value=brush({false,true});++value.action_->evidence.after.record_day;
    fixture::check(!leaf(value,false),"Host Record day mutation admitted as Brush cash");
    value=brush({false,true});++value.action_->evidence.after.record_cash;
    fixture::check(!leaf(value,false),"Host Record cash drift admitted as Brush cash");
    value=brush({false,true});fixture::need(leaf(value,false)&&leaf(value,true),"Valid Brush before actual readback failed");
    actual_ledger(value.action_->evidence.after);++cash_fixture::current.values.lines[19][1];cash_fixture::edits=0;
    fixture::check(!value.end_action(error)&&value.locked_&&error=="live economic authoritative full Ledger after differs",
        "Predicted Brush progress hid actual final ledger drift");
    value=prepared();value.action_.reset();fixture::check(!refund(value)&&!value.action_cash_calls_,"Unscoped refund was admitted");
    for(size_t line=0;line<20;++line){value=prepared();++value.action_->evidence.after.line_income[line];
        fixture::check(!refund(value)&&value.locked_,"Full Host category ledger difference was ignored");}
    value=prepared();fixture::need(refund(value),"Valid refund before idle exit failed");
    const auto after=value.action_->evidence.after;auto& actual=cash_fixture::current.values;
    actual.cash=after.cash;actual.calendar[3]=after.flat_day;actual.record={after.record_day,after.record_cash,0,after.record_income,after.record_expense};
    for(size_t i=0;i<20;++i)actual.lines[i]={after.line_counts[i],after.line_income[i]};
    cash_fixture::edits=0;
    fixture::check(value.end_action(error)&&cash_fixture::last_expected==0&&!value.locked_&&!value.action_,"Original EndEdit plus exact full Ledger failed idle exit");
    value=prepared();fixture::need(refund(value),"Valid refund before nonidle exit failed");
    fixture::check(!value.end_action(error)&&value.locked_&&error.find("required_edits=0")!=std::string::npos,"Action exit relaxed idle requirement");
    value=prepared();value.context_.role=action::Role::Host;
    fixture::check(refund(value)&&value.action_cash_calls_==0,"Host original cash forwarding changed");
}
}
int main(){try{bridge_cases();fence_cases();eraser_cash_cases();std::cout<<fixture::checks<<" actual Bridge/Fence Leave and cash cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
