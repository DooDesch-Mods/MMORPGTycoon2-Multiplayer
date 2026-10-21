#include <cstdint>
#include <string>
#include <iostream>
namespace character_import_tail_fixture {
struct State {uintptr_t image=0x100000;uint32_t thread=9;} state;
struct ImportTail {bool armed=false,preserve=false;uintptr_t view=0,target=0;uint32_t calls=0;} import_tail;
bool fault=false;uint32_t actual_thread=9,originals=0;
uint32_t GetCurrentThreadId(){return actual_thread;}
bool fail(std::string& error,const char* cause){error=cause;return false;}
namespace ability_edit {void original_view_select(void*,void*){++originals;}}
#include "character_import_tail_body.inc"
}
int main()try{using namespace character_import_tail_fixture;unsigned checks=0;std::string error;bool handled=false;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);};
    check(import_presentation(nullptr,nullptr,0,handled,error)&&!handled&&!originals,"Unrelated local selection intercepted");
    for(bool preserve:{false,true}){import_tail={true,preserve,0x200,0x300,0};fault=false;originals=0;
        check(import_presentation(reinterpret_cast<void*>(0x200),reinterpret_cast<void*>(0x300),state.image+0x68997a,handled,error)&&handled&&import_tail.calls==1&&originals==unsigned(!preserve),"Exact original presentation ownership changed");
        check(!import_presentation(reinterpret_cast<void*>(0x200),reinterpret_cast<void*>(0x300),state.image+0x68997a,handled,error)&&handled&&fault&&originals==unsigned(!preserve),"Scoped tail replay entered original twice");}
    for(unsigned scenario=0;scenario<4;++scenario){import_tail={true,false,0x200,0x300,0};fault=false;originals=0;actual_thread=scenario==0?10:9;
        const auto view=scenario==1?0x201u:0x200u;const auto selected=scenario==2?0x301u:0x300u;const auto caller=state.image+(scenario==3?0x68997bu:0x68997au);
        check(!import_presentation(reinterpret_cast<void*>(uintptr_t(view)),reinterpret_cast<void*>(uintptr_t(selected)),caller,handled,error)&&handled&&fault&&!originals,"Foreign receiver/definition/caller/thread reached original presentation");}
    std::cout<<checks<<" actual Character import original-tail ownership/no-replay checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
