#pragma once
#include "mt2mp/game/native_engine.h"
#include "vendor/nlohmann/json.hpp"
#include <climits>
#include <string>

namespace mt2mp::game::tutorial_startingpoint_fixture {
// Cross-command observations can span native Release scans. Immediate native
// owner pins and execution readbacks still compare their complete diagnostics.
inline bool same_expected_development(const nlohmann::json& actual,const nlohmann::json& expected){
    if(actual==expected)return true;
    const nlohmann::json::json_pointer stamp("/startingpoint_release/mutation_counter");
    if(!actual.contains(stamp)||!expected.contains(stamp))return false;
    const auto valid_stamp=[](const nlohmann::json& value){
        return value.is_number_integer()&&(value.is_number_unsigned()?
            value.get<uint64_t>()<=UINT32_MAX:value.get<int64_t>()>=0&&value.get<int64_t>()<=UINT32_MAX);
    };
    if(!valid_stamp(actual.at(stamp))||!valid_stamp(expected.at(stamp)))return false;
    auto compared=expected;compared.at(stamp)=actual.at(stamp);
    return actual==compared;
}
inline bool before_begin(const nlohmann::json& d,uintptr_t project,std::string& e){
    const auto& p=d.at("project");const auto& t=d.at("startingpoint");
    if(project||p.at("present")!=false||p.at("work")!=0||p.at("lines")!=0||p.at("desired_workers")!=0||
       p.at("bugs_total")!=0||p.at("bugs_known")!=0)
        {e="startingpoint begin requires no current project and reset native counters";return false;}
    if(t.at("record_present")!=true||t.at("purchased")!=false||t.at("deployed")!=false||
       t.at("custom")!=false||t.at("owned")!=false||t.at("inventory_count")!=0||
       d.at("startingpoint_release").at("feature_count")!=0)
        {e="startingpoint begin requires an existing unpurchased unowned record and no item/feature";return false;}
    e.clear();return true;
}
inline nlohmann::json begin_ledger(const native::SimulationState& s){
    return {{"cash",s.cash},{"record_present",s.record_present},{"record_day",s.record_day},
        {"record_cash",s.record_cash},{"record_income",s.record_income},{"record_expense",s.record_expense},
        {"line_counts",s.line_counts},{"line_income",s.line_income},{"subscribers",s.logged_in_subscribers},
        {"clock_day",s.clock.flat_day},{"clock_seconds",s.clock.seconds},{"clock_scale",s.clock.time_scale},
        {"pause_scale",s.clock.pause_scale},{"paused",s.clock.paused}};
}
inline bool begin_payment(const native::SimulationState& before,int32_t price,nlohmann::json& expected,std::string& e){
    expected=nullptr;
    if(price<0||before.cash<price||!before.record_present||before.record_day!=before.clock.flat_day||
       before.record_cash!=before.cash||before.clock.time_scale!=0||!before.clock.paused)
        {e="startingpoint begin requires affordable nonnegative price and paused matching current ledger";return false;}
    if(before.line_counts[3]==INT32_MAX||int64_t(before.record_expense)+price>INT32_MAX||int64_t(before.line_income[3])-price<INT32_MIN)
        {e="startingpoint begin category3 ledger would overflow";return false;}
    auto after=before;after.cash-=price;after.record_cash-=price;after.record_expense+=price;
    ++after.line_counts[3];after.line_income[3]-=price;
    expected=begin_ledger(after);e.clear();return true;
}
}
