#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <Windows.h>

namespace mt2mp::game::dungeon_cells_calls {
struct Text {const char* data=nullptr;uint64_t size=0,capacity=0,padding=0;};
// The pinned _StartEdit body accesses only these private storage offsets. It
// never dispatches through a Cursor vtable or changes the current view Shard.
struct alignas(16) EditStorage {std::array<unsigned char,0x350> bytes{};};
struct Start {uintptr_t graph=0,marker=0;bool entered=false,returned=false,opened=false;};
inline uint32_t start(uintptr_t image,uintptr_t dungeon,EditStorage& storage,Start& receipt){
    receipt={};const Text label{"Dungeon tiles",13};
    std::memcpy(storage.bytes.data()+0x2c0,&dungeon,sizeof dungeon);
    __try{receipt.entered=true;reinterpret_cast<void(*)(void*,const Text&)>(image+0x59d230)(&storage,label);receipt.returned=true;
        std::memcpy(&receipt.graph,storage.bytes.data()+0x338,sizeof receipt.graph);
        std::memcpy(&receipt.marker,storage.bytes.data()+0x348,sizeof receipt.marker);
        if(receipt.marker)receipt.opened=*reinterpret_cast<const unsigned char*>(receipt.marker)!=0;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){
        std::memcpy(&receipt.graph,storage.bytes.data()+0x338,sizeof receipt.graph);
        std::memcpy(&receipt.marker,storage.bytes.data()+0x348,sizeof receipt.marker);return GetExceptionCode();}
}
inline uint32_t active(uintptr_t image,uintptr_t graph){
    __try{reinterpret_cast<void(*)(void*)>(image+0x537840)(reinterpret_cast<void*>(graph));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t closed(uintptr_t image,uintptr_t dungeon,int32_t cell,bool& result){
    __try{result=reinterpret_cast<bool(*)(void*,int32_t)>(image+0xa8320)(reinterpret_cast<void*>(dungeon),cell);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t opened(uintptr_t image,uintptr_t dungeon,int32_t cell,const Text& theme,bool& result){
    __try{result=reinterpret_cast<bool(*)(void*,int32_t,const Text&)>(image+0xa84e0)(reinterpret_cast<void*>(dungeon),cell,theme);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t theme(uintptr_t image,uintptr_t dungeon,int32_t cell,const Text& value){
    __try{reinterpret_cast<void(*)(void*,int32_t,const Text&)>(image+0xa81d0)(reinterpret_cast<void*>(dungeon),cell,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t colors(uintptr_t image,uintptr_t dungeon,int32_t cell){
    __try{reinterpret_cast<void(*)(void*,int32_t)>(image+0xa4c60)(reinterpret_cast<void*>(dungeon),cell);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t can_open(uintptr_t image,uintptr_t dungeon,int32_t cell,bool& result){
    __try{result=reinterpret_cast<bool(*)(const void*,int32_t)>(image+0xa41f0)(reinterpret_cast<const void*>(dungeon),cell);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t obstructed(uintptr_t image,uintptr_t dungeon,int32_t cell,bool opening,bool& result){
    // The exact original const method does not read its Cursor receiver.
    __try{result=reinterpret_cast<bool(*)(const void*,void*,int32_t,bool)>(image+0x59d390)(nullptr,reinterpret_cast<void*>(dungeon),cell,opening);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t drain(uintptr_t image,uintptr_t graph){
    __try{reinterpret_cast<void(*)(void*)>(image+0x544680)(reinterpret_cast<void*>(graph));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t finished(uintptr_t image,uintptr_t dungeon){
    __try{reinterpret_cast<void(*)(void*,bool)>(image+0xa8b80)(reinterpret_cast<void*>(dungeon),true);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t release_marker(uintptr_t image,Start& receipt){
    if(!receipt.marker)return 0;
    __try{reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(receipt.marker),1);receipt.marker=0;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t end(uintptr_t image){
    __try{reinterpret_cast<void(*)()>(image+0x265650)();return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Commit {bool constructed=false,called=false,returned=false;uint32_t arguments_cleanup=0,string_cleanup=0;};
inline uint32_t commit(uintptr_t image,uintptr_t undo,uintptr_t graph,Commit& receipt){
    alignas(16) unsigned char label[0x38]{};uint32_t exception=0;
    __try{reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Edit dungeon tiles");receipt.constructed=true;
        receipt.called=true;reinterpret_cast<void(*)(void*,const void*,void*)>(image+0x53b260)(reinterpret_cast<void*>(undo),label,reinterpret_cast<void*>(graph));receipt.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(receipt.constructed){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){receipt.arguments_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){receipt.string_cleanup=GetExceptionCode();}
    }return exception;
}
inline uint32_t theme_lookup(uintptr_t image,uintptr_t manager,const Text& key,uintptr_t& theme){
    __try{theme=reinterpret_cast<uintptr_t(*)(void*,const Text&)>(image+0xcab90)(reinterpret_cast<void*>(manager),key);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t brush(uintptr_t image,void* tile,void* cursor,const void* position,float distance){
    __try{reinterpret_cast<void(*)(void*,void*,const void*,float)>(image+0x59c8d0)(tile,cursor,position,distance);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t hit(uintptr_t image,uintptr_t collision,uintptr_t& dungeon,int32_t& cell){
    __try{dungeon=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x523730)(reinterpret_cast<const void*>(collision));
        cell=reinterpret_cast<int32_t(*)(const void*)>(image+0x523750)(reinterpret_cast<const void*>(collision));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t selected(uintptr_t image,uintptr_t window,uintptr_t rows,uintptr_t& row){
    __try{const auto index=reinterpret_cast<int32_t(*)(const void*)>(image+0x742ff0)(reinterpret_cast<const void*>(window));
        if(index<0){row=0;return 0;}
        row=reinterpret_cast<uintptr_t(*)(const void*,int32_t)>(image+0xc4d290)(reinterpret_cast<const void*>(rows),index);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
