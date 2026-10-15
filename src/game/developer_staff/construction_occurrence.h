#pragma once
#include "construction_types.h"
#include "job_census.h"
namespace mt2mp::game::developer_staff::construction {
// Expiration can retain the old weak pair or clear both fields in the original
// getter. Neither form permits dereferencing the old Building or a freed proxy.
inline bool occurrence(const LocalJob& link,const job_census::Job& job,
                       uintptr_t live_building,uintptr_t live_region,
                       const job_census::Read& read,std::string& error){
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(link.retired||!link.job||!link.building||!link.building_proxy||!link.region||
       job.pointer!=link.job||job.region!=link.region)
        return fail("Construction original occurrence receiver/weak proxy/Region changed");
    if(job.type!=1||job.valid!=1||job.bug.pointer||job.monster||job.monster_proxy||(job.developer&&!job.reciprocal))
        return fail("Construction selected original Job type/valid/reciprocal shape differs");
    if(!job.building&&!job.building_proxy){
        if(link.target_alive){
            if(!link.target_restored||!live_building||!live_region)
                return fail("Construction cleared original reference requires confirmed separate restored target");
        }else if(live_building||live_region)
            return fail("Construction cleared original reference still has a live removed target");
        return true;
    }
    if(job.building!=link.building||job.building_proxy!=link.building_proxy)
        return fail("Construction original occurrence receiver/weak proxy/Region changed");
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
