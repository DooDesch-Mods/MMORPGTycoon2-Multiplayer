#pragma once
#include "live_host_state/state.h"
#include "mt2mp/game/native_engine.h"
#include <bit>
namespace mt2mp::game::marketing_authority {
inline Json ledger_fields(const native::SimulationState& s){return {{"cash",s.cash},{"record_present",s.record_present},{"record_day",s.record_day},
    {"record_cash",s.record_cash},{"record_income",s.record_income},{"record_expense",s.record_expense},{"line_counts",s.line_counts},{"line_income",s.line_income}};}
inline Json finance_fields(const native::SimulationState& s,const native::SoftwareState& software){
    auto out=ledger_fields(s);out["logged_in_subscribers"]=s.logged_in_subscribers;
    out["rating_bits"]=std::bit_cast<uint32_t>(software.rating);out["fun_bits"]=std::bit_cast<uint32_t>(software.fun);return out;
}
// Keep the existing equality rule and name every mismatched field in the receipt.
inline bool guard_equal(const char* phase,const char* group,const Json& required,const Json& measured,std::string& e){
    if(required==measured)return true;
    Json differences=Json::array();
    for(auto it=required.begin();it!=required.end();++it){
        const auto found=measured.find(it.key());if(found!=measured.end()&&*found==it.value())continue;
        differences.push_back({{"field",it.key()},{"required",it.value()},{"measured",found==measured.end()?Json(nullptr):*found}});
    }
    for(auto it=measured.begin();it!=measured.end();++it)if(!required.contains(it.key()))differences.push_back({{"field",it.key()},{"required",nullptr},{"measured",it.value()}});
    return fail(e,"guard differs "+Json{{"phase",phase},{"group",group},{"differences",std::move(differences)}}.dump());
}
inline bool host_frame_matches(const live_host_state::Frame& frame,const native::SimulationState& actual,std::string& e){
    native::SimulationState required;const auto ledger=live_host_state::ledger(frame.values);
    required.cash=ledger.cash;required.record_present=ledger.record_present!=0;required.record_day=ledger.record_day;required.record_cash=ledger.record_cash;
    required.record_income=ledger.record_income;required.record_expense=ledger.record_expense;required.line_counts=ledger.line_counts;required.line_income=ledger.line_income;
    if(!guard_equal("Host capture","Frame finance",ledger_fields(required),ledger_fields(actual),e))return false;
    const auto clock=[](int32_t day,int32_t seconds,float selected,float pause,bool paused){return Json{{"day",day},{"seconds",seconds},
        {"selected_bits",std::bit_cast<uint32_t>(selected)},{"pause_bits",std::bit_cast<uint32_t>(pause)},{"paused",paused}};};
    return guard_equal("Host capture","Frame Clock",clock(frame.values.calendar[3],frame.values.calendar[4],frame.selected,frame.pause,frame.selected==0||frame.pause==0),
        clock(actual.clock.flat_day,actual.clock.seconds,actual.clock.time_scale,actual.clock.pause_scale,actual.clock.paused),e);
}
}
