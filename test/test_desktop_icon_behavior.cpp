#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
using DWORD=unsigned long;
unsigned thread=31,current_thread=31;
unsigned GetCurrentThreadId(){return current_thread;}
void* ui_owner=reinterpret_cast<void*>(0xA1);
void* desktop_attempt_owner=reinterpret_cast<void*>(0xD1);
bool desktop_append_attempted=true;
bool shown=true,lookup_ok=true;
void* actual_entry=reinterpret_cast<void*>(0xE1);
unsigned forwarded=0,destroyed=0,deleted=0,commands=0,failures=0,logged=0;
DWORD command_error=0,delete_error=0;
template<class T> bool read(void* record,size_t offset,T& value){assert(record==reinterpret_cast<void*>(0xB1)&&offset==0x3c);value[0]=123.f;value[1]=234.f;return true;}
void forward(void*){++forwarded;}
void destroy(void*){++destroyed;}
void fail(const std::string& value){assert(!value.empty());++failures;}
bool current_desktop_entry(void** target,bool* visible,std::string& error){*target=actual_entry;*visible=shown;if(!lookup_ok)error="retained lookup failure";return lookup_ok;}
DWORD string_call(uintptr_t rva,void* receiver,const char* value){assert(rva==0x4b9be0&&receiver==ui_owner&&std::string(value)=="MT2MPOpen");++commands;return command_error;}
DWORD delete_icon_record(void* record){assert(record==reinterpret_cast<void*>(0xB1));assert(destroyed>0);++deleted;return delete_error;}
#define LOG_ERROR(...) (++logged)
#include "desktop_icon_behavior_body.inc"
void bind(){desktop_icon_bindings[reinterpret_cast<void*>(0xE1)]={reinterpret_cast<void*>(0xD1),reinterpret_cast<void*>(0xB1)};}
int main(){
    original_icon_launch=&forward;original_icon_destroy=&destroy;
    desktop_icon_launch(reinterpret_cast<void*>(0xF1));assert(forwarded==1&&commands==0&&failures==0);
    bind();desktop_icon_launch(actual_entry);assert(commands==1&&forwarded==1&&failures==0);
    shown=false;desktop_icon_launch(actual_entry);assert(commands==1&&failures==1);shown=true;
    current_thread=32;desktop_icon_launch(actual_entry);assert(commands==1&&failures==2);current_thread=31;
    actual_entry=reinterpret_cast<void*>(0xE2);desktop_icon_launch(reinterpret_cast<void*>(0xE1));assert(commands==1&&failures==3);actual_entry=reinterpret_cast<void*>(0xE1);
    lookup_ok=false;desktop_icon_launch(actual_entry);assert(commands==1&&failures==4);lookup_ok=true;
    command_error=5;desktop_icon_launch(actual_entry);assert(commands==2&&failures==5);command_error=0;
    desktop_icon_destroy(reinterpret_cast<void*>(0xF1));assert(destroyed==1&&deleted==0&&desktop_append_attempted);
    desktop_icon_destroy(actual_entry);assert(destroyed==2&&deleted==1&&!desktop_append_attempted&&desktop_icon_bindings.empty());
    assert(desktop_position_valid&&desktop_position_owner==reinterpret_cast<void*>(0xD1)&&desktop_position[0]==123.f&&desktop_position[1]==234.f);
    desktop_icon_launch(actual_entry);assert(forwarded==2&&commands==2);
    bind();desktop_append_attempted=true;delete_error=9;desktop_icon_destroy(actual_entry);assert(destroyed==3&&deleted==2&&logged==1&&desktop_icon_bindings.empty());
    std::cout<<"PASS: exact staged callbacks preserve other apps, require current visible owned Desktop/thread, route to original Mode, retire native record after icon, and expose failures\n";
}
