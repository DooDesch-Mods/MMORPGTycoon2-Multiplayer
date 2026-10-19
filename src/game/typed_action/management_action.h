#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>
#include <optional>

namespace mt2mp::action_session {
inline bool management_marketing_valid(const nlohmann::json& state){
    using Json=nlohmann::json;
    const auto number=[](const Json& v){return v.is_number_integer()&&
        (v.is_number_unsigned()?v.get<uint64_t>()<=uint64_t(INT32_MAX):v.get<int64_t>()>=INT32_MIN&&v.get<int64_t>()<=INT32_MAX);};
    const auto text=[](const Json& v){return v.is_string()&&v.get_ref<const std::string&>().size()<=256&&v.get_ref<const std::string&>().find('\0')==std::string::npos;};
    if(!state.is_object()||state.size()!=4||!state.contains("awards")||!state.contains("campaigns")||!state.contains("positiveBuzz")||!state.contains("negativeBuzz")||!number(state.at("positiveBuzz"))||!number(state.at("negativeBuzz")))return false;
    for(const auto* name:{"awards","campaigns"}){
        const bool award=std::string(name)=="awards";const auto& rows=state.at(name);
        if(!rows.is_array()||rows.size()>(award?512u:4u))return false;
        std::vector<std::string> keys;
        for(const auto& row:rows){if(!row.is_object()||row.size()!=(award?6u:8u))return false;
            if(award){
                for(auto field:{"name","source","day","impressions","conversions","isGood"})if(!row.contains(field))return false;
                if(!text(row.at("name"))||!text(row.at("source"))||!number(row.at("day"))||!row.at("isGood").is_boolean())return false;
            }else{
                for(auto field:{"customName","type","startedOn","duration","impressions","conversions","hype","active"})if(!row.contains(field))return false;
                if(!text(row.at("customName")))return false;
                for(auto field:{"type","startedOn","duration","hype","active"})if(!number(row.at(field)))return false;
                const auto type=row.at("type").get<int32_t>(),active=row.at("active").get<int32_t>();if(type<0||type>8||active<0||active>1)return false;
            }
            if(!number(row.at("impressions"))||!number(row.at("conversions")))return false;
            const auto key=award?Json::array({row.at("day"),row.at("name"),row.at("source")}).dump():row.at("type").dump();
            if(std::find(keys.begin(),keys.end(),key)!=keys.end())return false;keys.push_back(key);
        }
    }
    return true;
}
enum class ManagementOp:uint8_t {HireGameMaster=1,FireGameMaster,AllocateReleaseFunds,SpendStatistics,TakeLoan,RepayLoan,StartCampaign,ToggleReleaseKind,SetServicePrice,SetServiceSync,PurchaseStatistic};
struct ManagementIntent {
    ManagementOp op=ManagementOp::HireGameMaster;
    int32_t value=0;
    std::array<int32_t,10> statistics{};
    std::string key;
    int32_t service_ordinal=-1,currency=-1;
    bool operator==(const ManagementIntent&)const=default;
};
struct ManagementLoan {std::array<std::string,3> text{};std::array<int32_t,4> amounts{};bool operator==(const ManagementLoan&)const=default;};
struct ManagementOffer {std::array<std::string,3> text{};std::array<int32_t,3> amounts{};int32_t eligibility=0;bool available=false;bool operator==(const ManagementOffer&)const=default;};
struct ManagementService {int32_t type=0;std::string config_key;std::array<int32_t,2> price{};bool synced=false,allow_sync=false;bool operator==(const ManagementService&)const=default;};
struct ManagementServiceConfig {std::string key;std::array<int32_t,2> price{};bool operator==(const ManagementServiceConfig&)const=default;};
struct ManagementServices {std::string building_key;std::vector<ManagementService> rows;bool config_present=false;std::vector<ManagementServiceConfig> configs;bool operator==(const ManagementServices&)const=default;};
struct ManagementState {
    int32_t game_masters=0,release_funds=0,statistics_points=0,purchased_points=0;
    std::array<int32_t,10> statistics{},spent{};
    std::array<int32_t,3> character_slots{};
    std::array<uint8_t,10> purchased{};
    int32_t lounge_employed=0;std::vector<std::array<uint8_t,2>> gm_employment;
    bool release_present=false;int32_t release_type=0;
    std::array<int32_t,3> software_version{},planned_version{};
    std::array<int32_t,9> campaign_costs{};
    nlohmann::json marketing={{"awards",nlohmann::json::array()},{"campaigns",nlohmann::json::array()},{"positiveBuzz",0},{"negativeBuzz",0}};
    std::optional<ManagementServices> services;
    std::vector<ManagementLoan> loans;
    std::vector<ManagementOffer> offers;
    bool operator==(const ManagementState&)const=default;
};
struct Ledger;
nlohmann::json management_state_observation(const ManagementState&);
nlohmann::json management_intent_observation(const ManagementIntent&);
nlohmann::json management_ledger_observation(const Ledger&);
inline bool management_valid(const ManagementIntent& v){
    if(v.op<ManagementOp::HireGameMaster||v.op>ManagementOp::PurchaseStatistic)return false;
    if(v.op==ManagementOp::SetServicePrice||v.op==ManagementOp::SetServiceSync){
        if(v.key.empty()||v.key.size()>512||v.key.find('\0')!=std::string::npos||v.service_ordinal<0||v.service_ordinal>1023||v.statistics!=std::array<int32_t,10>{})return false;
        return v.op==ManagementOp::SetServicePrice?v.currency>=0&&v.currency<2&&v.value>=0:v.currency==-1&&(v.value==0||v.value==1);
    }
    if(v.service_ordinal!=-1||v.currency!=-1)return false;
    if(v.op==ManagementOp::PurchaseStatistic)return v.value>=0&&v.value<10&&v.key.empty()&&v.statistics==std::array<int32_t,10>{};
    if(v.op==ManagementOp::StartCampaign)return v.value>=0&&v.value<=8&&v.key.empty()&&v.statistics==std::array<int32_t,10>{};
    if(v.op==ManagementOp::AllocateReleaseFunds)return v.value>=0&&v.key.empty()&&v.statistics==std::array<int32_t,10>{};
    if(v.op==ManagementOp::SpendStatistics){
        int64_t sum=0;for(auto n:v.statistics){if(n<0)return false;sum+=n;}
        return sum>0&&sum<=INT32_MAX&&v.value==0&&v.key.empty();
    }
    if(v.op==ManagementOp::TakeLoan||v.op==ManagementOp::RepayLoan)
        return !v.key.empty()&&v.key.size()<=256&&v.key.find('\0')==std::string::npos&&v.value==0&&v.statistics==std::array<int32_t,10>{};
    return v.value==0&&v.key.empty()&&v.statistics==std::array<int32_t,10>{};
}
inline bool management_valid(const ManagementState& v){
    if(v.game_masters<0||v.game_masters>50||v.release_funds<0||v.statistics_points<0||v.purchased_points<0||v.loans.size()>1024||v.offers.size()>1024)return false;
    if(v.release_type<0||v.release_type>4||(!v.release_present&&(v.release_type||v.planned_version!=std::array<int32_t,3>{})))return false;
    for(const auto* values:{&v.software_version,&v.planned_version})for(auto n:*values)if(n<0)return false;
    for(size_t i=0;i<10;++i)if(v.statistics[i]<0||v.spent[i]<0||v.purchased[i]>1)return false;
    for(auto count:v.character_slots)if(count<0)return false;
    if(v.lounge_employed<0||v.lounge_employed>50||(!v.gm_employment.empty()&&v.gm_employment.size()!=50))return false;
    for(size_t n=0;n<v.gm_employment.size();++n)if(v.gm_employment[n][0]!=uint8_t(n<size_t(v.lounge_employed))||v.gm_employment[n][1]>1)return false;
    for(const auto& rows:{v.loans.size(),v.offers.size()})if(rows>1024)return false;
    for(const auto& r:v.loans)for(size_t i=0;i<3;++i)if(r.text[i].size()>(i?2048u:256u)||r.text[i].find('\0')!=std::string::npos)return false;
    for(const auto& r:v.offers)for(size_t i=0;i<3;++i)if(r.text[i].size()>(i?2048u:256u)||r.text[i].find('\0')!=std::string::npos)return false;
    for(auto cost:v.campaign_costs)if(cost<0||cost>INT32_MAX/7)return false;
    if(!management_marketing_valid(v.marketing))return false;
    if(v.services){const auto& services=*v.services;if(services.building_key.empty()||services.building_key.size()>512||services.rows.size()>1024||services.configs.size()>4096||(!services.config_present&&!services.configs.empty()))return false;
        for(const auto& row:services.rows)if(row.type<0||row.config_key.size()>256||row.config_key.find('\0')!=std::string::npos||row.price[0]<0||row.price[0]>=2||row.price[1]<0)return false;
        std::vector<std::string> keys;for(const auto& row:services.configs){if(row.key.empty()||row.key.size()>256||row.key.find('\0')!=std::string::npos||row.price[0]<0||row.price[0]>=2||row.price[1]<0||std::find(keys.begin(),keys.end(),row.key)!=keys.end())return false;keys.push_back(row.key);}
    }
    return true;
}
// The native leaf owns the derived statistics and LoanOffers rebuild. The action
// also checks their complete Host readback; these predicates constrain the intent.
inline bool management_transition(const ManagementIntent& i,const ManagementState& b,const ManagementState& a){
    if(!management_valid(i)||!management_valid(b)||!management_valid(a))return false;
    if((i.op==ManagementOp::SetServicePrice||i.op==ManagementOp::SetServiceSync)!=b.services.has_value()||b.services.has_value()!=a.services.has_value())return false;
    auto expected=b;
    switch(i.op){
    case ManagementOp::HireGameMaster:if(b.game_masters>=50)return false;expected.game_masters++;return a==expected;
    case ManagementOp::FireGameMaster:if(!b.game_masters)return false;expected.game_masters--;return a==expected;
    case ManagementOp::AllocateReleaseFunds:expected.release_funds=i.value;return a==expected;
    case ManagementOp::SpendStatistics:{
        int64_t sum=0;for(size_t n=0;n<10;++n){sum+=i.statistics[n];const int64_t spent=int64_t(b.spent[n])+i.statistics[n];
            if(spent>INT32_MAX)return false;expected.spent[n]=int32_t(spent);}
        const auto paid=std::max<int64_t>(0,sum-b.statistics_points);
        if(int64_t(b.purchased_points)+paid>INT32_MAX)return false;
        expected.purchased_points+=int32_t(paid);expected.statistics_points=int32_t(std::max<int64_t>(0,int64_t(b.statistics_points)-sum));
        for(size_t n=0;n<10;++n){
            const int64_t value=int64_t(b.statistics[n])+i.statistics[n];if(value>INT32_MAX)return false;
            expected.statistics[n]=int32_t(value);if(i.statistics[n])expected.purchased[n]=0;
        }
        for(size_t n=0;n<3;++n)expected.character_slots[n]=expected.statistics[n+5];
        return a==expected;
    }
    case ManagementOp::TakeLoan:{
        size_t index=0,matches=0;for(size_t n=0;n<b.offers.size();++n)if(b.offers[n].text[0]==i.key){index=n;++matches;}
        if(matches!=1||!b.offers[index].available)return false;const auto& q=b.offers[index];
        if(q.amounts[0]<=0||q.amounts[1]<q.amounts[0]||q.amounts[2]<=0)return false;
        expected.loans.push_back({q.text,{q.amounts[0],q.amounts[0]/q.amounts[2],(q.amounts[1]-q.amounts[0])/q.amounts[2],q.amounts[2]}});
        expected.offers.erase(expected.offers.begin()+index);return a==expected;
    }
    case ManagementOp::RepayLoan:{
        size_t index=0,matches=0;for(size_t n=0;n<b.loans.size();++n)if(b.loans[n].text[0]==i.key){index=n;++matches;}
        if(matches!=1||b.loans[index].amounts[0]<0)return false;
        expected.loans.erase(expected.loans.begin()+index);expected.offers=a.offers;return a==expected;
    }
    case ManagementOp::StartCampaign:{
        const auto& old=b.marketing.at("campaigns");const auto& now=a.marketing.at("campaigns");
        if(old.size()>=4||now.size()!=old.size()+1)return false;
        for(size_t n=0;n<old.size();++n)if(old[n]!=now[n]||old[n].at("type")==i.value)return false;
        const auto& row=now.back();if(row.at("type")!=i.value||row.at("duration")!=7||row.at("active")!=1)return false;
        expected.marketing=a.marketing;return a==expected&&b.marketing.at("awards")==a.marketing.at("awards")&&
            b.marketing.at("positiveBuzz")==a.marketing.at("positiveBuzz")&&b.marketing.at("negativeBuzz")==a.marketing.at("negativeBuzz");
    }
    case ManagementOp::ToggleReleaseKind:
        if(!b.release_present||(b.release_type!=2&&b.release_type!=3))return false;
        if(b.release_type==2){if(b.software_version[0]==INT32_MAX)return false;expected.release_type=3;expected.planned_version={b.software_version[0]+1,0,0};}
        else {if(b.software_version[1]==INT32_MAX)return false;expected.release_type=2;expected.planned_version={b.software_version[0],b.software_version[1]+1,0};}
        return a==expected;
    case ManagementOp::SetServicePrice:case ManagementOp::SetServiceSync:{
        if(!b.services||!a.services||b.services->building_key!=i.key||size_t(i.service_ordinal)>=b.services->rows.size())return false;
        auto& group=*expected.services;auto& row=group.rows[size_t(i.service_ordinal)];
        const auto config=std::find_if(group.configs.begin(),group.configs.end(),[&](const auto& item){return item.key==row.config_key;});
        if(i.op==ManagementOp::SetServicePrice){row.price={i.currency,i.value};
            if(row.synced&&group.config_present){if(config==group.configs.end())group.configs.push_back({row.config_key,row.price});else config->price=row.price;}
        }else{
            row.synced=row.allow_sync&&i.value!=0;
            if(row.synced&&group.config_present){if(config==group.configs.end())group.configs.push_back({row.config_key,row.price});else row.price=config->price;}
        }
        return a==expected;
    }
    case ManagementOp::PurchaseStatistic:expected.purchased[size_t(i.value)]=1;return a==expected;
    }
    return false;
}
template<class Ledger>bool management_ledger(const ManagementIntent& i,const ManagementState& state,const Ledger& before,Ledger& after){
    if(!management_valid(i))return false;
    after=before;int64_t amount=0;bool subtract=false;size_t category=0;
    switch(i.op){
    case ManagementOp::HireGameMaster:case ManagementOp::FireGameMaster:case ManagementOp::SpendStatistics:case ManagementOp::ToggleReleaseKind:case ManagementOp::SetServicePrice:case ManagementOp::SetServiceSync:return true;
    case ManagementOp::AllocateReleaseFunds:amount=int64_t(i.value)-state.release_funds;subtract=amount>0;amount=std::abs(amount);category=18;break;
    case ManagementOp::TakeLoan:for(const auto& q:state.offers)if(q.text[0]==i.key){amount=q.amounts[0];break;}category=17;break;
    case ManagementOp::RepayLoan:for(const auto& q:state.loans)if(q.text[0]==i.key){amount=q.amounts[0];break;}subtract=true;category=15;break;
    case ManagementOp::StartCampaign:amount=int64_t(state.campaign_costs[size_t(i.value)])*7;subtract=true;category=17;break;
    case ManagementOp::PurchaseStatistic:amount=int64_t(state.statistics[size_t(i.value)])*(i.value==8?10000:i.value==9?5000:2000);subtract=true;category=5;break;
    }
    if(!before.record_present||amount<0||amount>INT32_MAX||(subtract&&before.cash<amount))return false;
    auto add=[](int32_t& target,int64_t delta){const auto sum=int64_t(target)+delta;if(sum<INT32_MIN||sum>INT32_MAX)return false;target=int32_t(sum);return true;};
    const auto delta=subtract?-amount:amount;
    return add(after.cash,delta)&&add(after.record_cash,delta)&&add(subtract?after.record_expense:after.record_income,amount)&&add(after.line_counts[category],1)&&add(after.line_income[category],delta);
}
}
