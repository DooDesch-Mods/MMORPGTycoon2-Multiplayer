#pragma once
#include "region_rng_snapshot.h"
#include "region_saved_identity.h"

namespace mt2mp::game::region_rng_capture {
template<class Read>
bool capture(uintptr_t image,uintptr_t map,region_rng_snapshot::Manifest& out,Read&& read,std::string& error){
    out={};const auto root=map+0x1c0;
    region_saved_identity::Index index;
    if(!region_saved_identity::capture(image,map,root,index,read,error))return false;
    region_rng_snapshot::Manifest next;next.present=true;
    for(const auto& [id,region]:index.saved){
        uintptr_t source=0;region_rng_snapshot::Row row;row.region=id;
        if(!read(region,0x160,source,error)||!read(region,0x268,row.words,error)||
           !read(region,0x288,row.extra,error))return false;
        if(source!=region+0x268)return region_rng_snapshot::fail(error,"Region RNG Grid source differs from embedded owner id="+std::to_string(id));
        next.rows.push_back(row);
    }
    std::sort(next.rows.begin(),next.rows.end(),[](const auto& a,const auto& b){return a.region<b.region;});
    if(!region_rng_snapshot::valid(next,error)||!region_saved_identity::verify(image,map,root,index,read,error))return false;
    out=std::move(next);return true;
}
template<class Read>
bool unchanged(uintptr_t image,uintptr_t map,const region_rng_snapshot::Manifest& before,Read&& read,std::string& error){
    region_rng_snapshot::Manifest after;
    if(!before.present||!region_rng_snapshot::valid(before,error))
        return region_rng_snapshot::fail(error,"Region RNG capture lacks its complete pre-save census: "+error);
    if(!capture(image,map,after,read,error))return false;
    if(after!=before)return region_rng_snapshot::fail(error,"Region RNG state or complete saved ID census changed during held native save");
    return true;
}
}
