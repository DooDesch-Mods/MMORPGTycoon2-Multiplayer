#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <iostream>
using DWORD=unsigned long;
std::array<unsigned char,0x400> fixture_icon{},fixture_title_pane{};
std::array<unsigned char,0x200> fixture_picture{};
int fixture_desktop=0,calls=0,fail_call=0;
std::map<void*,std::string> strings;
template<class T> bool read(const void* p,size_t offset,T& out){
    if(p!=fixture_icon.data()||offset+sizeof(T)>fixture_icon.size())return false;
    std::memcpy(&out,fixture_icon.data()+offset,sizeof(T));return true;
}
bool native_icon_identity(void* p,void* owner,std::string& error){
    if(p==fixture_icon.data()&&owner==&fixture_desktop)return true;
    error="native fixture_icon identity mismatch";return false;
}
DWORD string_call(uintptr_t rva,void* receiver,const char* value){
    if(++calls==fail_call)return 123;
    assert(rva==0x11bfce0||rva==0x771f90);
    strings[rva==0x771f90?static_cast<unsigned char*>(receiver)+0x178:receiver]=value;
    return 0;
}
DWORD set_icon_text_key(void* pane,const void* key){
    if(++calls==fail_call)return 123;
    assert(pane==fixture_title_pane.data()&&key==fixture_icon.data()+0x380);
    strings[fixture_title_pane.data()+0x310]=strings[const_cast<void*>(key)];
    strings[fixture_title_pane.data()+0x2d0]=strings[const_cast<void*>(key)];return 0;
}
#include "desktop_icon_presentation_body.inc"
void initialize(){
    calls=0;fail_call=0;fixture_icon.fill(0);strings.clear();
    void* p=fixture_picture.data();std::memcpy(fixture_icon.data()+0x3c0,&p,sizeof(p));
    p=fixture_title_pane.data();std::memcpy(fixture_icon.data()+0x3c8,&p,sizeof(p));
    strings[fixture_icon.data()+0x360]="Multiplayer";
    strings[fixture_icon.data()+0x380]="{desktop_icon_discord_displayname}";
    strings[fixture_title_pane.data()+0x310]="{desktop_icon_discord_displayname}";
    strings[fixture_title_pane.data()+0x2d0]="Multiplayer";
    strings[fixture_picture.data()+0x178]="mt2mp_desktop_multiplayer";
}
void refresh_from_native_key(){
    const auto& key=strings[fixture_title_pane.data()+0x310];
    strings[fixture_title_pane.data()+0x2d0]=key=="{desktop_icon_discord_displayname}"?"Join the Discord! [weblink]":key;
}
int main(){
    std::string error;
    initialize();refresh_from_native_key();
    assert(strings[fixture_title_pane.data()+0x2d0]=="Join the Discord! [weblink]");
    assert(bind_desktop_icon_presentation(fixture_icon.data(),&fixture_desktop,error));
    assert(calls==4);
    assert(strings[fixture_icon.data()+0x360]=="Multiplayer");
    assert(strings[fixture_icon.data()+0x380]=="Multiplayer");
    assert(strings[fixture_title_pane.data()+0x310]=="Multiplayer");
    refresh_from_native_key();assert(strings[fixture_title_pane.data()+0x2d0]=="Multiplayer");
    assert(strings[fixture_picture.data()+0x178]=="mt2mp_desktop_multiplayer");
    initialize();assert(!bind_desktop_icon_presentation(fixture_title_pane.data(),&fixture_desktop,error));assert(calls==0);
    initialize();void* argument=fixture_picture.data();std::memcpy(fixture_icon.data()+0x3a8,&argument,sizeof(argument));
    assert(!bind_desktop_icon_presentation(fixture_icon.data(),&fixture_desktop,error));assert(calls==0);
    initialize();void* missing=nullptr;std::memcpy(fixture_icon.data()+0x3c8,&missing,sizeof(missing));
    assert(!bind_desktop_icon_presentation(fixture_icon.data(),&fixture_desktop,error));assert(calls==0);
    for(int step=1;step<=4;++step){
        initialize();fail_call=step;
        assert(!bind_desktop_icon_presentation(fixture_icon.data(),&fixture_desktop,error));
        assert(calls==step&&error.find("exception=123")!=std::string::npos);
    }
    std::cout<<"PASS: actual presentation body; stale-key regression, own identity, empty args, missing link, four native failures\n";
}
