#include "character_membership_reader.h"
#include <cstring>
#include <iostream>
namespace member=mt2mp::game::character_membership_reader;
namespace definitions=mt2mp::game::character_definition_identity;
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t p,const T& value){const auto* source=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=source[i];}
    template<class T>bool read(uintptr_t p,size_t offset,T& out,std::string& error){auto* target=reinterpret_cast<uint8_t*>(&out);
        for(size_t i=0;i<sizeof out;++i){const auto found=bytes.find(p+offset+i);if(found==bytes.end()){error="original Character current user field failure";return false;}target[i]=found->second;}return true;}
};
int main()try{
    constexpr uintptr_t image=0x140000000,base_type=0x20000000,variant=0x21000000,collection=0x22000000,record=0x23000000;
    Memory memory;std::map<uint64_t,uintptr_t> registered;std::string error;unsigned checks=0;
    const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    auto at=[&](uintptr_t p,size_t offset,auto& out,std::string& cause){return memory.read(p,offset,out,cause);};
    auto find=[&](uint64_t uid,uintptr_t& out,std::string&){const auto found=registered.find(uid);out=found==registered.end()?0:found->second;return true;};
    const auto array=[&](uintptr_t p,uintptr_t data,const std::vector<uintptr_t>& members){memory.put(p+8,data);memory.put(p+0x10,int32_t(members.size()));memory.put(p+0x14,int32_t(members.size()));
        for(size_t i=0;i<members.size();++i)memory.put(data+i*8,members[i]);};
    definitions::Census census;
    const auto setup=[&]{memory={};registered.clear();census={};
        definitions::Definition first;first.object=base_type;first.kind=1;first.id=0;first.key="character:1:0";first.owning_collection=collection;census.definitions.push_back(first);
        auto child=first;child.object=variant;child.id=1;child.key="character:1:1";child.parent=base_type;census.definitions.push_back(child);
        std::vector<uintptr_t> actors;
        for(size_t family=0;family<4;++family){const auto pointer=uintptr_t(0x30000000+family*0x10000),ap=uintptr_t(0x40000000+family*0x10000);const auto uid=(uint64_t(family+1)<<32)|1;
            actors.push_back(pointer);memory.put(pointer,ap);memory.put(pointer+0x18,uid);memory.put(ap+0x438,image+member::callbacks[family]);registered[uid]=pointer;}
        for(const auto object:{base_type,variant}){memory.put(object+0x4c0,image+0x1747ae0);array(object+0x4c0,object+0x1000,actors);}
        memory.put(collection,image+0x173a6f0);memory.put(collection+0x70,image+0x16eef80);array(collection+0x70,collection+0x1000,{record});
        memory.put(record,image+0x1740fa0);memory.put(record+0x18,int32_t(1));memory.put(record+0x1c,int32_t(0));};
    member::Users users;setup();check(member::users(at,find,image,census,users,error),error);
    check(users.rows.size()==8&&users.by_definition.size()==2,"Complete four-family per-definition membership was collapsed");
    for(size_t family=0;family<4;++family)check(users.rows[family].family==member::Family(family),"Actual original destruction callback mapped to wrong Actor family");
    for(int mode=0;mode<10;++mode){setup();
        if(mode==0)memory.put(base_type+0x4c0,image+0x1747ae1);if(mode==1)memory.put(base_type+0x4d0,int32_t(-1));if(mode==2)memory.put(base_type+0x4d4,int32_t(1));
        if(mode==3)memory.put(base_type+0x1008,uintptr_t(0x30000000));if(mode==4)memory.put(uintptr_t(0x30010018),uint64_t(0x100000001));
        if(mode==5)registered[0x100000001]=0x30010000;if(mode==6)memory.put(uintptr_t(0x40000438),image+0x3976f1);
        if(mode==7)memory.bytes.erase(0x30000018);if(mode==8)memory.put(uintptr_t(0x30000018),uint64_t(0));if(mode==9)memory.put(uintptr_t(0x30000018),uint64_t(0x900000001));
        error.clear();check(!member::users(at,find,image,census,users,error)&&!error.empty()&&users.rows.empty(),"Invalid complete current user witness accepted mode="+std::to_string(mode));
        if(mode==7)check(error=="original Character current user field failure","Original current user field failure was replaced");
    }
    setup();census.definitions[1].deleted=true;
    for(auto it=memory.bytes.begin();it!=memory.bytes.end();){if(it->first>=variant+0x4c0&&it->first<variant+0x4d8)it=memory.bytes.erase(it);else ++it;}
    check(member::users(at,find,image,census,users,error)&&users.rows.size()==4,"Destroyed definition's opaque former user array was dereferenced");
    setup();uintptr_t member_record=0;
    check(member::collection_member(at,image,census.definitions[0],member_record,error)&&member_record==record,error);
    for(int mode=0;mode<6;++mode){setup();auto selected=census.definitions[0];
        if(mode==0)selected.kind=-1;if(mode==1)selected.deleted=true;if(mode==2)memory.put(record,image+0x1740fa1);
        if(mode==3)memory.put(record+0x1c,int32_t(2));if(mode==4)array(collection+0x70,collection+0x1000,{record,record});if(mode==5)memory.bytes.erase(record+0x18);
        error.clear();check(!member::collection_member(at,image,selected,member_record,error)&&!error.empty(),"Invalid current owning Collection admitted mode="+std::to_string(mode));}
    setup();check(member::affected(census,census.definitions[0])==std::vector<std::string>{"character:1:0","character:1:1"},"Base destruction omitted its current live variant");
    check(member::affected(census,census.definitions[1])==std::vector<std::string>{"character:1:1"},"Variant destruction selected its base");
    std::cout<<checks<<" complete Character current native user/full UID/callback/Collection comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
