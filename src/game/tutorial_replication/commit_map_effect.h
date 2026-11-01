#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace mt2mp::game::tutorial_replication::startingpoint::map_effect {
using Json=nlohmann::json;
inline bool check(const Json& before,const Json& after,int32_t total,int32_t known,std::string& error){
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(total<0||known<0||known>total)return fail("Startingpoint Map effect project counts invalid");
    const auto& a=before.at("regions");const auto& b=after.at("regions");
    if(a.size()!=b.size())return fail("Startingpoint Commit changed Map Region membership");
    uint64_t appended=0,discovered=0,strengthened=0;
    for(size_t region=0;region<a.size();++region){
        if(a[region][0]!=b[region][0]||a[region][1].size()!=b[region][1].size())return fail("Startingpoint Commit changed Region target membership");
        for(size_t target=0;target<a[region][1].size();++target){
            const auto& old=a[region][1][target];const auto& now=b[region][1][target];
            if(old.size()!=6||now.size()!=6)return fail("Startingpoint Bug projection shape differs");
            for(size_t n=0;n<5;++n)if(old[n]!=now[n])return fail("Startingpoint Commit changed Bug target ownership/order/pose");
            const auto& prior=old[5];const auto& current=now[5];
            if(current.size()<prior.size())return fail("Startingpoint Commit removed an existing Bug");
            for(size_t n=0;n<current.size();++n){
                const auto value=current[n].get<std::array<uint8_t,27>>();float reproducibility=0;
                std::memcpy(&reproducibility,value.data()+16,sizeof(reproducibility));
                if(!std::isfinite(reproducibility)||reproducibility<0||reproducibility>1||value[24]>1||value[25]>1||value[26]>1)
                    return fail("Startingpoint original Bug values invalid");
                if(n<prior.size()){
                    const auto original=prior[n].get<std::array<uint8_t,27>>();auto unchanged=value;
                    std::copy(original.begin()+16,original.begin()+20,unchanged.begin()+16);
                    if(unchanged!=original)return fail("Startingpoint Commit changed existing Bug fields beyond original MakeWorse");
                    if(value!=original){if(reproducibility>0.2f)return fail("Startingpoint MakeWorse RNG result exceeds native range");++strengthened;}
                }else{
                    ++appended;if(value[24])++discovered;
                    if(reproducibility>0.2f||value[25]||value[26])return fail("Startingpoint Commit created invalid original Bug");
                }
            }
        }
    }
    // Commit calls AddBugsFromTech(known,true), then (total,false). Each call
    // can append, strengthen one existing Bug or find no eligible target.
    if(appended+strengthened>uint64_t(total)+uint64_t(known)||discovered>uint64_t(known))
        return fail("Startingpoint Commit Bug effects exceed original Map attempts");
    error.clear();return true;
}
}
