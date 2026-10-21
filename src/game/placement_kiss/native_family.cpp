#include "native_family.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include <limits>
#include <set>

namespace placement_kiss::family {
namespace {
struct StringView { const char* data; std::size_t size; std::byte rest[16]{}; };
struct Identifier { StringView type, variant; };
static_assert(sizeof(StringView)==32 && sizeof(Identifier)==64);
bool fail(std::string& cause, const char* text) { cause=text; return false; }
bool read(const void* address,void* output,std::size_t size,std::uint32_t& exception) noexcept {
    const auto p=reinterpret_cast<std::uintptr_t>(address);
    if(!p || p>(std::numeric_limits<std::uintptr_t>::max)()-size) return false;
    __try { std::memcpy(output,address,size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode(); return false; }
}
template<class T> bool at(const void* address,std::size_t offset,T& output,std::uint32_t& exception) {
    const auto p=reinterpret_cast<std::uintptr_t>(address);
    return p && p<=(std::numeric_limits<std::uintptr_t>::max)()-offset && read(reinterpret_cast<const void*>(p+offset),&output,sizeof(output),exception);
}
bool text(const void* address,std::string& output,std::uint32_t& exception) {
    StringView s{}; char bytes[256]{};
    if(!read(address,&s,sizeof(s),exception)||!s.data||!s.size||s.size>sizeof(bytes)||!read(s.data,bytes,s.size,exception))return false;
    output.assign(bytes,s.size);return output.find('\0')==std::string::npos;
}
bool item(IdentifierAt fn,void* header,int index,void*& output,std::uint32_t& exception) noexcept {
    __try { output=fn(header,index);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode();return false; }
}
bool clear(Clear fn,void* header,std::uint32_t& exception) noexcept {
    __try { fn(header);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode();return false; }
}
bool add(Add fn,void* header,const void* value,std::uint32_t& exception) noexcept {
    __try { fn(header,value);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode();return false; }
}
bool selection(void* receiver,std::int32_t index,std::uint32_t& exception) noexcept {
    __try {
        auto* p=static_cast<std::byte*>(receiver);
        std::memcpy(p+0x6cc,&index,sizeof(index));
        p[0x6d0]=index?std::byte{1}:std::byte{0};
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {exception=GetExceptionCode();return false;}
}
}
bool definition(void* value,std::uintptr_t image,const Request& request,void*& variants,std::int32_t& count,std::string& cause,std::uint32_t& exception) {
    variants=nullptr;count=0;std::uintptr_t table=0;std::uint32_t flags=0;std::int32_t capacity=0;std::string type;
    if(!image||image>(std::numeric_limits<std::uintptr_t>::max)()-0x172ae50||!at(value,0,table,exception)||table!=image+0x172ae50)return fail(cause,"error: actual BuildingDefinition class differs");
    const auto base=reinterpret_cast<std::uintptr_t>(value);
    if(base>(std::numeric_limits<std::uintptr_t>::max)()-0x188)return fail(cause,"error: actual BuildingDefinition address overflows");
    if(!text(reinterpret_cast<void*>(base+0x18),type,exception)||type!=request.type)return fail(cause,"error: actual BuildingDefinition type differs");
    if(type=="Scenery")return fail(cause,"error: actual BuildingDefinition is the Scenery definition");
    if(!at(value,0x16c,flags,exception))return fail(cause,"error: actual BuildingDefinition feature flags unreadable");
    if(flags&(1u<<13))return fail(cause,"error: actual BuildingDefinition is Scenery");
    const auto dungeon_flags=flags&((1u<<16)|(1u<<17));
    if(dungeon_flags){
        if(!request.dungeon_family)return fail(cause,"error: actual BuildingDefinition uses a dungeon branch");
        if((type=="DungeonExterior"&&dungeon_flags!=(1u<<16))||
           (type=="DungeonPortal"&&dungeon_flags!=(1u<<17))||
           (type!="DungeonExterior"&&type!="DungeonPortal"))
            return fail(cause,"error: actual Dungeon Building type/feature differs");
    }else if(type=="DungeonExterior"||type=="DungeonPortal")
        return fail(cause,"error: actual Dungeon Building definition lacks its native feature");
    if(((flags&(1u<<2))!=0)!=(type=="StartingPoint"))return fail(cause,"error: actual StartingPoint definition type/feature differs");
    if(!at(value,0x178,variants,exception)||!at(value,0x180,count,exception)||!at(value,0x184,capacity,exception)||!variants||count<=0||capacity<count)return fail(cause,"error: actual BuildingDefinition variant array invalid");
    std::set<std::string> names;bool found=false;
    for(std::int32_t i=0;i<count;++i) {
        void* variant=nullptr;std::uintptr_t variant_table=0;std::string name;
        if(!at(variants,std::size_t(i)*8,variant,exception)||!at(variant,0,variant_table,exception)||variant_table!=image+0x1718550)return fail(cause,"error: actual BuildingVariant class differs");
        const auto p=reinterpret_cast<std::uintptr_t>(variant);
        if(p>(std::numeric_limits<std::uintptr_t>::max)()-0x38||!text(reinterpret_cast<void*>(p+0x18),name,exception))return fail(cause,"error: actual BuildingVariant identifier unreadable");
        if(!names.insert(name).second)return fail(cause,"error: actual BuildingDefinition variant is ambiguous");
        if(name==request.variant)found=true;
    }
    return found||fail(cause,"error: requested variant is absent from actual BuildingDefinition");
}
bool select_identifier(IdentifierAt fn,void* header,const Request& request,std::int32_t& index,void*& identifier,std::string& cause,std::uint32_t& exception) {
    index=-1;identifier=nullptr;void* data=nullptr;std::int32_t count=0,capacity=0;
    if(!fn||!at(header,8,data,exception)||!at(header,16,count,exception)||!at(header,20,capacity,exception)||!data||count<=0||capacity<count)return fail(cause,"error: native identifier array invalid");
    const auto base=reinterpret_cast<std::uintptr_t>(data);
    if(base>(std::numeric_limits<std::uintptr_t>::max)()-std::size_t(count)*64)return fail(cause,"error: native identifier array address overflows");
    for(std::int32_t i=0;i<count;++i) {
        void* row=nullptr;std::string type,variant;
        if(!item(fn,header,i,row,exception)||row!=reinterpret_cast<void*>(base+std::size_t(i)*64))return fail(cause,"error: native identifier getter differs from owned array");
        if(!text(row,type,exception)||!text(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(row)+32),variant,exception))return fail(cause,"error: native identifier strings unreadable");
        if(type==request.type&&variant==request.variant) {
            if(index!=-1)return fail(cause,"error: requested native identifier is ambiguous");
            index=i;identifier=row;
        }
    }
    return index>=0||fail(cause,"error: requested native identifier is absent after original filtering");
}
bool populate_identifiers(ArrayCalls calls,void* header,void* value,std::uintptr_t image,const Request& request,std::string& cause,std::uint32_t& exception) {
    void* variants=nullptr;std::int32_t count=0;
    if(!calls.variant||!calls.clear||!calls.add)return fail(cause,"error: native variant array methods unavailable");
    if(!definition(value,image,request,variants,count,cause,exception))return false;
    if(!clear(calls.clear,header,exception))return fail(cause,"error: private native identifier Clear failed");
    for(std::int32_t i=0;i<count;++i) {
        void* variant=nullptr;void* owned=nullptr;
        if(!at(variants,std::size_t(i)*8,owned,exception)||!item(calls.variant,value,i,variant,exception)||variant!=owned)return fail(cause,"error: native GetVariant differs from definition ownership");
        Identifier row{{request.type.data(),request.type.size(),{}},{}};
        if(!at(variant,0x18,row.variant,exception))return fail(cause,"error: native variant string view unreadable");
        // AddItem copies both native strings into the C1-owned array.
        if(!add(calls.add,header,&row,exception))return fail(cause,"error: private native identifier AddItem failed");
    }
    std::int32_t after=0;
    if(!at(header,16,after,exception)||after!=count)return fail(cause,"error: private native identifier population count differs");
    return true;
}
bool set_private_selection(void* receiver,std::int32_t index,std::string& cause,std::uint32_t& exception) {
    std::int32_t count=0,after=-1;void* window=nullptr;void* tooltip=nullptr;unsigned char palette=0;
    if(!at(receiver,0x1b0,count,exception)||index<0||index>=count)return fail(cause,"error: private filtered selection index out of bounds");
    if(!at(receiver,0x3a8,window,exception)||!at(receiver,0x3b0,tooltip,exception)||window||tooltip)return fail(cause,"error: private Build unexpectedly owns live palette UI");
    if(!selection(receiver,index,exception))return fail(cause,"error: private palette selection write failed");
    if(!at(receiver,0x6cc,after,exception)||after!=index||!at(receiver,0x6d0,palette,exception)||palette!=(index?1:0))return fail(cause,"error: private palette selection readback differs");
    return true;
}
}
