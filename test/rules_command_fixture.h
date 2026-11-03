#pragma once
#include <Windows.h>
#include <string>
#include <vector>
namespace native_command_fixture {
struct View {const char* data=nullptr;size_t length=0;unsigned char rest[16]{};};
struct Record {std::string label;std::vector<std::string> arguments;View view;};
inline unsigned label_calls=0;
inline const View* label(const void* value){++label_calls;return &static_cast<const Record*>(value)->view;}
inline uintptr_t image(){return reinterpret_cast<uintptr_t>(&label)-0x96d880;}
inline Record* record(const char* label_text,std::initializer_list<const char*> arguments={}){
    static Record value;value.label=label_text;value.arguments.assign(arguments.begin(),arguments.end());
    value.view={value.label.data(),value.label.size(),{}};return &value;
}
}
