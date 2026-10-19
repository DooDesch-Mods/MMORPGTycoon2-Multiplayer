#include "character_boss_retirement_read.h"
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace cb=mt2mp::game::character_boss_retirement_read;
namespace {
int cases=0;
void need(bool result,const std::string& cause){++cases;if(!result)throw std::runtime_error(cause);}
constexpr uintptr_t image=0x140000000,root=0x10000,region=0x20000,manager=0x30000,fixture_data=0x40000;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    std::map<uint64_t,uintptr_t> objects;
    template<class T>void put(uintptr_t address,const T& value){
        const auto* source=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i)bytes[address+i]=source[i];
    }
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& error){
        auto* destination=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto it=bytes.find(object+offset+i);
            if(it==bytes.end()){error="fixture actual field unavailable";return false;}destination[i]=it->second;}
        return true;
    }
    bool find(uint64_t uid,uintptr_t& found,std::string&){
        const auto it=objects.find(uid);found=it==objects.end()?0:it->second;return true;
    }
    Memory(){
        put(region+0x70,root);put(region+0x990,manager);put(manager,image+0x16fa780);
        put(manager+0x68,region);put(manager+0x38,image+0x172dfa0);put(manager+0x40,fixture_data);
        put(manager+0x48,int32_t(3));put(manager+0x4c,int32_t(4));put(image+0x1743040+0x438,image+0x3976f0);
        for(uint64_t i=0;i<3;++i){const auto object=uintptr_t(0x50000+i*0x1000);const uint64_t uid=200+i;
            put(fixture_data+size_t(i)*8,object);put(object,image+0x1743040);put(object+0x18,uid);
            put(object+0x170,root);put(object+0x178,region);objects[uid]=object;}
    }
    bool capture(cb::Pool& out,std::string& error){
        auto read_field=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return read(object,offset,value,cause);};
        auto lookup=[&](uint64_t uid,uintptr_t& found,std::string& cause){return find(uid,found,cause);};
        return cb::census(image,root,region,read_field,lookup,out,error);
    }
};
}
int main(){try{
    Memory memory;cb::Pool before;std::string error;
    need(memory.capture(before,error),error);need(before.members.size()==3,"complete original Boss census missing");
    need(cb::removed(before,before,{},error),"zero selected Boss users must preserve complete roster");
    auto after=before;const auto middle=before.members[1];after.members[1]=after.members.back();after.members.pop_back();
    need(cb::removed(before,after,{middle},error),"original swap-last removal must retain exact survivors");
    auto multiple=before;multiple.members={before.members[1]};
    need(cb::removed(before,multiple,{before.members[2],before.members[0]},error),"multiple native notifications must remove only selected users");
    auto corrupt=after;corrupt.members[0].uid=999;
    need(!cb::removed(before,corrupt,{middle},error),"replacement of a surviving full UID accepted");
    corrupt=after;corrupt.members[0].pointer+=8;
    need(!cb::removed(before,corrupt,{middle},error),"surviving UID reassignment accepted");
    corrupt=after;corrupt.members.push_back(before.members[0]);
    need(!cb::removed(before,corrupt,{middle},error),"extra or duplicate survivor accepted");
    corrupt=after;corrupt.members.pop_back();
    need(!cb::removed(before,corrupt,{middle},error),"unrelated original destruction accepted");
    corrupt=after;corrupt.manager+=8;
    need(!cb::removed(before,corrupt,{middle},error),"changed Boss manager accepted");
    need(!cb::removed(before,after,{middle,middle},error),"duplicate selected destruction accepted");
    need(!cb::removed(before,after,{{0x77777,999}},error),"absent selected destruction accepted");
    const std::array<std::pair<uintptr_t,uintptr_t>,8> invalid{{
        {region+0x70,root+8},{manager,image+0x1714100},{manager+0x68,region+8},
        {manager+0x38,image+0x172dfa8},{0x50000,image+0x16c0770},
        {0x50000+0x170,root+8},{0x50000+0x178,region+8},{image+0x1743040+0x438,image+0x3b9e20}}};
    for(const auto& [address,value]:invalid){Memory changed;changed.put(address,value);cb::Pool result;
        need(!changed.capture(result,error),"wrong native Boss ownership or notification accepted address="+std::to_string(address));}
    for(const int32_t count:{-1,100001}){Memory changed;changed.put(manager+0x48,count);cb::Pool result;
        need(!changed.capture(result,error),"invalid complete native manager count accepted");}
    {Memory changed;changed.put(manager+0x4c,int32_t(2));cb::Pool result;
        need(!changed.capture(result,error),"native array capacity below count accepted");}
    {Memory changed;changed.put(fixture_data+8,uintptr_t(0x50000));cb::Pool result;
        need(!changed.capture(result,error),"duplicate native Boss pointer/full UID accepted");}
    {Memory changed;changed.objects[200]=0x90000;cb::Pool result;
        need(!changed.capture(result,error),"FindUID mismatch accepted");}
    {Memory changed;changed.put(0x50000+0x18,uint64_t(0));cb::Pool result;
        need(!changed.capture(result,error),"zero native full UID accepted");}
    {Memory changed;changed.put(manager+0x40,uintptr_t(UINTPTR_MAX-8));cb::Pool result;
        need(!changed.capture(result,error),"overflowing native roster span accepted");}
    {Memory changed;changed.put(manager+0x48,int32_t(0));cb::Pool result;
        need(changed.capture(result,error)&&result.members.empty(),"empty native BossManager rejected");}
    {Memory changed;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){
            if(object==manager&&offset==0x4c)changed.put(manager+0x40,fixture_data+8);
            return changed.read(object,offset,value,cause);};
        auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){return changed.find(uid,found,cause);};cb::Pool result;
        need(!cb::census(image,root,region,read,find,result,error),"changed complete native array during capture accepted");}
    std::cout<<cases<<" actual Boss census and original destruction consequence cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
