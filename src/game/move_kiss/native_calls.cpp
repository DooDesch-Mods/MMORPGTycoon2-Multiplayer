#include "move_kiss.h"
#include "../object_identity/remove_runtime/owned_undo_open.h"
#include <type_traits>
#include <windows.h>
#include <limits>
#include <cstring>
#include <exception>

namespace mt2mp::move_kiss {
namespace {
void invoke_pod(const Calls& c,uintptr_t target,const unsigned char* pose,Receipt& r){
    alignas(16) unsigned char loc[0x38]{};
    alignas(16) unsigned char action[0x130]{};
    r.started=true;
    __try {
        r.loc_attempted=true;c.loc_ctor(loc,"Move");r.loc_returned=true;
        r.begin_attempted=true;c.begin(loc);r.begin_returned=true;
        r.open_attempted=true;r.graph=reinterpret_cast<uintptr_t>(c.open(loc));r.open_returned=true;
        if(r.graph){
            r.ctor_attempted=true;c.move_ctor(action,reinterpret_cast<void*>(target),pose);r.ctor_returned=true;
            r.move_attempted=true;c.move_do(action,reinterpret_cast<void*>(r.graph));r.move_returned=true;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){r.exception=GetExceptionCode();}
    if(r.ctor_returned){
        r.dtor_attempted=true;
        __try{c.move_dtor(action);r.dtor_returned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}
    }
    if(r.open_returned&&r.graph){
        r.close_attempted=true;
        __try{c.close();r.close_returned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}
    }
    if(r.begin_returned){
        r.end_attempted=true;
        __try{c.end();r.end_returned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}
    }
    if(r.loc_returned){
        r.vector_attempted=true;
        __try{c.vector_dtor(loc+0x20);r.vector_returned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}
        r.string_attempted=true;
        __try{c.string_dtor(loc);r.string_returned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!r.cleanup_exception)r.cleanup_exception=GetExceptionCode();}
    }
}
}
bool execute_once(const Calls& c,uintptr_t target,const std::array<unsigned char,0x6c>& pose,Receipt& r,std::string& error){
    if(r.started){error="move_native_already_attempted";return false;}
    if(!target||!c.loc_ctor||!c.begin||!c.open||!c.move_ctor||!c.move_do||!c.move_dtor||!c.close||!c.end||!c.vector_dtor||!c.string_dtor){error="move_native_calls_incomplete";return false;}
    invoke_pod(c,target,pose.data(),r);
    if(r.exception||r.cleanup_exception){error="move_native_exception="+std::to_string(r.exception)+" cleanup_exception="+std::to_string(r.cleanup_exception);return false;}
    if(!r.graph){error="move_native_undo_open_returned_null";return false;}
    if(!r.move_returned){error="move_native_effect_not_returned";return false;}
    return true;
}
bool resolve_calls(Memory& memory,uintptr_t image,const std::string& sha,Calls& out,std::string& error){
    out={};
    if(sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||!image||image>(std::numeric_limits<uintptr_t>::max)()-0x11bd1e0){error="move_native_exe_identity";return false;}
    struct Guard{uintptr_t rva;const char* hex;};
    constexpr Guard guards[]{
        {0xa16f10,"555756534883ec38"},{0x265300,"5756534883ec404889cb"},
        {0x539990,"56534883ec2848833de210500100"},{0x55e8e0,"4157415641554154555756534883ec68"},
        {0x55ed80,"555756534883ec484889cf"},{0xcf1600,"488d05e959a900488901"},
        {0x5379c0,"534883ec20488b1db4305001"},{0x265650,"4157415641554154555756534881ec98020000"},
        {0x11b2b00,"555756534883ec28488b7908488b1948"},{0x11bd1e0,"488b014883c1104839c8740c4889c1e9"},
        {0x5c2a1b,"e890fbffff84c074ba"},{0x5c0780,"31c04883c4585b5ec3"},
        {0x55edf2,"e86919feff"},{0x55eeb6,"4883c4485b5e5f5de9ed5fd7ff"}
    };
    Open checked_open=reinterpret_cast<Open>(image+0x539990);
    static_assert(std::is_same_v<Open,remove_runtime::native::UndoOpenFn>);
    try{
        for(const auto& g:guards){
            const size_t n=std::strlen(g.hex)/2;unsigned char bytes[32]{};
            if(!n){error="move_native_guard_missing="+std::to_string(g.rva);return false;}
            if(!memory.read(image+g.rva,bytes,n,error))return false;
            auto digit=[](char v){return v<='9'?v-'0':v-'a'+10;};
            bool same=true;
            for(size_t i=0;i<n;++i)if(bytes[i]!=(digit(g.hex[i*2])*16+digit(g.hex[i*2+1])))same=false;
            if(!same){
                const auto cause="move_native_guard_mismatch="+std::to_string(g.rva);
                if(g.rva!=0x539990){error=cause;return false;}
                std::string owned_error;
                if(!remove_runtime::read_owned_undo_open(image,checked_open,owned_error)||checked_open!=reinterpret_cast<Open>(image+g.rva)){
                    error=cause+"; "+(owned_error.empty()?"remove_runtime_owned_Open_missing_exact_entry":owned_error);return false;
                }
            }
        }
    }catch(const std::exception& x){error=std::string("move_native_resolve_exception: ")+x.what();return false;}
    catch(...){error="move_native_resolve_nonstandard_exception";return false;}
    out={reinterpret_cast<Ctor>(image+0xa16f10),reinterpret_cast<Begin>(image+0x265300),checked_open,
         reinterpret_cast<MoveCtor>(image+0x55e8e0),reinterpret_cast<MoveDo>(image+0x55ed80),reinterpret_cast<Dtor>(image+0xcf1600),
         reinterpret_cast<Finish>(image+0x5379c0),reinterpret_cast<Finish>(image+0x265650),reinterpret_cast<Dtor>(image+0x11b2b00),reinterpret_cast<Dtor>(image+0x11bd1e0)};
    return true;
}
}
