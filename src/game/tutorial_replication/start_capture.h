#pragma once
#include <cstdint>
#include <exception>
#include <string>

namespace mt2mp::game::tutorial_replication::start_intent {
inline constexpr uintptr_t pay_rva=0x262ec0,ui_return_rva=0x6c7e02;
enum class Route {Original,ClientIntent};
inline Route route(bool joined_client,bool main_thread,uintptr_t image,uintptr_t return_address){
    return joined_client&&main_thread&&image&&image<=UINTPTR_MAX-ui_return_rva&&return_address==image+ui_return_rva?
        Route::ClientIntent:Route::Original;
}
struct Receipt {
    bool original_attempted=false,suppressed=false,submitted=false;
    std::string action_uuid,cause;
};
// Exact Client UI origin is selected before capture. Failed ownership or submit
// must never fall through to Pay and alter the local purchased flag.
template<class Calls>
bool dispatch(Route selected,Calls& calls,Receipt& receipt){
    receipt={};
    if(selected==Route::Original){receipt.original_attempted=true;return calls.original();}
    receipt.suppressed=true;
    try{
        if(!calls.capture(receipt.cause)){if(receipt.cause.empty())receipt.cause="Startingpoint StartTech capture rejected without cause";return false;}
        if(!calls.action_uuid(receipt.action_uuid,receipt.cause)){if(receipt.cause.empty())receipt.cause="Startingpoint StartTech UUID creation failed without cause";return false;}
        receipt.submitted=calls.submit(receipt.action_uuid,receipt.cause);
        if(!receipt.submitted&&receipt.cause.empty())receipt.cause="Startingpoint StartTech Controller submission rejected without cause";
    }catch(const std::exception& cause){receipt.cause=std::string("Startingpoint StartTech intent failed: ")+cause.what();}
    catch(...){receipt.cause="Startingpoint StartTech intent failed with nonstandard exception";}
    return false;
}
}
