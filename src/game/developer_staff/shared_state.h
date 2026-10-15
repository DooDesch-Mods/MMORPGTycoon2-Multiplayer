#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::developer_staff {
struct PoolRow {
    int32_t id=-1,state=0;
    bool employed=false,active=false,on_map=false,job_link=false;
    std::array<float,3> position{};
    bool operator==(const PoolRow&)const=default;
};
struct PoolState {
    int32_t count=0;
    std::vector<PoolRow> rows;
    std::vector<int32_t> indoor,outdoor;
    bool operator==(const PoolState&)const=default;
};
bool valid(const PoolState&,std::string&);
nlohmann::json encode(const PoolState&);
bool decode(const nlohmann::json&,PoolState&,std::string&);
bool count_effect(bool hire,const PoolState& before,const PoolState& after,std::string&);
template<class Ledger>
bool expected_payment(bool hire,const Ledger& before,Ledger& expected,std::string& e){
    expected=before;if(!hire){e.clear();return true;}
    if(before.record_present!=1||before.flat_day!=before.record_day||before.cash!=before.record_cash||before.cash<500||before.line_counts[10]==INT32_MAX||
       int64_t(before.record_expense)+500>INT32_MAX||int64_t(before.line_income[10])-500<INT32_MIN){e="Developer Hire native current record/affordability/overflow rejected";return false;}
    expected.cash-=500;expected.record_cash-=500;expected.record_expense+=500;++expected.line_counts[10];expected.line_income[10]-=500;
    e.clear();return true;
}
template<class Ledger>
bool payment(bool hire,const Ledger& before,const Ledger& after,std::string& e){
    Ledger expected;if(!expected_payment(hire,before,expected,e))return false;
    if(expected!=after){e=hire?"Developer Hire original500/category10 full ledger differs":"Developer Fire changed native full ledger";return false;}e.clear();return true;
}
}
