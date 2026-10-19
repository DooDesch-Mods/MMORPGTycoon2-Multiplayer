#pragma once
#include "party_identity.h"
#include "duplicate_records.h"
#include <functional>

namespace mt2mp::game::party_live {
struct References {
    std::function<bool(uintptr_t,std::string&,std::string&)> identify;
    std::function<bool(const std::string&,uintptr_t&,std::string&)> resolve;
};
struct External {int32_t id=0;std::string key;bool operator==(const External&)const=default;};
struct Manager {std::string region;bool removal=false,release=false;bool operator==(const Manager&)const=default;};
struct Record {
    std::string key,sha;
    std::string shard,region,manager_region;
    std::vector<Manager> managers;
    std::vector<subscriber_lifecycle::Record> records;
    std::vector<int32_t> local_ids;
    std::vector<External> external;
    std::vector<std::string> toons;
    int32_t root_link=0;
};
namespace record_detail {
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
inline bool summarize(const Record& value,std::string& sha,std::string& error){
    if(value.records.empty()||value.records.size()>100000||value.records[0].label.type!=1||value.records[0].label.text!="mmoParty"||
       value.root_link<=0||value.local_ids.empty())return fail(error,"Party original Record root/local manifest differs");
    subscriber_lifecycle::Graph portable;portable.records=value.records;
    std::set<int32_t> ids;std::map<uintptr_t,std::string> external;
    for(const int32_t id:value.local_ids){if(id<=0||!ids.insert(id).second)return fail(error,"Party original local link IDs repeat or are invalid");
        portable.private_links.push_back({id,uintptr_t(uint32_t(id)),true});}
    if(!ids.contains(value.root_link))return fail(error,"Party original root is absent from local inline manifest");
    for(const auto& row:value.external){if(row.id<=0||row.key.empty()||row.key.size()>4096||!ids.insert(row.id).second)
            return fail(error,"Party original external canonical manifest is incomplete or duplicated");
        portable.private_links.push_back({row.id,uintptr_t(uint32_t(row.id)),false});external.emplace(uintptr_t(uint32_t(row.id)),row.key);}
    // Handles here validate a portable link manifest only. Native replay resolves
    // each canonical owner afresh and never receives these local validation values.
    duplicate_records::Witness witness;
    const auto owner=[&](uintptr_t handle,std::string& key,std::string& cause){const auto found=external.find(handle);
        if(found==external.end())return fail(cause,"Party portable external manifest contains an unknown link");key=found->second;return true;};
    if(!duplicate_records::summarize(portable,uintptr_t(uint32_t(value.root_link)),owner,witness,error))return false;
    std::set<std::string> toons;
    for(const auto& key:value.toons){if(key.empty()||!toons.insert(key).second||
        std::none_of(value.external.begin(),value.external.end(),[&](const auto& row){return row.key==key;}))
            return fail(error,"Party ordered Toon member lacks its original external canonical link");}
    sha=std::move(witness.sha);return true;
}
}
inline bool validate(const Record& value,std::string& error){
    if(!party_identity::valid_key(value.key))return record_detail::fail(error,"Party Record canonical identity differs");
    if(value.shard.empty()||value.region.empty()||value.managers.empty())return record_detail::fail(error,"Party Record current canonical Scene/manager ownership is absent");
    std::set<std::string> managers;
    for(const auto& owner:value.managers)if(owner.region.empty()||!managers.insert(owner.region).second)
        return record_detail::fail(error,"Party Record current canonical manager Region repeats or is absent");
    std::string sha;if(!record_detail::summarize(value,sha,error))return false;
    return sha==value.sha||record_detail::fail(error,"Party complete original Record/reference checksum differs");
}
}
