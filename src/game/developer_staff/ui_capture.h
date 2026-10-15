#pragma once
#include <exception>
#include <string>
namespace mt2mp::game::developer_staff {
enum class UiIntent {Other,Hire,Fire,Unreadable};
struct IntentReceipt {bool suppressed=false,captured=false;UiIntent intent=UiIntent::Other;std::string action_uuid,cause;};
template<class Calls>
bool capture_ui(bool cooperative,bool mainthread,UiIntent intent,Calls& calls,IntentReceipt& receipt){
    receipt={};receipt.intent=intent;
    if(!cooperative||!mainthread||intent==UiIntent::Other)return calls.original();
    receipt.suppressed=true;
    if(intent==UiIntent::Unreadable){receipt.cause="Developer UI command unreadable; original suppressed before cash";return true;}
    try{
        if(!calls.admit(intent,receipt.cause))return true;
        if(!calls.uuid(receipt.action_uuid,receipt.cause))return true;
        receipt.captured=calls.retain(intent,receipt.action_uuid,receipt.cause);
        if(!receipt.captured&&receipt.cause.empty())receipt.cause="Developer UI intent rejected without original cause";
    }catch(const std::exception& ex){receipt.cause=std::string("Developer UI intent exception: ")+ex.what();}
    catch(...){receipt.cause="Developer UI intent nonstandard exception";}
    return true;
}
}
