#pragma once
#include "vendor/nlohmann/json.hpp"
#include "npc_quest_daily_apply.h"
#include <array>
#include <climits>
#include <set>

namespace mt2mp::game::service_live {
struct Count {
    int32_t today=0,total=0,day=0,highest=0;
    std::vector<int32_t> history;
    bool warning=false;
    bool operator==(const Count&)const=default;
};
struct Row {std::array<Count,2> counts;bool operator==(const Row&)const=default;};
struct Building {
    std::string key,metadata;
    std::vector<Row> rows;
    bool operator==(const Building&)const=default;
};
using State=std::vector<Building>;
inline bool digest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool valid(const State& v,std::string& e){
    if(v.size()>65536){e="Service Frame Building bound exceeded";return false;}
    std::string last;for(const auto& b:v){
        if(b.key.empty()||b.key.size()>512||b.key.find('\0')!=std::string::npos||(!last.empty()&&b.key<=last)||!digest(b.metadata)||b.rows.size()>1024){e="Service Frame key/order/metadata/row extent invalid";return false;}last=b.key;
        for(const auto& r:b.rows)for(const auto& c:r.counts)if(c.history.size()>10){e="Service Frame DailyCount history bound exceeded";return false;}
    }e.clear();return true;
}
inline nlohmann::json encode(const State& v){
    auto out=nlohmann::json::array();for(const auto& b:v){auto rows=nlohmann::json::array();for(const auto& r:b.rows){auto counts=nlohmann::json::array();for(const auto& c:r.counts)counts.push_back({{"today",c.today},{"total",c.total},{"day",c.day},{"highest",c.highest},{"history",c.history},{"warning",c.warning}});rows.push_back(std::move(counts));}out.push_back({{"key",b.key},{"metadata",b.metadata},{"rows",std::move(rows)}});}return out;
}
inline bool decode(const nlohmann::json& j,State& out,std::string& e){
    try{
        const auto number=[](const auto& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.template get<uint64_t>()>uint64_t(INT32_MAX):v.template get<int64_t>()<INT32_MIN||v.template get<int64_t>()>INT32_MAX))throw std::runtime_error("Service Frame signed int32 required");return v.template get<int32_t>();};
        if(!j.is_array()||j.size()>65536)throw std::runtime_error("Service Frame Building array required");State v;
        for(const auto& b:j){if(!b.is_object()||b.size()!=3||!b.at("rows").is_array()||b.at("rows").size()>1024)throw std::runtime_error("Service Frame Building shape invalid");Building item;item.key=b.at("key").get<std::string>();item.metadata=b.at("metadata").get<std::string>();
            for(const auto& r:b.at("rows")){if(!r.is_array()||r.size()!=2)throw std::runtime_error("Service Frame two counters required");Row row;
                for(size_t i=0;i<2;++i){const auto& c=r[i];if(!c.is_object()||c.size()!=6||!c.at("history").is_array()||c.at("history").size()>10||!c.at("warning").is_boolean())throw std::runtime_error("Service Frame DailyCount shape invalid");auto& x=row.counts[i];x.today=number(c.at("today"));x.total=number(c.at("total"));x.day=number(c.at("day"));x.highest=number(c.at("highest"));x.warning=c.at("warning").get<bool>();for(const auto& h:c.at("history"))x.history.push_back(number(h));}item.rows.push_back(std::move(row));}v.push_back(std::move(item));}
        if(!valid(v,e))return false;out=std::move(v);e.clear();return true;
    }catch(const std::exception& x){e=std::string("Service Frame decode: ")+x.what();return false;}
}
inline nlohmann::json first_difference(const State& before,const State& target){
    if(before.size()!=target.size())return {{"field","building_count"},{"actual",before.size()},{"expected",target.size()}};
    for(size_t i=0;i<before.size();++i){const auto& actual=before[i];const auto& expected=target[i];
        const char* field=actual.key!=expected.key?"key":actual.rows.size()!=expected.rows.size()?"row_count":actual.metadata!=expected.metadata?"metadata":nullptr;
        if(field)return {{"field",field},{"ordinal",i},{"actual_key",actual.key},{"expected_key",expected.key},
            {"actual_metadata",actual.metadata},{"expected_metadata",expected.metadata},{"actual_rows",actual.rows.size()},{"expected_rows",expected.rows.size()}};
    }return nullptr;
}
inline bool compatible(const State& before,const State& target,std::string& e){
    if(!valid(before,e)||!valid(target,e))return false;
    const auto difference=first_difference(before,target);
    if(!difference.is_null()){e=std::string(before.size()!=target.size()?"Service Frame complete Building coverage differs: ":"Service Frame membership or non-counter service state differs: ")+difference.dump();return false;}e.clear();return true;
}
struct Receipt {bool attempted=false,unknown=false,confirmed=false;uint32_t stores=0;};
template<class Memory,class Compare,class Reserve,class Guard>
bool apply_count(Memory& memory,uintptr_t image,uintptr_t object,size_t offset,const Count& before,const Count& target,
                 Compare&& compare,Reserve&& reserve,Guard&& guard,Receipt& out,std::string& e){
    out={};if(object>UINTPTR_MAX-offset||target.history.size()>10){e="Service DailyCount address/history bound invalid";return false;}
    npc_quest_daily_apply::NativeState actual;
    if(!guard(e)||!npc_quest_daily_apply::read_native(memory,image,object+offset,actual,e))return false;
    if(actual.value!=npc_quest_records::DailyCount{before.history,before.today,before.total}||actual.words[4]!=before.day||actual.words[6]!=before.highest||actual.warning!=uint8_t(before.warning)){e="Service DailyCount complete before differs";return false;}
    if(before==target){out.confirmed=true;return true;}
    npc_quest_daily_apply::Receipt receipt;
    const bool applied=npc_quest_daily_apply::apply_daily(memory,image,object,offset,actual.value,
        npc_quest_records::DailyCount{target.history,target.today,target.total},target.highest,target.day,compare,reserve,guard,receipt,e);
    out.attempted=receipt.mutation_attempted;out.unknown=receipt.unknown;out.stores=receipt.changed?1:0;
    if(!applied)return false;
    if(before.warning!=target.warning){
        uint8_t a=uint8_t(before.warning),b=uint8_t(target.warning);
        if(!guard(e)){out.unknown=out.attempted;return false;}
        out.attempted=true;if(!compare(object+offset+0x68,&a,&b,1,e)){out.unknown=true;return false;}++out.stores;
    }
    if(!guard(e)||!npc_quest_daily_apply::read_native(memory,image,object+offset,actual,e)){out.unknown=out.attempted;return false;}
    if(actual.value!=npc_quest_records::DailyCount{target.history,target.today,target.total}||actual.words[4]!=target.day||actual.words[6]!=target.highest||actual.warning!=uint8_t(target.warning)){e="Service DailyCount complete after differs";out.unknown=out.attempted;return false;}
    out.confirmed=true;e.clear();return true;
}
}
