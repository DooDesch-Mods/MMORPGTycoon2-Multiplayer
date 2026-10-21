#include "dungeon_zone_roster.h"
#include "world_zone_serialization.h"
#include "zone_edit_identity.h"
#include "zone_edit_read.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
namespace zones=mt2mp::game::dungeon_zone_roster;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t changing=0;unsigned hits=0;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    bool read(uintptr_t address,void* value,size_t size,std::string& error){auto* target=reinterpret_cast<unsigned char*>(value);
        for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){error="original Zone serialization field read failed";return false;}target[i]=found->second;}return true;}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& error){auto* target=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i){auto item=bytes.find(object+offset+i);if(item==bytes.end()){error="original Zone census field read failed";return false;}target[i]=item->second;}if(changing==object+offset&&++hits==2)target[0]^=8;return true;}
};
#include "zone_edit_identity_cases.inc"
int main()try{
    constexpr uintptr_t image=0x10000000,manager=0x20000000,shard=0x21000000,region=0x22000000,data=0x23000000,actors=0x30000000;
    Memory memory;std::map<uint64_t,uintptr_t> registry;std::string error;size_t checks=0;
    const auto need=[&](bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause+" comparison="+std::to_string(checks));};
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.read(object,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){auto item=registry.find(uid);if(item==registry.end()){cause="original Zone full UID lookup failed";return false;}object=item->second;return true;};
    const auto setup=[&]{memory={};registry.clear();memory.put(manager,image+0x16fe170);memory.put(manager+0xf0,region);memory.put(region+0x968,manager);memory.put(region+0x70,shard);
        for(size_t type=0;type<4;++type){const auto array=manager+0x18+type*0x30,slots=data+type*0x100,actor=actors+type*0x1000;const uint64_t uid=0x100000007+type*0x100000000;
            memory.put(array,image+0x172e1e0);memory.put(array+8,slots);memory.put(array+0x10,int32_t(1));memory.put(array+0x14,int32_t(1));memory.put(slots,actor);
            memory.put(actor,image+0x1745ce0);memory.put(actor+0x18,uid);memory.put(actor+0x170,shard);memory.put(actor+0x178,region);memory.put(actor+0x24c,int32_t(type));registry[uid]=actor;}};
    zones::Census census;setup();need(zones::read(at,find,image,manager,shard,region,census,error),error);
    need(census.members.size()==4&&census.members[0].type==0&&census.members[3].type==3,"None/Puzzle/Story arrays were omitted from the actual four-type cascade");
    for(int mode=0;mode<14;++mode){setup();const auto actor=actors+3*0x1000;
        if(mode==0)memory.put(manager,image+0x16fe171);if(mode==1)memory.put(manager+0xf0,region+8);if(mode==2)memory.put(region+0x968,manager+8);if(mode==3)memory.put(region+0x70,shard+8);
        if(mode==4)memory.put(manager+0x18+3*0x30,image+0x172e1e1);if(mode==5)memory.put(actor+0x24c,int32_t(1));if(mode==6)memory.put(actor+0x18,uint64_t(0x100000007));
        if(mode==7)memory.put(actor+0x170,shard+8);if(mode==8)memory.put(actor+0x178,region+8);if(mode==9)registry[0x400000007]=actors;
        if(mode==10)memory.put(data+3*0x100,actors);if(mode==11)memory.changing=manager+0x18+3*0x30+8;
        if(mode==12)memory.changing=actor+0x18;if(mode==13)memory.put(manager+0x18+3*0x30+0x14,int32_t(0));
        need(!zones::read(at,find,image,manager,shard,region,census,error)&&!error.empty()&&census.members.empty(),"Foreign or racing non-Monster Zone admitted mode="+std::to_string(mode));
    }
    setup();memory.bytes.erase(actors+3*0x1000+0x18);need(!zones::read(at,find,image,manager,shard,region,census,error)&&error=="original Zone census field read failed","Original non-Monster field cause replaced");
    setup();registry.erase(0x400000007);need(!zones::read(at,find,image,manager,shard,region,census,error)&&error=="original Zone full UID lookup failed","Original non-Monster full UID cause replaced");
    namespace serialized=mt2mp::game::world_zone_serialization;
    setup();serialized::Pool saved;auto links=[&](uintptr_t actor,int32_t& link,std::string&){link=actor==actors?17:0;return true;};
    need(serialized::read(memory,image,manager,links,saved,error),error);
    need(saved.arrays[0][0].link==17&&saved.arrays[3][0].uid==0x400000007,"Four-array serialization lost original links/full UIDs");
    const std::array<std::string,4> labels{"None","Monster","Puzzle","Story"};
    const auto loaded=[&]{serialized::Load value;for(size_t type=0;type<4;++type){need(value.children(labels[type],1,1,error),error);
        need(value.registered(int32_t(type),actors+type*0x1000,type==0?17:0,error),error);}return value;};
    auto load=loaded();serialized::Pool restored;need(load.finish(memory,image,manager,restored,error)&&restored==saved,"Original four-group load differs from original save identities");
    for(int mode=0;mode<7;++mode){auto bad=loaded();
        if(mode==0)bad.active=2;if(mode==1)bad.declared[3]=2;if(mode==2)bad.registrations[0].clear();
        if(mode==3)bad.registrations[0][actors]=18;if(mode==4)bad.registrations[3].emplace(actors+4*0x1000,0);
        if(mode==5)bad.registrations[3].begin()->second=17;if(mode==6)std::swap(bad.registrations[0],bad.registrations[3]);
        const bool result=bad.finish(memory,image,manager,restored,error);
        if(mode==3)need(result&&restored!=saved,"Changed original linked record was silently replaced by save identity");
        else need(!result&&!error.empty(),"Partial/duplicate/migrated original Zone registration admitted mode="+std::to_string(mode));
    }
    serialized::Load partial;need(!partial.children("Monster",0,0,error),"Skipped None enum group admitted");
    need(partial.children("None",1,1,error),error);need(!partial.children("Monster",0,0,error),"Missing original None registration admitted");
    need(!partial.registered(1,actors,0,error),"Registration outside current native enum admitted");
    need(partial.registered(0,actors,0,error),error);need(!partial.registered(0,actors,0,error),"Duplicate native registration admitted");
    need(!partial.children("Monster",1,0,error),"Original child count discrepancy admitted");
    setup();memory.put(actors+3*0x1000+0x18,uint64_t(0x100000007));need(!serialized::read(memory,image,manager,links,restored,error),"Partial UID equality admitted across four arrays");
    setup();memory.bytes.erase(actors+3*0x1000+0x18);need(!serialized::read(memory,image,manager,links,restored,error)&&error=="original Zone serialization field read failed","Original serialization read error lost");
    checks+=zone_edit_identity_cases();
    std::cout<<checks<<" complete four-type Dungeon Zone membership/owner/full UID comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
