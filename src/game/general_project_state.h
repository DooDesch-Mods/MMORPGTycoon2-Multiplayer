#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cmath>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace mt2mp::game::general_project {
enum class Command:uint8_t {Start=1,Cancel=2,Commit=3,Workers=4};
struct Source {
    int32_t kind=0,statistic=0;
    std::string tech,art;
    bool operator==(const Source&)const=default;
};
inline bool text(const std::string& v){return v.size()<=200&&v.find('\0')==std::string::npos;}
inline bool valid(const Source& v){return v.kind>=0&&v.kind<=3&&(v.kind!=2||(v.statistic>=0&&v.statistic<10))&&text(v.tech)&&text(v.art)&&((v.kind!=0)||!v.tech.empty())&&((v.kind!=1)||!v.art.empty());}
inline nlohmann::json encode(const Source& v){return {{"kind",v.kind},{"tech",v.tech},{"art",v.art},{"statistic",v.statistic}};}
inline std::string key(const Source& v){return encode(v).dump();}
struct Catalogue {
    Source source;
    bool valid=false,visible=false,owned=false,choose=false,afford=false;
    std::array<uint8_t,3> tech_flags{};
    bool art_purchased=false,art_deployed=false;
    bool operator==(const Catalogue&)const=default;
};
struct Progress {
    std::optional<Source> current;
    uint32_t work=0;
    int32_t lines=0,bugs_total=0,bugs_known=0,workers=0;
    bool operator==(const Progress&)const=default;
};
inline bool valid(const Progress& v){
    const auto work=std::bit_cast<float>(v.work);
    return (!v.current||valid(*v.current))&&std::isfinite(work)&&work>=0&&v.lines>=0&&v.bugs_total>=0&&v.bugs_known>=0&&v.bugs_known<=v.bugs_total&&v.workers>=(v.current?-1:0);
}
struct State {
    Progress progress;
    std::vector<Catalogue> catalogue;
    std::array<std::string,200> inventory{};
    std::array<uint64_t,4> random{};
    struct Mod {int32_t value_id=0;std::array<uint32_t,2> payload{};bool integer=false,active=false;bool operator==(const Mod&)const=default;};
    struct Tech {std::string key;std::array<uint8_t,3> flags{};std::vector<Mod> mods;bool operator==(const Tech&)const=default;};
    struct Pack {std::string key;bool purchased=false,deployed=false;bool operator==(const Pack&)const=default;};
    struct Value {std::string name;uint32_t base=0,current=0;std::vector<std::array<uint32_t,2>> mods;bool operator==(const Value&)const=default;};
    std::vector<Tech> techs;std::vector<Pack> packs;
    std::array<uint8_t,10> features{};
    std::array<std::vector<Value>,2> values;
    std::array<int32_t,10> statistics{},spent{};std::array<uint8_t,10> purchased{};
    int32_t free_points=0,purchased_points=0;
    bool operator==(const State&)const=default;
};
struct Intent {Command command=Command::Start;Source source;int32_t workers=0;bool operator==(const Intent&)const=default;};
inline bool valid(const Intent& i){return i.command>=Command::Start&&i.command<=Command::Workers&&valid(i.source)&&i.workers>=0&&i.workers<=50&&(i.command==Command::Workers||!i.workers);}
inline bool valid(const State& v){
    if(!valid(v.progress)||v.catalogue.size()>4096)return false;std::set<std::string> keys;
    for(const auto& row:v.catalogue){if(!valid(row.source)||!keys.insert(key(row.source)).second)return false;for(auto flag:row.tech_flags)if(flag>1)return false;}
    std::set<std::string> techs,packs;
    for(const auto& row:v.techs){if(row.key.empty()||!text(row.key)||!techs.insert(row.key).second||row.mods.size()>4096)return false;for(auto flag:row.flags)if(flag>1)return false;for(const auto& mod:row.mods)if(mod.value_id<0||mod.payload[0]>2||size_t(mod.value_id)>=v.values[mod.integer?0:1].size())return false;}
    for(const auto& row:v.packs)if(row.key.empty()||!text(row.key)||!packs.insert(row.key).second)return false;
    for(const auto& s:v.inventory)if(!text(s))return false;
    for(const auto& a:{v.statistics,v.spent})for(auto value:a)if(value<0)return false;
    for(auto flag:v.purchased)if(flag>1)return false;
    for(auto flag:v.features)if(flag>1)return false;
    for(const auto& rows:v.values){if(rows.size()>4096)return false;for(const auto& row:rows){if(!text(row.name)||row.mods.size()>4096)return false;for(const auto& mod:row.mods)if(mod[0]>2)return false;}}
    return v.free_points>=0&&v.purchased_points>=0&&v.techs.size()<=4096&&v.packs.size()<=4096;
}
inline bool before(const Intent& i,const State& s,std::string& e){
    if(!valid(i)||!valid(s)){e="General project command/state invalid";return false;}
    const auto found=std::find_if(s.catalogue.begin(),s.catalogue.end(),[&](const auto& r){return r.source==i.source;});
    if(found==s.catalogue.end()){e="General project exact current catalogue source missing";return false;}
    if(i.command==Command::Start){if(!found->valid||!found->visible||found->owned||!found->choose||!found->afford){e="General project original visibility/ownership/prerequisites/payment rejects Start";return false;}}
    else if(!s.progress.current||*s.progress.current!=i.source){e="General project current owning clone/source differs";return false;}
    e.clear();return true;
}
inline bool transition(const Intent& i,const State& a,const State& b,std::string& e){
    if(!before(i,a,e)||!valid(b)){if(e.empty())e="General project after invalid";return false;}
    if(a.catalogue.size()!=b.catalogue.size()){e="General project changed catalogue ownership";return false;}
    for(size_t n=0;n<a.catalogue.size();++n)if(a.catalogue[n].source!=b.catalogue[n].source){e="General project replaced/reordered catalogue source";return false;}
    if(i.command==Command::Start){
        const bool same=a.progress.current&&*a.progress.current==i.source;
        if(!b.progress.current||*b.progress.current!=i.source||(!same&&(b.progress.work||b.progress.lines||b.progress.bugs_total||b.progress.bugs_known))||(same&&a.progress!=b.progress)||a.inventory!=b.inventory){e="General project original Start clone/reset/inventory differs";return false;}
    }else if(i.command==Command::Workers){auto expected=a;expected.progress.workers=i.workers;if(expected!=b){e="General project worker allocation changed unrelated state";return false;}}
    else if(b.progress.current||b.progress.work||b.progress.lines||b.progress.bugs_total||b.progress.bugs_known||b.progress.workers){e="General project original owning retirement/counters not complete";return false;}
    if(i.command==Command::Cancel){auto expected=a;expected.progress={};if(expected!=b){e="General project Cancel changed payment/ownership/inventory/RNG";return false;}}
    e.clear();return true;
}
inline nlohmann::json encode(const Progress& p){return {{"current",p.current?encode(*p.current):nlohmann::json(nullptr)},{"work",p.work},{"lines",p.lines},{"bugs_total",p.bugs_total},{"bugs_known",p.bugs_known},{"workers",p.workers}};}
inline nlohmann::json encode(const Catalogue& r){return {{"source",encode(r.source)},{"valid",r.valid},{"visible",r.visible},{"owned",r.owned},{"choose",r.choose},{"afford",r.afford},{"tech_flags",r.tech_flags},{"art_purchased",r.art_purchased},{"art_deployed",r.art_deployed}};}
struct Proof {
    Catalogue selected;Progress progress;
    std::string catalogue_sha256,ownership_sha256,character_catalogue_sha256;
    std::array<uint64_t,4> random{};
    bool operator==(const Proof&)const=default;
};
inline nlohmann::json encode(const Proof& p){return {{"selected",encode(p.selected)},{"progress",encode(p.progress)},{"catalogue_sha256",p.catalogue_sha256},{"ownership_sha256",p.ownership_sha256},{"character_catalogue_sha256",p.character_catalogue_sha256},{"random",p.random}};}
template<class Hash>bool proof(const Source& selected,const State& s,const std::string& character_catalogue_sha256,Hash&& hash,Proof& out,std::string& e){
    const auto found=std::find_if(s.catalogue.begin(),s.catalogue.end(),[&](const auto& r){return r.source==selected;});
    if(character_catalogue_sha256.size()!=64||character_catalogue_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos){e="General project complete Character catalogue digest absent/invalid";return false;}
    if(!valid(s)||found==s.catalogue.end()){e="General project complete proof source/state invalid";return false;}
    auto catalogue=nlohmann::json::array(),techs=nlohmann::json::array(),packs=nlohmann::json::array();
    std::map<std::string,Source> canonical_sources;std::map<std::string,State::Tech> canonical_techs;std::map<std::string,State::Pack> canonical_packs;
    for(const auto& row:s.catalogue)canonical_sources.emplace(key(row.source),row.source);for(const auto& row:s.techs)canonical_techs.emplace(row.key,row);for(const auto& row:s.packs)canonical_packs.emplace(row.key,row);
    for(const auto& [k,row]:canonical_sources)catalogue.push_back(encode(row));
    for(const auto& [k,row]:canonical_techs){auto mods=nlohmann::json::array();for(const auto& m:row.mods)mods.push_back({m.value_id,m.payload,m.integer,m.active});techs.push_back({row.key,row.flags,mods});}for(const auto& [k,row]:canonical_packs)packs.push_back({row.key,row.purchased,row.deployed});
    auto values=nlohmann::json::array();for(const auto& rows:s.values){auto group=nlohmann::json::array();for(const auto& row:rows)group.push_back({row.name,row.base,row.current,row.mods});values.push_back(group);}
    out.character_catalogue_sha256=character_catalogue_sha256;out.selected=*found;out.progress=s.progress;out.random=s.random;out.catalogue_sha256=hash(catalogue.dump());
    out.ownership_sha256=hash(nlohmann::json{techs,packs,s.inventory,s.statistics,s.spent,s.purchased,s.free_points,s.purchased_points,s.features,values}.dump());e.clear();return true;
}
}
