#include "dungeon_build_cursor.h"
#include "dungeon_build_cursor_guard.h"
#include "dungeon_build_local_ui.h"
#include <iostream>
#include <map>
#include <stdexcept>

namespace cursor_fixture {
namespace cursor=mt2mp::game::dungeon_build_cursor;
namespace local=mt2mp::game::dungeon_build_local_ui;
unsigned checks=0;
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    uintptr_t unreadable=0;
    bool copy(uintptr_t pointer,void* output,size_t size,std::string& error){
        auto* target=static_cast<unsigned char*>(output);
        for(size_t i=0;i<size;++i){
            if(pointer+i==unreadable||!bytes.contains(pointer+i)){error="fixture native memory unavailable";return false;}
            target[i]=bytes.at(pointer+i);
        }
        return true;
    }
    template<class T>void put(uintptr_t pointer,const T& value){
        const auto* source=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i)bytes[pointer+i]=source[i];
    }
    void data(uintptr_t pointer,const std::vector<unsigned char>& value){
        for(size_t i=0;i<value.size();++i)bytes[pointer+i]=value[i];
    }
};
void guards(){
    constexpr uintptr_t image=0x140000000ULL,trampoline=0x150000000ULL,relay=0x140700000ULL,detour=0x160000000ULL;
    for(const auto& actual:cursor::codes){
        Memory memory;
        std::vector<unsigned char> body(actual.size,0x90);
        std::copy_n(actual.prefix.begin(),actual.copied,body.begin());
        const auto sha=mt2mp::sha256(body);
        auto code=actual;code.sha=sha.c_str();memory.data(image+code.rva,body);
        std::string error;
        need(cursor::guard(memory,image,code,0,detour,false,error),"pristine original: "+error);
        auto original=std::vector<unsigned char>(body.begin(),body.begin()+code.copied);
        original.insert(original.end(),{0xff,0x25,0,0,0,0});
        const auto resume=image+code.rva+code.copied;
        const auto* raw=reinterpret_cast<const unsigned char*>(&resume);
        original.insert(original.end(),raw,raw+8);
        memory.data(trampoline,original);
        need(cursor::guard(memory,image,code,trampoline,detour,false,error),"disabled instruction boundary: "+error);
        auto corrupt=memory;corrupt.bytes.at(trampoline+code.copied)=0x90;
        need(!cursor::guard(corrupt,image,code,trampoline,detour,false,error)&&
             error.find("instruction boundary")!=std::string::npos,"disabled copied boundary accepted");
        corrupt=memory;corrupt.put(trampoline+code.copied+6,resume+1);
        need(!cursor::guard(corrupt,image,code,trampoline,detour,false,error),"wrong original continuation accepted");
        corrupt=memory;corrupt.bytes.at(image+code.rva+code.size-1)^=1;
        need(!cursor::guard(corrupt,image,code,trampoline,detour,false,error)&&
             error.find("complete original body")!=std::string::npos,"body tail mutation accepted");
        memory.bytes.at(image+code.rva)=0xe9;
        const auto displacement=int32_t(int64_t(relay)-int64_t(image+code.rva+5));
        memory.put(image+code.rva+1,displacement);
        memory.data(relay,{0xff,0x25,0,0,0,0,0,0,0,0,0,0,0,0});memory.put(relay+6,detour);
        need(cursor::guard(memory,image,code,trampoline,detour,true,error),"enabled owned original: "+error);
        need(!cursor::guard(memory,image,code,0,detour,true,error),"enabled missing original accepted");
        corrupt=memory;corrupt.put(relay+6,detour+1);
        need(!cursor::guard(corrupt,image,code,trampoline,detour,true,error)&&
             error.find("another detour")!=std::string::npos,"foreign owned relay accepted");
        corrupt=memory;corrupt.bytes.at(image+code.rva+code.copied-1)^=1;
        need(!cursor::guard(corrupt,image,code,trampoline,detour,true,error),"uncopied live entry tail mutation accepted");
        corrupt=memory;corrupt.unreadable=trampoline;
        need(!cursor::guard(corrupt,image,code,trampoline,detour,true,error)&&
             error=="fixture native memory unavailable","original memory failure cause lost");
        auto wrong=code;wrong.copied=code.copied==7?8:7;
        need(!cursor::guard(memory,image,wrong,trampoline,detour,true,error),"shared wrong copied length accepted");
    }
}
void routes(){
    cursor::Scope scope;scope.exterior=true;scope.cursor=0x1000;scope.building=0x2000;std::string error;
    need(cursor::route(scope,false,0x5944e5,scope.cursor,0)==cursor::Route::Suppressed,"original behaviour not scoped");
    need(cursor::route(scope,true,0x5944f5,scope.cursor,scope.building)==cursor::Route::Suppressed&&cursor::complete(scope,error),
         "original ordered selection not scoped");
    need(cursor::route(scope,true,0x5944f5,scope.cursor,scope.building)==cursor::Route::Rejected&&!cursor::complete(scope,error),
         "reentrant original completion accepted");
    for(unsigned scenario=0;scenario<5;++scenario){
        cursor::Scope test;test.exterior=true;test.cursor=0x1000;test.building=0x2000;
        const bool selected=scenario==0;
        need(cursor::route(test,selected,selected?0x5944f5:0x5944e5,
             scenario==1?0x1001:test.cursor,scenario==2?1:0)==cursor::Route::Rejected||scenario>2,
             "wrong caller order/owner/argument accepted");
        if(scenario==3){test={};test.exterior=false;need(cursor::route(test,false,0x5944e5,0,0)==cursor::Route::Forward,"ordinary Build intercepted");}
        if(scenario==4){test={};test.exterior=true;need(cursor::route(test,false,0x5944e6,0,0)==cursor::Route::Forward,"foreign native caller intercepted");}
    }
    cursor::Scope missing;missing.exterior=true;need(!cursor::complete(missing,error),"missing original calls accepted");
}
void completion(){
    constexpr uintptr_t image=0x140000000ULL,owner=0x10000,build=0x20000,default_tool=0x30000,selection=0x40000,target=0x50000;
    for(unsigned scenario=0;scenario<8;++scenario){
        Memory memory;memory.put(image+0x13df9d0,owner);memory.put(owner,image+0x1786dd0);
        memory.put(owner+0x190,build);memory.put(owner+0x198,default_tool);memory.put(owner+0x148,selection);
        memory.put(build,image+0x17327d0);memory.put(default_tool,image+0x17391a0);
        if(scenario==1)memory.put(owner+0x190,uintptr_t{0x60000});
        if(scenario==2)memory.put(build,uintptr_t{1});
        unsigned switches=0,selections=0,readbacks=0;local::Receipt receipt;std::string error;
        auto read=[&](uintptr_t pointer,size_t offset,uintptr_t& value,std::string& cause){return memory.copy(pointer+offset,&value,8,cause);};
        auto call=[&](bool selected,uintptr_t cursor_owner,uintptr_t value,std::string& cause){
            need(cursor_owner==owner,"local original Cursor owner changed");
            if(selected){++selections;need(value==target,"local selected another Building");if(scenario==5){cause="original Select failed";return false;}}
            else{++switches;need(value==0,"local default argument differs");if(scenario==3){cause="original Switch failed";return false;}if(scenario!=4)memory.put(owner+0x190,default_tool);}
            return true;
        };
        auto selected=[&](uintptr_t manager,uintptr_t value,std::string& cause){
            ++readbacks;need(manager==selection&&value==target,"local selected readback owner differs");
            if(scenario==6){cause="selection membership differs";return false;}return true;
        };
        const bool okay=local::complete(image,build,target,receipt,read,call,selected,error);
        if(scenario==0||scenario==7){
            need(okay&&receipt.attempted&&receipt.switched&&receipt.selected&&switches==1&&selections==1&&readbacks==1,"local original completion incomplete");
            need(!local::complete(image,build,target,receipt,read,call,selected,error)&&switches==1&&selections==1,"local original completion replayed");
        }else if(scenario==1)need(okay&&receipt.superseded&&switches==0&&selections==0,"new local tool overwritten by old ACK");
        else{
            need(!okay&&!error.empty(),"failed local completion became success");
            need(switches<=1&&selections<=1,"failed local original replayed");
            if(scenario<5)need(selections==0,"selection ran after failed switch admission");
        }
    }
}
}
int main()try{
    cursor_fixture::guards();cursor_fixture::routes();cursor_fixture::completion();
    std::cout<<cursor_fixture::checks<<" Dungeon original Cursor guard and local completion checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
