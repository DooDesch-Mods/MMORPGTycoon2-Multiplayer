#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::monster_zone_theme_find {
struct Receipt {bool constructed=false,returned=false,cleaned=false,unknown=false;uint32_t exception=0,cleanup_exception=0;};
template<class Calls>bool resolve(Calls& calls,uintptr_t& window,Receipt& receipt,std::string& error){
    window=0;receipt={};
    receipt.exception=calls.construct();
    if(!receipt.exception){
        receipt.constructed=true;
        receipt.exception=calls.find(window);
        receipt.returned=receipt.exception==0;
        receipt.cleanup_exception=calls.cleanup();
        receipt.cleaned=receipt.cleanup_exception==0;
    }
    if(receipt.exception||receipt.cleanup_exception){
        receipt.unknown=true;
        error="Zone original Window.Find temporary lifetime Unknown exception="+std::to_string(receipt.exception)+
            " cleanup_exception="+std::to_string(receipt.cleanup_exception)+
            " constructed="+std::to_string(receipt.constructed)+" returned="+std::to_string(receipt.returned)+
            " cleaned="+std::to_string(receipt.cleaned)+"; no replay";
        return false;
    }
    return true;
}
}
