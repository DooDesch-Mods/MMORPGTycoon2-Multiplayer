#pragma once
#include "alert_lifecycle.h"

namespace mt2mp::game::scenario_vip_authority::client_alert::message_invoke {
using LocConstruct=void(*)(void*,const char*);
using Dispose=void(*)(void*);
using Factory=uintptr_t(*)(int32_t,const void*);
struct Functions {LocConstruct construct=nullptr;Factory factory=nullptr;Dispose arguments=nullptr,string=nullptr;};
inline int filter(uint32_t code){return code==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER;}
inline void create(const Functions& f,const char* text,MessageCalls& r,int32_t type=9){
    alignas(8) unsigned char storage[0x38]{};bool constructed=false;
    __try {
        __try {
            r.loc_construct.attempted=true;f.construct(storage,text);r.loc_construct.returned=true;constructed=true;
            r.create.attempted=true;r.create.result=f.factory(type,storage);r.create.returned=true;
        }__finally {
            if(constructed){
                __try{r.loc_arguments_destroy.attempted=true;f.arguments(storage+0x20);r.loc_arguments_destroy.returned=true;}
                __except(filter(GetExceptionCode())){r.loc_arguments_destroy.exception=GetExceptionCode();}
                __try{r.loc_string_destroy.attempted=true;f.string(storage);r.loc_string_destroy.returned=true;}
                __except(filter(GetExceptionCode())){r.loc_string_destroy.exception=GetExceptionCode();}
            }
        }
    }__except(filter(GetExceptionCode())){
        if(!r.loc_construct.returned)r.loc_construct.exception=GetExceptionCode();else r.create.exception=GetExceptionCode();
    }
}
}
