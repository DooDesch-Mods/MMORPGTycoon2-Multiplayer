#include "dungeon_gizmo_record.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace records=mt2mp::game::dungeon_gizmo_record;
namespace action=mt2mp::action_session;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=source[i];}
    template<class T>bool read(uintptr_t address,size_t offset,T& value,std::string& error){auto* out=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i){const auto found=bytes.find(address+offset+i);if(found==bytes.end()){error="original Gizmo field read cause";return false;}out[i]=found->second;}return true;}
};
int main()try{size_t comparisons=0;const auto check=[&](bool value,const std::string& why){++comparisons;if(!value)throw std::runtime_error(why+" comparison="+std::to_string(comparisons));};
    constexpr uintptr_t image=0x10000000,fixture_window=0x20000000,actor=0x30000000,proxy=0x31000000,fixture_grid=0x40000000,native_record=0x50000000,tokens=0x51000000;
    constexpr uint64_t uid=0x100000012;Memory memory;std::string error;
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return memory.read(base,offset,value,cause);};
    auto find=[&](uint64_t full,uintptr_t& found,std::string&){found=full==uid?actor:0;return true;};
    const auto setup=[&]{memory={};memory.put(fixture_window,image+0x1719c80);memory.put(fixture_window+0x4e8,image+0x16f9ee0);memory.put(fixture_window+0x4f0,actor);memory.put(fixture_window+0x4f8,proxy);memory.put(fixture_window+0x568,fixture_grid);
        memory.put(actor,image+0x1749ab0);memory.put(actor+0x18,uid);memory.put(actor+0x20,image+0x1749de0);memory.put(actor+0x28,proxy);memory.put(proxy,actor+0x20);
        memory.put(fixture_grid,image+0x16dade0);memory.put(fixture_grid+0x280,fixture_window);memory.put(fixture_grid+0x3c8,int32_t(5));
        memory.put(native_record+0x38,tokens);memory.put(native_record+0x40,int32_t(1));memory.put(native_record+0x44,int32_t(2));memory.put(tokens,int32_t(4));memory.put(tokens+0x28,int32_t(5));};
    setup();records::Window actual;check(records::window(read,find,image,fixture_window,actual,error)&&actual.target==actor&&actual.uid==uid,error);check(records::grid(read,image,actual,error),error);
    for(int mode=0;mode<8;++mode){setup();if(mode==0)memory.put(fixture_window,image+0x1719c81);if(mode==1)memory.put(fixture_window+0x4e8,image+0x16f9ee1);if(mode==2)memory.put(fixture_window+0x4f8,uintptr_t(0));
        if(mode==3)memory.put(actor+0x28,proxy+8);if(mode==4)memory.put(proxy,image+0x1749de0);if(mode==5)memory.put(actor+0x18,uid+0x100000000);if(mode==6)memory.put(actor,image+0x1749ab1);if(mode==7)memory.bytes.erase(actor+0x18);
        error.clear();check(!records::window(read,find,image,fixture_window,actual,error)&&!error.empty(),"Foreign Gizmo native Window/full identity accepted mode="+std::to_string(mode));}
    setup();memory.put(fixture_window+0x4f0,uintptr_t(0));memory.put(fixture_window+0x4f8,uintptr_t(0));check(records::window(read,find,image,fixture_window,actual,error)&&actual.target==0,error);
    memory.put(fixture_window+0x4f8,proxy);check(!records::window(read,find,image,fixture_window,actual,error),"Empty Window with retained proxy accepted");
    for(int mode=0;mode<4;++mode){setup();check(records::window(read,find,image,fixture_window,actual,error),error);if(mode==0)actual.grid=0;if(mode==1)memory.put(fixture_grid,image+0x16dade1);if(mode==2)memory.put(fixture_grid+0x280,fixture_window+8);if(mode==3)memory.put(fixture_grid+0x3c8,int32_t(4));
        error.clear();check(!records::grid(read,image,actual,error)&&!error.empty(),"Foreign Grid accepted");}
    for(int32_t value:{0,1,2,3,4,5}){setup();memory.put(tokens+0x28,value);int32_t out=-1;check(records::value(read,native_record,out,error)&&out==value,error);}
    for(int mode=0;mode<7;++mode){setup();if(mode==0)memory.put(native_record+0x38,uintptr_t(0));if(mode==1)memory.put(native_record+0x40,int32_t(2));if(mode==2)memory.put(native_record+0x44,int32_t(0));
        if(mode==3)memory.put(tokens,int32_t(3));if(mode==4)memory.put(tokens+0x28,int32_t(-1));if(mode==5)memory.put(tokens+0x28,int32_t(6));if(mode==6)memory.bytes.erase(tokens+0x28);
        int32_t out=0;error.clear();check(!records::value(read,native_record,out,error)&&!error.empty(),"Malformed native integer token admitted");}
    action::DungeonGizmoRecord contents;contents.gold=0;contents.loot=1;contents.vendor_trash=2;contents.keys=3;contents.boss_keys=4;
    for(unsigned edit=0;edit<5;++edit){const auto kind=action::DungeonGizmoEdit(edit);const int32_t value=*action::dungeon_gizmo_value(contents,kind);uint32_t calls=0;bool execute=true;
        check(records::replay(fixture_window,kind,value,fixture_window,action::DungeonGizmoEdit::Gold,5,&contents,calls,execute,error)&&!execute&&calls==1,"Original Window setup escaped before effect");
        check(!records::replay(fixture_window,kind,value+1,fixture_window,kind,5,&contents,calls,execute,error),"Changed setup field accepted");
        calls=0;check(records::replay(fixture_window,kind,5,fixture_window,kind,5,nullptr,calls,execute,error)&&execute&&calls==1,"Exact original-once command rejected");
        check(!records::replay(fixture_window,kind,5,fixture_window,kind,5,nullptr,calls,execute,error)&&!execute,"Repeated original command executed");
        calls=0;check(!records::replay(fixture_window+8,kind,5,fixture_window,kind,5,nullptr,calls,execute,error)&&calls==0,"Foreign original Window accepted");
        check(!records::replay(fixture_window,kind,4,fixture_window,kind,5,nullptr,calls,execute,error)&&calls==0,"Foreign original argument accepted");
    }
    for(bool door:{false,true})for(int32_t status_value:{0,1,2})for(bool triggered:{false,true}){
        action::DungeonGizmoRecord observed;observed.state=status_value;observed.triggered=triggered;
        const auto value=door?status_value+1:triggered?1:3;uint32_t calls=0;bool execute=true;
        check(records::replay(fixture_window,action::DungeonGizmoEdit::State,value,fixture_window,action::DungeonGizmoEdit::State,1,&observed,calls,execute,error,door)&&!execute,
            "Fresh equal original Door/trigger UI preparation must not replay gameplay");
        check(!records::replay(fixture_window,action::DungeonGizmoEdit::State,value==1?3:1,fixture_window,action::DungeonGizmoEdit::State,1,&observed,calls,execute,error,door),
            "Changed State UI preparation accepted");
    }
    for(bool returned:{false,true}){bool attempted=false;unsigned calls=0;auto restore=[&](std::string& cause){++calls;if(!returned){cause="original native Window restore failure";return false;}return true;};
        check(records::restore_once(attempted,restore,error)==returned&&attempted&&calls==1,"Original local Window restore not attempted once");
        if(!returned)check(error=="original native Window restore failure","Original restoration failure lost");
        check(!records::restore_once(attempted,restore,error)&&calls==1&&error.find("not replayed")!=std::string::npos,"Failed or confirmed local Window restore replayed");}
    std::cout<<comparisons<<" Gizmo native weak/owner/Grid/integer/preparation/original-once comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
