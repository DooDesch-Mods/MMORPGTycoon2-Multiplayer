#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
using Json=nlohmann::json;
using DWORD=unsigned long;
struct alignas(8) String{const char* data=nullptr;size_t size=0;void* padding[2]{};};
bool current=true,source_readable=true,key_readable=true,construct_ok=true,panel_shown=false,menu_shown=true,open_ok=true,caption_drift=false;
unsigned original_builds=0,original_selections=0,show_calls=0,open_calls=0,disposals=0;
uint64_t command_count=0,command_failures=0;
std::string cause,selected_unknown;
void* panel=reinterpret_cast<void*>(2);void* ui_owner=reinterpret_cast<void*>(3);void* fixture_start=reinterpret_cast<void*>(4);
uintptr_t image=0x1000;
std::vector<std::string> received_keys;
std::vector<std::array<unsigned char,0x58>> received_items;
bool fail(const std::string& error){cause=error;++command_failures;return false;}
bool text(const void* p,std::string& out){if(!key_readable)return false;const auto& value=*static_cast<const String*>(p);out.assign(value.data,value.size);return true;}
bool readable(const void*,size_t){return source_readable;}
template<class T>bool read(const void* p,size_t offset,T& out){if(!source_readable||!p)return false;std::memcpy(&out,static_cast<const unsigned char*>(p)+offset,sizeof(out));return true;}
unsigned find_window(const char*,void** out){*out=fixture_start;return 0;}
bool menu_owner_current(void* p){return current&&p==fixture_start;}
int show(void* receiver,bool visible){++show_calls;if(receiver==fixture_start)menu_shown=visible;return 0;}
unsigned string_call(uintptr_t rva,void* receiver,const char*){if(rva!=0x4a2f90||receiver!=ui_owner)throw std::runtime_error("wrong Mode command path");++open_calls;if(open_ok){++command_count;panel_shown=true;}return 0;}
bool panel_visibility(void* receiver,bool& visible,std::string&){visible=receiver==fixture_start?menu_shown:panel_shown;return true;}
#include "native_start_menu_state.inc"
DWORD menu_item_construct(MenuElement* item,unsigned* built){if(!construct_ok)return 123;item->key={"mt2mp_multiplayer",17,{}};item->label.value={"Multiplayer...",14,{}};*built=3;return 0;}
DWORD menu_item_dispose(MenuElement*,unsigned){++disposals;return 0;}
std::vector<MenuElement> native_elements;
std::vector<std::array<unsigned char,0x358>> native_items;
std::vector<void*> native_item_pointers;
template<class T>void put(void* p,size_t offset,const T& value){std::memcpy(static_cast<unsigned char*>(p)+offset,&value,sizeof(value));}
#include "native_start_menu_readback.inc"
void record_elements(void* menu,const void* source,float){
    ++original_builds;received_keys.clear();received_items.clear();const auto& array=*static_cast<const MenuArray*>(source);
    for(int i=0;i<array.count;++i){const auto* item=static_cast<const MenuElement*>(array.data)+i;std::string key;
        const auto& native=item->key;key.assign(native.data,native.size);received_keys.push_back(key);
        std::array<unsigned char,0x58> bytes{};std::memcpy(bytes.data(),item,bytes.size());received_items.push_back(bytes);}
    native_elements.assign(static_cast<const MenuElement*>(array.data),static_cast<const MenuElement*>(array.data)+array.count);
    native_items.clear();native_items.resize(size_t(array.count));native_item_pointers.clear();
    for(size_t i=0;i<native_items.size();++i){
        auto* item=native_items[i].data();put(item,0,image+0x16dc680);put(item,0x1b0,native_elements[i].key);
        String caption=native_elements[i].label.value;if(caption_drift&&received_keys[i]=="mt2mp_multiplayer")caption={"Wrong",5,{}};
        put(item,0x2c8,caption);native_item_pointers.push_back(item);
    }
    void* element_data=native_elements.data();void* item_data=native_item_pointers.data();
    put(menu,0x388,element_data);put(menu,0x390,array.count);put(menu,0x370,item_data);put(menu,0x378,array.count);
}
void record_select(void*,const void* key){++original_selections;const auto& value=*static_cast<const String*>(key);selected_unknown.assign(value.data,value.size);}
#include "native_start_menu_callbacks.inc"
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void reset(){current=source_readable=key_readable=construct_ok=open_ok=menu_shown=true;panel_shown=caption_drift=false;original_builds=original_selections=show_calls=open_calls=disposals=0;command_count=command_failures=0;cause.clear();received_keys.clear();received_items.clear();start_menu_owner=start_menu_native=nullptr;start_menu_readback_confirmed=false;start_menu_builds=start_menu_launches=start_menu_failures=0;start_menu_error.clear();start_menu_first_error.clear();original_menu_elements=record_elements;original_menu_select=record_select;}
int main(){try{
    std::array<MenuElement,4> items{};
    const std::array<const char*,4> keys={"save","mt2fw_mods","options","quit"};
    for(size_t i=0;i<items.size();++i){items[i].key={keys[i],std::strlen(keys[i]),{}};items[i].label.value=items[i].key;items[i].label.args[0]=reinterpret_cast<void*>(i+100);}
    MenuArray source{nullptr,items.data(),4,4};std::array<unsigned char,0x400> menu{};put(menu.data(),0x280,fixture_start);
    reset();start_menu_elements(menu.data(),&source,200.f);
    check(received_keys==std::vector<std::string>{"save","mt2fw_mods","mt2mp_multiplayer","options","quit"},"insertion loses/reorders Mods or original entries");
    check(source.count==4&&source.data==items.data()&&original_builds==1&&disposals==1&&start_menu_owner==fixture_start,"original borrowed source changed, original replayed or temporary not disposed");
    Json actual;std::string readback_error;check(start_menu_readback_confirmed&&read_start_menu(actual,readback_error)&&actual.at("multiplayer_elements")==1&&actual.at("multiplayer_items")==1,"actual native copied array and built TextPane do not confirm one matching entry");
    for(size_t i=0;i<items.size();++i){const size_t position=i<2?i:i+1;check(!std::memcmp(received_items[position].data(),&items[i],sizeof(MenuElement)),"foreign menu item key/label payload changed");}
    String mods{"mt2fw_mods",10,{}};start_menu_select(fixture_start,&mods);
    check(original_selections==1&&selected_unknown=="mt2fw_mods"&&show_calls==0&&open_calls==0,"Mods selection was intercepted");
    String quit{"quit",4,{}};start_menu_select(fixture_start,&quit);check(original_selections==2&&selected_unknown=="quit","Quit selection was intercepted");
    String ours{"mt2mp_multiplayer",17,{}};start_menu_select(fixture_start,&ours);
    check(original_selections==2&&show_calls==1&&open_calls==1&&start_menu_launches==1&&panel_shown,"own selection does not close native menu and open one same panel");
    current=false;start_menu_select(fixture_start,&ours);check(show_calls==1&&open_calls==1&&start_menu_failures==1,"stale menu owner opens panel");
    reset();current=false;start_menu_elements(menu.data(),&source,200.f);check(received_keys==std::vector<std::string>{"save","mt2fw_mods","options","quit"}&&start_menu_builds==0,"unsupported mode menu mutated");
    reset();construct_ok=false;start_menu_elements(menu.data(),&source,200.f);check(received_keys.size()==4&&original_builds==1&&start_menu_failures==1&&start_menu_error.find("exception=123")!=std::string::npos,"construction failure drops original menu or loses cause");
    reset();key_readable=false;start_menu_elements(menu.data(),&source,200.f);check(received_keys.size()==4&&original_builds==1&&start_menu_error=="native start menu key unreadable","invalid key drops original menu or hides error");
    reset();std::array<MenuElement,2> duplicate{};duplicate[0].key=ours;duplicate[0].label.value={"Multiplayer...",14,{}};duplicate[1].key=quit;duplicate[1].label.value=quit;MenuArray duplicate_source{nullptr,duplicate.data(),2,2};start_menu_elements(menu.data(),&duplicate_source,200.f);
    check(received_keys==std::vector<std::string>{"mt2mp_multiplayer","quit"}&&original_builds==1&&disposals==0,"duplicate entry inserted twice");
    open_ok=false;start_menu_select(fixture_start,&ours);check(start_menu_launches==0&&open_calls==1&&start_menu_failures==1,"returned native call counted as confirmed without panel/readback");
    reset();caption_drift=true;start_menu_elements(menu.data(),&source,200.f);check(!start_menu_readback_confirmed&&start_menu_failures==1&&start_menu_error=="native built Multiplayer menu literal caption differs","forwarded source alone confirms an incorrectly built caption");
    start_menu_select(fixture_start,&ours);check(open_calls==0&&start_menu_launches==0,"unconfirmed native item can launch panel");
    std::cout<<"PASS actual StartMenu callback bodies: preserve all foreign entries, forward Mods/Quit, duplicate/owner/failure guards and one native panel open\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
