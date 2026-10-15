namespace display_native {
struct alignas(8) Loc {unsigned char bytes[0x38]{};};
struct alignas(8) Arg {unsigned char bytes[0x68]{};};
struct Borrowed {const char* data;uint64_t length;unsigned char reserved[16]{};};
static_assert(sizeof(Loc)==56&&sizeof(Arg)==104&&sizeof(Borrowed)==32);
bool code_result(DWORD code,const char* what,std::string& e){if(!code)return true;e=std::string("Staff display native ")+what+" exception="+std::to_string(code);unknown(e);return false;}
DWORD native_destroy(uintptr_t rva,void* out){__try{reinterpret_cast<void(*)(void*)>(image+rva)(out);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_string(void* out,const char* key){__try{reinterpret_cast<void*(*)(void*,const char*)>(image+0xa16f10)(out,key);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_number(void* out,int32_t number){__try{reinterpret_cast<void*(*)(void*,int32_t)>(image+0x882400)(out,number);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_name(void* out,uintptr_t project){__try{reinterpret_cast<void*(*)(void*,void*)>(image+0x2642a0)(out,reinterpret_cast<void*>(project));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_arg(void* out,const void* name,const void* value){__try{reinterpret_cast<void(*)(void*,const void*,const void*)>(image+0xc6c620)(out,name,value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_format(void* out,const char* key,const void* arg){__try{reinterpret_cast<void*(*)(void*,const char*,const void*)>(image+0xa94360)(out,key,arg);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_complete(uintptr_t lounge,bool& out){__try{out=reinterpret_cast<bool(*)(void*)>(image+0x25ed50)(reinterpret_cast<void*>(lounge));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_summary(uintptr_t grid,const void* value){__try{reinterpret_cast<void(*)(void*,int,int,const void*)>(image+0x701bb0)(reinterpret_cast<void*>(grid),1,1,value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_project(uintptr_t pane,const void* value){__try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7f80a0)(reinterpret_cast<void*>(pane),value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD native_developer(uintptr_t grid,int row,const void* value,const void* empty){__try{reinterpret_cast<void(*)(void*,int,int,const void*,bool,const void*,float)>(image+0x7070f0)(reinterpret_cast<void*>(grid),1,row,value,false,empty,0.0f);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct Value {Loc value;bool live=false;
    Value()=default;Value(const Value&)=delete;Value& operator=(const Value&)=delete;
    bool close(std::string& e){if(!live)return true;live=false;return code_result(native_destroy(0x11b2b00,value.bytes+0x20),"LocString vector destruction",e)&&code_result(native_destroy(0x11bd1e0,value.bytes),"LocString key destruction",e);}
    ~Value(){std::string e;if(!close(e))unknown(e);}
    bool literal(const char* key,std::string& e){live=code_result(native_string(value.bytes,key),"LocString construction",e);return live;}
    bool number(int32_t n,std::string& e){live=code_result(native_number(value.bytes,n),"number construction",e);return live;}
    bool name(uintptr_t p,std::string& e){live=code_result(native_name(value.bytes,p),"project name",e);return live;}
};
struct Argument {Arg value;bool live=false;
    ~Argument(){std::string e;if(!close(e))unknown(e);}
    bool close(std::string& e){if(!live)return true;live=false;return code_result(native_destroy(0xc6c9b0,value.bytes),"LocArg destruction",e);}
    bool make(const Value& name,std::string& e){const Borrowed key{"techname",8,{}};live=code_result(native_arg(value.bytes,&key,name.value.bytes),"LocArg construction",e);return live;}
};
bool activity_value(const staff_display::Developer& d,uintptr_t project,Value& out,std::string& e){
    const auto key=staff_display::activity_key(d);if(!key)return fail(e,"Staff display unsupported actual activity");
    if(d.job!=6&&d.job!=7)return out.literal(key,e);
    Value name;Argument arg;
    if(!name.name(project,e)||!arg.make(name,e))return false;
    out.live=code_result(native_format(out.value.bytes,key,arg.value.bytes),"project activity format",e);
    return out.live&&arg.close(e)&&name.close(e);
}
bool loc(uintptr_t value,json& out,std::string& e,unsigned depth=0){
    if(depth>2)return fail(e,"Staff native LocString nesting exceeds2");
    std::string key;uintptr_t first=0,last=0,end=0;
    if(!str(value,0,key,e)||!at(value,0x20,first,e)||!at(value,0x28,last,e)||!at(value,0x30,end,e))return false;
    if((!first&&(last||end))||last<first||end<last||(last-first)%0x68||(end-first)%0x68||
       (last-first)/0x68>4||(end-first)/0x68>8)return fail(e,"Staff native LocString argument bounds invalid");
    out={{"key",key},{"args",json::array()}};std::set<std::string> names;
    for(uintptr_t p=first;p<last;p+=0x68){std::string name;int32_t kind=0;
        if(!str(p,0,name,e)||!at(p,0x60,kind,e))return false;
        if(name.empty()||!names.insert(name).second)return fail(e,"Staff native LocString argument name absent/duplicated");
        json item={{"name",name},{"type",kind}};
        if(kind==0){json nested;if(!loc(p+0x20,nested,e,depth+1))return false;item["value"]=nested;}
        else if(kind==1){int32_t number=0;if(!at(p,0x58,number,e))return false;item["value"]=number;}
        else if(kind==2){float number=0;int32_t places=0;if(!at(p,0x5c,number,e)||!at(p,0x58,places,e))return false;
            if(!std::isfinite(number))return fail(e,"Staff native LocString float is not finite");item["value"]=number;item["places"]=places;}
        else return fail(e,"Staff native LocString argument type outside0..2");
        out["args"].push_back(std::move(item));
    }return true;
}

}
