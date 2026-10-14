#include "ui_contract.h"
#include <limits>
namespace mt2mp::scenery_kiss {
namespace {
template<class T>bool at(native::Memory& m,uintptr_t p,size_t off,T& out,std::string& e){if(!p||p>(std::numeric_limits<uintptr_t>::max)()-off){e="scenery input address range invalid";return false;}if(!m.copy(p+off,&out,sizeof out,e)){if(e.empty())e="scenery input read failed without cause";return false;}return true;}
}
bool live_cursor(native::Memory& m,uintptr_t image,uintptr_t receiver,bool& own,std::string& e){
    own=false;uintptr_t cursor=0,current=0;
    if(!receiver){e="scenery input receiver missing";return false;}
    if(!at(m,image,0x13df9d0,cursor,e)||!at(m,cursor,0x190,current,e))return false;
    own=current==receiver;return true;
}
bool single_press(native::Memory& m,uintptr_t image,uintptr_t receiver,std::string& e){
    struct View {uintptr_t ap=0,options=0,options_ap=0,data=0;size_t size=0;int32_t mode=-1;std::array<char,6> text{};bool operator==(const View&)const=default;};
    auto read=[&](View& v){if(!at(m,receiver,0,v.ap,e)||!at(m,receiver,0x174,v.mode,e)||!at(m,receiver,0xb68,v.options,e)||!at(m,v.options,0,v.options_ap,e)||!at(m,v.options,0x88,v.data,e)||!at(m,v.options,0x90,v.size,e))return false;
        if(v.ap!=image+0x17396b0||v.mode!=0||v.options_ap!=image+0x1715390||v.size!=6||!v.data){e="scenery ordered single input required";return false;}
        if(!m.copy(v.data,v.text.data(),v.text.size(),e))return false;
        if(v.text!=std::array<char,6>{'s','i','n','g','l','e'}){e="scenery ordered single input required";return false;}return true;};
    View a,b;if(!read(a)||!read(b))return false;if(a!=b){e="scenery input changed during guard";return false;}return true;
}
bool idle_single_update(native::Memory& m,uintptr_t image,uintptr_t receiver,bool& allowed,std::string& e){
    allowed=false;
    struct View {uintptr_t ap=0,options=0,options_ap=0,data=0,graph=0,edit=0;size_t size=0;int32_t mode=-1;std::array<uint8_t,3> flags{};uint8_t painting=0;std::array<char,6> name{};bool operator==(const View&)const=default;};
    auto read=[&](View& v){
        if(!at(m,receiver,0,v.ap,e)||!at(m,receiver,0x174,v.mode,e)||!at(m,receiver,0xb68,v.options,e)||!at(m,v.options,0,v.options_ap,e)||
           !at(m,v.options,0x88,v.data,e)||!at(m,v.options,0x90,v.size,e)||!at(m,receiver,0x6c0,v.flags,e)||!at(m,receiver,0xb48,v.painting,e)||
           !at(m,receiver,0xc50,v.graph,e)||!at(m,receiver,0x360,v.edit,e))return false;
        if(v.ap!=image+0x17396b0||v.options_ap!=image+0x1715390||v.mode<0||v.mode>2||v.painting>1){e="scenery idle mode transition native owner/domain differs";return false;}
        if(v.size==6&&v.data&&!m.copy(v.data,v.name.data(),v.name.size(),e))return false;return true;
    };
    View a,b;if(!read(a)||!read(b))return false;if(a!=b){e="scenery idle mode transition changed during read";return false;}
    allowed=a.mode!=0&&a.size==6&&a.name==std::array<char,6>{'s','i','n','g','l','e'}&&a.flags==std::array<uint8_t,3>{}&&!a.painting&&!a.graph&&!a.edit;
    return true;
}

bool valid_effect_counts(uint32_t pa,uint32_t pr,uint32_t aa,uint32_t ar,uint32_t ua,uint32_t ur,std::string& e){
    if(pa!=1||pr!=1||aa!=1||ar!=1||ua!=1||ur!=1){e="scenery actual Purchase/Add/Undo attempts or returns differ";return false;}return true;
}
bool purchase_input(native::Memory& memory,int32_t actual,int32_t expected,uintptr_t position,const std::array<float,3>& wanted,std::string& e){
    if(actual!=expected){e="scenery native Purchase category differs";return false;}
    std::array<float,3> observed{};if(!at(memory,position,0,observed,e))return false;
    if(observed!=wanted){e="scenery native Purchase position differs";return false;}return true;
}
}
