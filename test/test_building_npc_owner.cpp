#include "building_npc_owner.h"
#include "npc_target_key.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace child=mt2mp::game::building_npc_lifecycle;
namespace owner=mt2mp::game::building_npc_owner;
namespace keys=mt2mp::game::building_npc_key;
namespace census=mt2mp::npc_census;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t changing=0;unsigned hits=0;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    template<class T>bool read(uintptr_t address,size_t offset,T& value,std::string& error){auto* data=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto found=bytes.find(address+offset+i);if(found==bytes.end()){error="fixture unreadable exact native field";return false;}data[i]=found->second;}
        if(changing==address+offset&&++hits==2)data[0]^=8;return true;}
};
int main()try{
    constexpr uintptr_t image=0x10000000,root=0x20000000,region=0x21000000,manager=0x22000000,parent=0x30000000,npc=0x40000000,node=0x50000000;
    constexpr uint64_t parent_uid=0x100000005,npc_uid=0x200000005,character_uid=0x300000005;
    const std::string parent_key="action:12345678-1234-1234-1234-123456789abc";
    Memory memory;std::map<uint64_t,uintptr_t> registry;std::string error;size_t checks=0;census::State rows;census::Row selected;
    child::Parent building;child::Receipt before;child::MakeNPCInverse inverse;bool present=false;
    const auto need=[&](bool okay,const std::string& label){++checks;if(!okay)throw std::runtime_error(label+" comparison="+std::to_string(checks)+" cause="+error);};
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return memory.read(p,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& p,std::string&){const auto item=registry.find(uid);p=item==registry.end()?0:item->second;return true;};
    auto text=[&](uintptr_t p,size_t offset,std::string& value,std::string& cause){if(p!=node||offset!=0xc0){cause="fixture unexpected native name owner";return false;}value=before.original_display_name;return true;};
    const auto setup=[&]{memory={};registry={{parent_uid,parent},{npc_uid,npc}};error.clear();rows={};rows.manager=manager;
        building={parent_key,parent,parent_uid,7,child::MakeForm::Functional,npc_uid};
        census::Row row;row.local_pointer=npc;row.local_uid=npc_uid;row.type=0;row.character_type=1;row.character_slot=2;row.level=5;row.name="Innkeeper";rows.rows={row};
        memory.put(parent,image+0x16d93c0);memory.put(parent+0x18,parent_uid);memory.put(parent+0x170,root);memory.put(parent+0x178,region);memory.put(parent+0xa30,npc_uid);
        memory.put(region+0x70,root);memory.put(region+0x834,int32_t(7));memory.put(region+0x998,manager);memory.put(npc+0x268,parent);
        before={};before.parent=building;before.present=true;before.child={keys::make(parent_key),7,row};before.original_record_sha=std::string(64,'a');
        before.original_character_uid=character_uid;before.original_shard=root;before.original_region=region;before.original_display_name=row.name;
        before.original_transform[2]=42;
        memory.put(node,image+0x16de8a0);memory.put(node+0xb8,npc_uid);memory.put(node+0xe0,row.type);memory.put(node+0xe8,root);memory.put(node+0xf0,region);
        memory.put(node+0xf8,before.original_transform);memory.put(node+0x164,row.level);memory.put(node+0x168,std::array<int32_t,2>{1,2});
        memory.put(node+0x170,image+0x17469a0);memory.put(node+0x178,uintptr_t(0x60000000));memory.put(node+0x180,int32_t(0));memory.put(node+0x184,int32_t(4));
    };
    const auto read=[&]{return owner::read(at,find,{image,root,region},building,rows,selected,present,error);};
    setup();need(read()&&present&&selected.local_uid==npc_uid,"reciprocal full UID child");
    need(keys::valid(keys::make(parent_key))&&mt2mp::game::npc_target_key::valid(keys::make(parent_key)),"child canonical key admitted");
    need(mt2mp::subscriber_quest_state::valid_key(keys::make(parent_key)),"Host Frame child owner key admitted");
    need(mt2mp::game::npc_target_key::request(keys::make(parent_key),{}),"child target has no unrelated free NPC birth UUID");
    for(const auto& value:std::vector<std::string>{"building-npc:","building-npc:"+keys::make(parent_key),"building-npc:action:123","building-npc:action:12345678-1234-1234-1234-123456789abC"})need(!keys::valid(value),"invalid or nested child owner key");
    setup();registry[parent_uid]=parent+8;need(!read()&&error.find("parent UID registry")!=std::string::npos,"foreign full parent UID");
    setup();registry[npc_uid]=npc+8;need(!read(),"foreign full child UID");
    setup();memory.put(npc+0x268,parent+8);need(!read(),"foreign reciprocal parent");
    setup();memory.put(parent+0xa30,uint64_t(5));need(!read(),"same low bits do not replace full child UID");
    setup();rows.rows.clear();need(!read(),"child absent from complete Region census");
    setup();rows.rows.push_back(rows.rows.front());need(!read(),"duplicate complete census child");
    setup();memory.put(region+0x998,manager+8);need(!read(),"wrong current Region manager");
    setup();memory.put(parent+0x170,root+8);need(!read(),"wrong root Shard");
    setup();memory.put(region+0x834,int32_t(8));need(!read(),"wrong saved Region");
    setup();memory.put(parent+0xa30,uint64_t(0));memory.put(npc+0x268,uintptr_t(0));need(read()&&!present,"explicit no-child receipt");
    setup();memory.put(parent+0xa30,uint64_t(0));need(!read(),"hidden reciprocal child blocks empty a30");
    setup();memory.changing=parent+0xa30;need(!read(),"parent changed during witness");
    setup();need(!owner::absent(find,rows,rows.rows.front(),error),"registered child cannot be retired");
    setup();const auto old=rows.rows.front();registry.erase(npc_uid);need(!owner::absent(find,rows,old,error),"unregistered but census-live child cannot be retired");
    rows.rows.clear();need(owner::absent(find,rows,old,error),"actual original full UID and complete census absence");
    setup();need(owner::restored_uid(building,npc_uid,before.child.row,before.child.row,error),"validated MakeNPC same UID restore");
    building.make_form=child::MakeForm::Modular;building.stored_child_uid=0;
    need(!owner::restored_uid(building,0,before.child.row,before.child.row,error),"Modular form alone cannot authorize a fresh native UID");
    auto recreated=before.child.row;recreated.local_pointer+=0x1000;
    need(owner::restored_uid(building,npc_uid,before.child.row,recreated,error),"new process pointer preserves original saved child UID");
    recreated.local_uid+=uint64_t(1)<<32;need(!owner::restored_uid(building,npc_uid,before.child.row,recreated,error),"restored full UID mismatch");
    setup();need(owner::make_npc_inverse(at,text,image,node,before,inverse,error)&&inverse.produced_uid==npc_uid&&inverse.required_character_uid==character_uid,"complete original MakeNPC stored fields");
    for(const size_t offset:{size_t(0),size_t(0xb8),size_t(0xe0),size_t(0xe8),size_t(0xf0),size_t(0xf8),size_t(0x164),size_t(0x168),size_t(0x170),size_t(0x180)}){
        setup();memory.bytes[node+offset]^=1;need(!owner::make_npc_inverse(at,text,image,node,before,inverse,error),"tampered MakeNPC stored field");}
    setup();before.original_record_sha.clear();need(!owner::make_npc_inverse(at,text,image,node,before,inverse,error),"inverse without complete before Record");
    constexpr uintptr_t set=0x70000000,table=0x71000000,values=0x72000000,link=0x73000000;
    const auto set_up=[&]{memory={};memory.put(set,table);memory.put(set+8,int32_t(2));memory.put(set+0xc,int32_t(31));memory.put(set+0x18,values);memory.put(set+0x20,int32_t(1));memory.put(set+0x24,int32_t(4));memory.put(values,npc_uid);
        const auto hash=uint32_t(npc_uid&0xffffffffull);const size_t bucket=size_t((hash*0x9e3779ffu)>>31);for(size_t i=0;i<2;++i)memory.put(table+i*24+0x10,i==bucket?link:uintptr_t(0));memory.put(link,npc_uid);memory.put(link+8,hash);memory.put(link+0x10,uintptr_t(0));};
    set_up();need(owner::inverse_uid_set(at,set,npc_uid,error),"complete original singleton dependency set");
    set_up();memory.put(values,npc_uid+(uint64_t(1)<<32));need(!owner::inverse_uid_set(at,set,npc_uid,error),"dependency vector retains full UID");
    set_up();memory.put(link+8,uint32_t(0));need(!owner::inverse_uid_set(at,set,npc_uid,error),"dependency raw hash mismatch");
    set_up();memory.put(link+0x10,link);need(!owner::inverse_uid_set(at,set,npc_uid,error),"dependency chain cycle");
    set_up();memory.put(set+0x20,int32_t(2));need(!owner::inverse_uid_set(at,set,npc_uid,error),"extra dependency member count");
    set_up();memory.changing=set+0x18;need(!owner::inverse_uid_set(at,set,npc_uid,error),"dependency header changed during read");
    std::cout<<"Building child NPC ownership/inverse "<<checks<<" comparisons passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
