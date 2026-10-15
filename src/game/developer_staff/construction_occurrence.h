#pragma once
#include "construction_types.h"
#include "job_census.h"
namespace mt2mp::game::developer_staff::construction {
// A tombstone uses only the retained receiver values and the live weak proxy.
// Its cached Building receiver must never be dereferenced after removal.
inline bool occurrence(const LocalJob& link,const job_census::Job& job,
                       uintptr_t live_building,uintptr_t live_region,
                       const job_census::Read& read,std::string& error){
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(link.retired||!link.building||!link.building_proxy||!link.region||
       job.pointer!=link.job||job.building!=link.building||job.building_proxy!=link.building_proxy||job.region!=link.region)
        return fail("Construction original occurrence receiver/weak proxy/Region changed");
    if(job.type!=1||!job.valid||job.bug.pointer||job.monster||job.monster_proxy||(job.developer&&!job.reciprocal))
        return fail("Construction selected original Job type/valid/reciprocal shape differs");
    uintptr_t weak=0;
    if(!read(job.building_proxy,&weak,sizeof(weak),error))return false;
    if(link.target_alive){
        if(link.target_restored){
            if(!live_building||!live_region||weak)
                return fail("Construction restored Building must remain separate from expired original Job");
        }else if(live_building!=link.building||live_region!=link.region||weak!=live_building+0x20)
            return fail("Construction selected Job live Building weak target differs");
    }else if(live_building||live_region||weak)
        return fail("Construction removed Building target is not expired");
    return true;
}
}
