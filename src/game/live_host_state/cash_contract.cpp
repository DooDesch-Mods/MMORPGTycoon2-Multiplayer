#include "economic_fence.h"
#include <limits>
namespace mt2mp::game::live_host_state {
namespace {
bool brush_cash_shape(const action_session::Ledger& before,const action_session::Ledger& value,std::string& e){
    // The pinned GetTypeCost body returns 10 for all 47 Scenery categories.
    const int64_t income=int64_t(value.record_income)-before.record_income;
    const int64_t expense=int64_t(value.record_expense)-before.record_expense;
    if(!before.record_present||income<0||expense<0||income%10||expense%10){e="Brush authoritative cash totals differ from original Scenery cost";return false;}
    auto expected=before;
    const auto add=[&](int32_t& field,int64_t delta){const int64_t sum=int64_t(field)+delta;
        if(sum<INT32_MIN||sum>INT32_MAX){e="Brush authoritative cash total overflows int32";return false;}field=int32_t(sum);return true;};
    if(!add(expected.cash,income-expense)||!add(expected.record_cash,income-expense)||
       !add(expected.record_income,income)||!add(expected.record_expense,expense)||
       !add(expected.line_counts[6],(income+expense)/10)||!add(expected.line_income[6],income-expense))return false;
    if(expected!=value){e="Brush authoritative complete Ledger/category/count differs";return false;}return true;
}
bool brush_cash_prefix(const action_session::Ledger& before,const action_session::Ledger& after,
    const action_session::Ledger& current,std::string& e){
    if(!brush_cash_shape(before,after,e)||!brush_cash_shape(before,current,e))return false;
    if(current.record_income>after.record_income||current.record_expense>after.record_expense||current.line_counts[6]>after.line_counts[6]){
        e="Brush native cash exceeds authoritative income/expense/call count";return false;}return true;
}
}
bool brush_cash_progress(const action_session::Ledger& before,const action_session::Ledger& after,
    const action_session::Ledger& current,bool subtract,int32_t amount,int32_t category,action_session::Ledger& out,std::string& e){
    out={};if(category!=6||amount!=10){e="Brush native cash leaf requires category6 and original amount10";return false;}
    if(!brush_cash_prefix(before,after,current,e)||!predicted_cash(current,subtract,amount,category,out,e)||
       !brush_cash_prefix(before,after,out,e))return false;e.clear();return true;
}
bool brush_cash_complete(const action_session::Ledger& before,const action_session::Ledger& after,
    const action_session::Ledger& current,uint32_t calls,std::string& e){
    if(!brush_cash_prefix(before,after,current,e))return false;
    const int64_t expected=int64_t(after.line_counts[6])-before.line_counts[6];
    if(current!=after||int64_t(calls)!=expected){e="Brush native cash completion differs calls="+std::to_string(calls)+
        " required="+std::to_string(expected);return false;}e.clear();return true;
}
bool predicted_cash(const action_session::Ledger& before,bool subtract,int32_t amount,int32_t category,action_session::Ledger& out,std::string& e){out={};if(!before.record_present||category<0||category>=20){e="native authoritative cash record/category invalid";return false;}out=before;auto plus=[&](int32_t& field,int64_t delta){const auto n=int64_t(field)+delta;if(n<INT32_MIN||n>INT32_MAX){e="native authoritative cash int32 overflow";return false;}field=int32_t(n);return true;};const int64_t delta=subtract?-int64_t(amount):int64_t(amount);if(!plus(out.cash,delta)||!plus(out.record_cash,delta)||!plus(subtract?out.record_expense:out.record_income,int64_t(amount))||!plus(out.line_counts[size_t(category)],1)||!plus(out.line_income[size_t(category)],delta))return false;e.clear();return true;}
}
