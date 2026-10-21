namespace gizmo_name_bound_fixture {
namespace b=mt2mp::game::dungeon_gizmo_name_bound;
struct Copy {
    b::Text header{0x90000,1,{}};std::string value=",";unsigned headers=0,strings=0;int fault=0;
    bool operator()(uintptr_t address,void* out,size_t bytes,std::string& error){
        if(address==0x140000000+0x1c4eaa0&&bytes==sizeof header){++headers;auto current=header;
            if(fault==1&&headers==2)++current.data;std::memcpy(out,&current,bytes);return true;}
        if(address==header.data&&bytes==value.size()){++strings;auto current=value;
            if(fault==2&&strings==2)current[0]='.';std::memcpy(out,current.data(),bytes);return true;}
        error="fixture exact native separator read absent";return false;
    }
};
template<class Check>void run(Check& check){std::string error,separator;
    Copy read;check(b::separator(read,0x140000000,separator,error)&&separator==","&&read.headers==2&&read.strings==2,"actual separator reads all owner and value bytes twice");
    check(b::widest("container",separator)=="container #-2,147,483,648","native fixed format and int32 signed bound differ");
    for(const auto& value:std::vector<std::string>{"",",","\t","\"","\xc2\xa0"}){
        const auto maximum=nlohmann::json(b::widest("container",value)).dump().size();
        for(const auto& number:std::vector<std::string>{"0","9","999","1"+value+"000","2"+value+"147"+value+"483"+value+"647","-2"+value+"147"+value+"483"+value+"648"})
            check(nlohmann::json("container #"+number).dump().size()<=maximum,"JSON escaped native output exceeds its exact fixed/type/separator bound");
    }
    read={};read.fault=1;check(!b::separator(read,0x140000000,separator,error)&&error=="Gizmo native integer separator owner changed","separator owner replacement was admitted");
    read={};read.fault=2;check(!b::separator(read,0x140000000,separator,error)&&error=="Gizmo native integer separator bytes changed or contain NUL","separator value replacement was admitted");
    read={};read.header.size=8193;check(!b::separator(read,0x140000000,separator,error),"unbounded native separator allocation was admitted");
}
}
