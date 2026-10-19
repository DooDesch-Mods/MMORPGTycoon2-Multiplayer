#pragma once
#include "move_kiss/move_kiss.h"
#include <Windows.h>
namespace mt2mp::selection_move_native {
struct Target {uintptr_t object=0;std::array<unsigned char,0x6c> pose{};};
struct Receipt {
    move_kiss::Receipt scope;
    size_t constructed=0,moved=0,destroyed=0;
};
inline void invoke(const move_kiss::Calls& calls,const Target* targets,size_t count,Receipt& receipt){
    alignas(16) unsigned char loc[0x38]{};alignas(16) unsigned char operation[0x130]{};
    auto& r=receipt.scope;r.started=true;bool current_constructed=false;
    __try{
        r.loc_attempted=true;calls.loc_ctor(loc,"Move");r.loc_returned=true;
        r.begin_attempted=true;calls.begin(loc);r.begin_returned=true;
        r.open_attempted=true;r.graph=uintptr_t(calls.open(loc));r.open_returned=true;
        if(r.graph)for(size_t i=0;i<count;++i){
            r.ctor_attempted=true;calls.move_ctor(operation,reinterpret_cast<void*>(targets[i].object),targets[i].pose.data());
            current_constructed=true;++receipt.constructed;r.ctor_returned=true;
            r.move_attempted=true;calls.move_do(operation,reinterpret_cast<void*>(r.graph));++receipt.moved;r.move_returned=true;
            r.dtor_attempted=true;current_constructed=false;calls.move_dtor(operation);++receipt.destroyed;r.dtor_returned=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){r.exception=GetExceptionCode();}
    if(current_constructed){
        r.dtor_attempted=true;
        __try{calls.move_dtor(operation);++receipt.destroyed;r.dtor_returned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){r.cleanup_exception=GetExceptionCode();}
    }
    if(r.open_returned&&r.graph){r.close_attempted=true;
        __try{calls.close();r.close_returned=true;}__except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}}
    if(r.begin_returned){r.end_attempted=true;
        __try{calls.end();r.end_returned=true;}__except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}}
    if(r.loc_returned){r.vector_attempted=true;
        __try{calls.vector_dtor(loc+0x20);r.vector_returned=true;}__except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}
        r.string_attempted=true;
        __try{calls.string_dtor(loc);r.string_returned=true;}__except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}}
}
inline bool execute(const move_kiss::Calls& c,const std::vector<Target>& targets,Receipt& receipt,std::string& error){
    if(receipt.scope.started){error="Selection Move native action already attempted; no replay";return false;}
    if(targets.size()<2||!c.loc_ctor||!c.begin||!c.open||!c.move_ctor||!c.move_do||!c.move_dtor||!c.close||!c.end||!c.vector_dtor||!c.string_dtor){error="Selection Move native calls incomplete";return false;}
    for(const auto& target:targets)if(!target.object){error="Selection Move native target absent";return false;}
    invoke(c,targets.data(),targets.size(),receipt);const auto& r=receipt.scope;
    if(r.exception||r.cleanup_exception){error="Selection Move native SEH="+std::to_string(r.exception)+" cleanup="+std::to_string(r.cleanup_exception);return false;}
    if(!r.graph||receipt.constructed!=targets.size()||receipt.moved!=targets.size()||receipt.destroyed!=targets.size()||
       !r.close_returned||!r.end_returned||!r.vector_returned||!r.string_returned){error="Selection Move original complete graph/cleanup did not return";return false;}
    return true;
}
}
