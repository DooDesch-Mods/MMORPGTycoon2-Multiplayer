#include "economic_fence.h"
#include <limits>
namespace mt2mp::game::live_host_state {
bool predicted_cash(const action_session::Ledger& before,bool subtract,int32_t amount,int32_t category,action_session::Ledger& out,std::string& e){out={};if(!before.record_present||category<0||category>=20){e="native authoritative cash record/category invalid";return false;}out=before;auto plus=[&](int32_t& field,int64_t delta){const auto n=int64_t(field)+delta;if(n<INT32_MIN||n>INT32_MAX){e="native authoritative cash int32 overflow";return false;}field=int32_t(n);return true;};const int64_t delta=subtract?-int64_t(amount):int64_t(amount);if(!plus(out.cash,delta)||!plus(out.record_cash,delta)||!plus(subtract?out.record_expense:out.record_income,int64_t(amount))||!plus(out.line_counts[size_t(category)],1)||!plus(out.line_income[size_t(category)],delta))return false;e.clear();return true;}
}
