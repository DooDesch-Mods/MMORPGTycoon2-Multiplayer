#pragma once
#include "scenery_witness.h"
#include "prop_lease.h"
#include "region_saved_identity.h"
#include <algorithm>

namespace mt2mp::build_actions::private_native::host {
struct SavedSceneryIndexCapture {
    template<class Read>
    bool operator()(uintptr_t image,uintptr_t map,uintptr_t root,mt2mp::game::region_saved_identity::Index& out,Read&& read,std::string& error)const{
        return mt2mp::game::region_saved_identity::capture(image,map,root,out,read,error);
    }
};
struct SavedSceneryMemberCapture {
    bool operator()(Memory& memory,place::SceneryHost& owner,uintptr_t image,uintptr_t region,uintptr_t root,uintptr_t manager,
        uintptr_t object,uint64_t uid,std::string& error)const{
        place::SceneryWitness witness;
        if(!place::read_scenery(memory,owner,image,witness,error))return false;
        if(witness.context.region!=region||witness.context.shard!=root||witness.context.manager!=manager){error="saved_scenery_current_owner_context_differs";return false;}
        if(std::none_of(witness.members.begin(),witness.members.end(),[&](const auto& row){return row.object==object&&row.uid==uid;})){
            error="saved_scenery_absent_complete_category_census";return false;}
        return true;
    }
};
// This is a read-only snapshot member proof. Ordinary action admission still
// uses Reader::prop and its current Root collection policy.
template<class Typed,class CaptureIndex=SavedSceneryIndexCapture,class CaptureMember=SavedSceneryMemberCapture>
bool read_saved_root_scenery(Memory& memory,place::SceneryHost& owner,uintptr_t image,uintptr_t map,uintptr_t root,
    int32_t region_id,uintptr_t region,uintptr_t object,uint64_t uid,Typed&& typed,mt2mp::prop_lease::Prop& out,std::string& error,CaptureIndex capture_index={},CaptureMember capture_member={}){
    out={};
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(region_id<0||!region||!object||!uid)return fail("saved_scenery_exact_region_object_or_uid_missing");
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){
        if(!base||base>UINTPTR_MAX-offset){cause="saved_scenery_address_overflow";return false;}
        return memory.copy(base+offset,&value,sizeof value,cause);
    };
    mt2mp::game::region_saved_identity::Index before,after;
    if(!capture_index(image,map,root,before,at,error))return false;
    const auto member=std::find(before.saved.begin(),before.saved.end(),std::pair{region_id,region});
    if(member==before.saved.end())return fail("saved_scenery_exact_region_absent_saved_map");
    uintptr_t ap=0,shard=0,actual_region=0,manager=0;uint64_t full=0;
    if(!at(object,0,ap,error)||!at(object,0x18,full,error)||!at(object,0x170,shard,error)||
       !at(object,0x178,actual_region,error)||!at(region,0x960,manager,error))return false;
    if(ap!=image+0x16c1390||full!=uid||shard!=root||actual_region!=region||!manager)return fail("saved_scenery_class_uid_or_region_differs");
    if(!typed(object,uid,error))return false;
    if(!capture_member(memory,owner,image,region,root,manager,object,uid,error))return false;
    if(!typed(object,uid,error)||!capture_index(image,map,root,after,at,error))return false;
    if(before!=after)return fail("saved_scenery_complete_region_index_changed");
    uintptr_t final_ap=0,final_shard=0,final_region=0,final_manager=0;
    if(!at(object,0,final_ap,error)||!at(object,0x170,final_shard,error)||!at(object,0x178,final_region,error)||!at(region,0x960,final_manager,error))return false;
    if(final_ap!=ap||final_shard!=shard||final_region!=region||final_manager!=manager)return fail("saved_scenery_member_owner_changed");
    out={uid,reinterpret_cast<void*>(object),true,true,ap,"Scenery/47CategoryCensus/savedRootRegion"};error.clear();return true;
}
}
