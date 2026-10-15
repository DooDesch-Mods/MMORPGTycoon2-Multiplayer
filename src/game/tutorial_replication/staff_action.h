#pragma once
#include "startingpoint.h"
#include <cstdint>
#include <string>

namespace mt2mp::game::tutorial_replication::staff {
inline constexpr uintptr_t allocate_rva=0x25ec40,ui_return_rva=0x6c82f9;
inline bool value(int32_t desired){return desired>=0&&desired<=50;}
inline bool rows(const startingpoint::Row& before,const startingpoint::Row& after,int32_t desired,std::string& error){
    if(!startingpoint::valid(before,error)||!startingpoint::valid(after,error))return false;
    if(!value(desired)||!before.project_present||!before.purchased||before.deployed||before.custom){error="Startingpoint staff requires current purchased project and desired workers0..50";return false;}
    auto expected=before;expected.desired_workers=desired;
    if(after!=expected){error="Startingpoint staff changed fields outside desired workers";return false;}
    return true;
}
inline bool capture_route(bool client,bool main_thread,uintptr_t image,uintptr_t caller){
    return client&&main_thread&&image&&image<=UINTPTR_MAX-ui_return_rva&&caller==image+ui_return_rva;
}
struct Receipt{bool suppressed=false,submitted=false;int32_t desired=0;std::string action_uuid,cause;};
template<class Calls>
void capture(bool selected,int32_t desired,Calls& calls,Receipt& receipt){
    receipt={};
    if(!selected){calls.original();return;}
    receipt.suppressed=true;receipt.desired=desired;
    try{
        if(!calls.pin(receipt.cause)){if(receipt.cause.empty())receipt.cause="Startingpoint staff capture rejected without cause";return;}
        if(!calls.uuid(receipt.action_uuid,receipt.cause))return;
        receipt.submitted=calls.submit(receipt.action_uuid,receipt.cause);
        if(!receipt.submitted&&receipt.cause.empty())receipt.cause="Startingpoint staff submission rejected without cause";
    }catch(const std::exception& e){receipt.cause=std::string("Startingpoint staff intent failed: ")+e.what();}
    catch(...){receipt.cause="Startingpoint staff intent failed with nonstandard exception";}
}
}
