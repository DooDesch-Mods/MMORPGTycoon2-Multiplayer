#pragma once
#include "dungeon_input_calls.h"
#include <intrin.h>
namespace mt2mp::game::dungeon_input_action_calls {
struct LevelSelection {uintptr_t window=0,pane=0,view=0;std::array<int32_t,2> range{};bool fallback=false;};
inline uint32_t monster_level(uintptr_t image,uintptr_t receiver,LevelSelection& out)noexcept{
    __try{
        out={};out.window=*reinterpret_cast<const uintptr_t*>(receiver+0x150);if(!out.window)return 0xe0010003;
        const auto table=*reinterpret_cast<const uintptr_t*>(out.window);
        if(*reinterpret_cast<const uintptr_t*>(table+0xf0)!=image+0x81d860)return 0xe0010003;
        const dungeon_input_calls::Text name{"level_view",10};
        out.pane=reinterpret_cast<uintptr_t(*)(void*,const dungeon_input_calls::Text&)>(image+0x81d860)(reinterpret_cast<void*>(out.window),name);
        if(!out.pane){out.fallback=true;out.range.fill(*reinterpret_cast<const int32_t*>(receiver+0x350));return 0;}
        out.view=reinterpret_cast<uintptr_t(*)(const void*,const void*,const void*,ptrdiff_t)>(image+0x12654f0)
            (reinterpret_cast<const void*>(out.pane),reinterpret_cast<const void*>(image+0x166bda0),reinterpret_cast<const void*>(image+0x1665320),0);
        if(!out.view)return 0xe0010003;
        // GetLevelRange uses the original GCC result-buffer ABI at 5a84ac.
        reinterpret_cast<void(*)(void*,const void*)>(image+0x725970)(&out.range,reinterpret_cast<const void*>(out.view));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t fill_selection(uintptr_t image,uintptr_t window,bool& selected,uintptr_t& fill)noexcept{
    __try{
        selected=reinterpret_cast<bool(*)(const void*)>(image+0x6dd930)(reinterpret_cast<const void*>(window));
        if(selected)fill=reinterpret_cast<uintptr_t(*)(void*)>(image+0x6dd950)(reinterpret_cast<void*>(window));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t level(uintptr_t image,uintptr_t object,int32_t expected,int32_t desired,bool& stored)noexcept{
    __try{
        const auto table=*reinterpret_cast<const uintptr_t*>(object);
        if(table!=image+0x16c0770||*reinterpret_cast<const uintptr_t*>(table+0x470)!=image+0x3c06e0)return 0xe0010003;
        // The original DungeonMonster Release stores the selected native level
        // at 5a84b8/5a84f9, then dispatches this exact LevelChanged method.
        auto* target=reinterpret_cast<volatile long*>(object+0x6f8);
        if(_InterlockedCompareExchange(target,long(desired),long(expected))!=expected)return 0xe0010003;
        stored=true;reinterpret_cast<void(*)(void*)>(image+0x3c06e0)(reinterpret_cast<void*>(object));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t placement_clear(uintptr_t image,uintptr_t receiver,bool treasure=false)noexcept{
    __try{if(treasure)*reinterpret_cast<volatile unsigned char*>(receiver+0x749)=0;
        reinterpret_cast<void(*)(void*)>(image+0x5f8c20)(reinterpret_cast<void*>(receiver+0x448));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t treasure_sound(uintptr_t image)noexcept{
    __try{const auto sound=*reinterpret_cast<const uintptr_t*>(image+0x13df450);
        if(!sound)return 0xe0010003;
        reinterpret_cast<void(*)(void*,int32_t)>(image+0x50e810)(reinterpret_cast<void*>(sound),0x20);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t complete_monster_release(uintptr_t image,uintptr_t cursor,uintptr_t receiver,bool& cleared)noexcept{
    __try{
        if(*reinterpret_cast<const uintptr_t*>(cursor+0x190)!=receiver||*reinterpret_cast<const uintptr_t*>(receiver)!=image+0x1741060||
           *reinterpret_cast<const unsigned char*>(receiver+0x3d9)!=0)return 0xe0010003;
        // This is the exact UI-only common tail of original Release at 5a8333.
        // Warp, selection, preview and all native World objects stay untouched.
        auto* pressed=reinterpret_cast<volatile char*>(receiver+0x3da);
        if(_InterlockedCompareExchange8(pressed,0,1)!=1)return 0xe0010003;
        cleared=*pressed==0;return cleared?0:0xe0010003;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t begin(uintptr_t image,bool& opened,uint32_t& cleanup)noexcept{
    alignas(16) unsigned char label[0x20]{};bool constructed=false;uint32_t exception=0;
    __try{if(reinterpret_cast<bool(*)()>(image+0x264c30)()){
        reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(label,"Dungeon input");constructed=true;
        reinterpret_cast<void(*)(const void*)>(image+0x265300)(label);opened=true;
    }}__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(constructed)__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}
        __except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}
    return exception;
}
struct Commit {
    bool text_constructed=false,label_constructed=false,called=false,returned=false;
    uint32_t arguments_cleanup=0,label_cleanup=0,text_cleanup=0;
};
struct MonsterTail {bool achievement_checked=false,achievement_granted=false,sound_returned=false;};
inline uint32_t monster_tail(uintptr_t image,uintptr_t object,int32_t rank,uintptr_t sound,uintptr_t shard,
                             const std::array<float,3>& position,MonsterTail& receipt)noexcept{
    __try{
        if(rank==2){
            const auto definition=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x39cdf0)(reinterpret_cast<const void*>(object));
            if(definition&&reinterpret_cast<int32_t(*)(const void*)>(image+0x938c0)(reinterpret_cast<const void*>(definition))>1){
                reinterpret_cast<void(*)(int32_t)>(image+0x458e10)(15);receipt.achievement_granted=true;
            }
        }
        receipt.achievement_checked=true;
        reinterpret_cast<void(*)(void*,int32_t,void*,const void*)>(image+0x50eb70)
            (reinterpret_cast<void*>(sound),0x2f,reinterpret_cast<void*>(shard),position.data());
        receipt.sound_returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t commit(uintptr_t image,uintptr_t undo,uintptr_t graph,uintptr_t object,Commit& receipt)noexcept{
    alignas(16) unsigned char text[0x20]{},label[0x38]{};uint32_t exception=0;
    __try{
        if(object){
            reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(text,"{undo_msg_place}");receipt.text_constructed=true;
            reinterpret_cast<void(*)(void*,const void*,void*)>(image+0x538ae0)(label,text,reinterpret_cast<void*>(object));
        }else reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Edit dungeon paint");
        receipt.label_constructed=true;receipt.called=true;
        reinterpret_cast<void(*)(void*,const void*,void*)>(image+0x53b260)(reinterpret_cast<void*>(undo),label,reinterpret_cast<void*>(graph));
        receipt.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(receipt.label_constructed){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){receipt.arguments_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){receipt.label_cleanup=GetExceptionCode();}
    }
    if(receipt.text_constructed)__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(text);}
        __except(EXCEPTION_EXECUTE_HANDLER){receipt.text_cleanup=GetExceptionCode();}
    return exception;
}
}
