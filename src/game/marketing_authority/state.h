#pragma once
#include "vendor/nlohmann/json.hpp"
#include "notices.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::marketing_authority {
using Json=nlohmann::json;
struct State {Json values;std::optional<notice::Batch> notices;bool operator==(const State&)const=default;};
inline bool fail(std::string& e,const std::string& s){e="Marketing "+s;return false;}
inline void exact(const Json& j,std::initializer_list<const char*> names){
    if(!j.is_object()||j.size()!=names.size())throw std::runtime_error("exact object fields required");
    for(auto n:names)if(!j.contains(n))throw std::runtime_error(std::string("missing ")+n);
}
inline int32_t integer(const Json& j){
    if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>uint64_t(INT32_MAX):j.get<int64_t>()<INT32_MIN||j.get<int64_t>()>INT32_MAX))throw std::runtime_error("int32 required");
    return j.get<int32_t>();
}
inline void text(const Json& j){if(!j.is_string()||j.get_ref<const std::string&>().size()>256||j.get_ref<const std::string&>().find('\0')!=std::string::npos)throw std::runtime_error("bounded LocString base text required");}
inline std::string award_key(const Json& row){return Json::array({row.at("day"),row.at("name"),row.at("source")}).dump();}
inline std::string campaign_key(const Json& row){return std::to_string(integer(row.at("type")));}
inline bool current_campaign(const Json& row,int32_t day){
    const int64_t start=integer(row.at("startedOn")),duration=integer(row.at("duration")),end=start+duration;
    return duration>0&&end<=INT32_MAX&&end>=INT32_MIN&&start<=day&&day<end&&integer(row.at("active"))==1;
}
inline bool current_award(const Json& row,int32_t day){return int64_t(integer(row.at("day")))+6>=day;}
inline bool valid(const State& s,int32_t day,std::string& e,bool require_current=true){
    try{
        if(s.notices&&!notice::valid(*s.notices,e))return false;
        exact(s.values,{"awards","campaigns","positiveBuzz","negativeBuzz"});
        integer(s.values.at("positiveBuzz"));integer(s.values.at("negativeBuzz"));
        for(const auto name:{"awards","campaigns"}){
            const auto& rows=s.values.at(name);const bool award=std::string(name)=="awards";
            if(!rows.is_array()||rows.size()>(award?512u:4u))throw std::runtime_error("current row resource/admission bound exceeded");
            std::set<std::string> keys;
            for(const auto& row:rows){
                if(award){exact(row,{"name","source","day","impressions","conversions","isGood"});text(row.at("name"));text(row.at("source"));
                    const auto award_day=integer(row.at("day"));if(award_day<0||award_day>INT32_MAX-6||(require_current&&award_day>day))throw std::runtime_error("Award native expiry arithmetic/day invalid");if(!row.at("isGood").is_boolean())throw std::runtime_error("isGood bool required");
                    if(require_current&&!current_award(row,day))throw std::runtime_error("Host Award is expired at authoritative day");
                }else{exact(row,{"customName","type","startedOn","duration","impressions","conversions","hype","active"});text(row.at("customName"));
                    const int32_t type=integer(row.at("type"));if(type<0||type>8)throw std::runtime_error("Campaign type outside original enum");
                    integer(row.at("startedOn"));integer(row.at("duration"));integer(row.at("hype"));
                    const auto active=integer(row.at("active"));if(active<0||active>1)throw std::runtime_error("Campaign active byte invalid");
                    if(require_current&&!current_campaign(row,day))throw std::runtime_error("Campaign original Resolve current-day branch not admitted");
                }
                integer(row.at("impressions"));integer(row.at("conversions"));
                if(!keys.insert(award?award_key(row):campaign_key(row)).second)throw std::runtime_error("duplicate portable row identity");
            }
        }
        e.clear();return true;
    }catch(const std::exception& x){return fail(e,std::string("state: ")+x.what());}
}
struct Plan {
    bool purge_awards=false;
    std::vector<int32_t> abort_campaigns;
    std::vector<size_t> add_awards,add_campaigns,update_awards,update_campaigns;
    bool buzz=false;
    bool changed()const{return purge_awards||!abort_campaigns.empty()||!add_awards.empty()||!add_campaigns.empty()||!update_awards.empty()||!update_campaigns.empty()||buzz;}
};
// Retained native rows keep their owner and relative array order. New rows append.
inline bool plan(const State& before,const State& target,int32_t day,Plan& out,std::string& e){
    out={};if(!valid(before,day,e,false)||!valid(target,day,e))return false;
    for(const auto name:{"awards","campaigns"}){
        const bool award=std::string(name)=="awards";const auto& a=before.values.at(name);const auto& b=target.values.at(name);
        const auto key=[&](const Json& r){return award?award_key(r):campaign_key(r);};
        std::vector<std::string> retained,wanted;
        for(const auto& row:a){const auto k=key(row);auto found=std::find_if(b.begin(),b.end(),[&](const Json& r){return key(r)==k;});
            if(found!=b.end())retained.push_back(k);
            else if(award){if(current_award(row,day))return fail(e,"nonexpired Client-only Award cannot use PurgeOldAwards");out.purge_awards=true;}
            else out.abort_campaigns.push_back(integer(row.at("type")));
        }
        for(size_t i=0;i<b.size();++i){const auto k=key(b[i]);wanted.push_back(k);auto found=std::find_if(a.begin(),a.end(),[&](const Json& r){return key(r)==k;});
            if(found==a.end()){(award?out.add_awards:out.add_campaigns).push_back(i);retained.push_back(k);}
            else if(*found!=b[i]){if(!award&&integer(found->at("active"))!=1)return fail(e,"retained Campaign cached active flag differs before original Load");(award?out.update_awards:out.update_campaigns).push_back(i);}
        }
        if(retained!=wanted)return fail(e,"native retained/append order differs from Host current rows");
    }
    out.buzz=before.values.at("positiveBuzz")!=target.values.at("positiveBuzz")||before.values.at("negativeBuzz")!=target.values.at("negativeBuzz");
    e.clear();return true;
}
inline constexpr std::array<const char*,9> campaign_names={"MagazineAdvertisement","WebsiteAdvertisement","GameExhibition","FreeGameVouchers","PositiveStreamer","NegativeStreamer","Launch","Major","Minor"};
}
