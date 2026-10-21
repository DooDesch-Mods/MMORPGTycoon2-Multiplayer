#include "npc_history_graph.h"
#include "npc_history_identity.h"
#include "monster_quest_retirement.h"
#include "mt2mp/core/hash.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace npc_history_fixture {
constexpr uintptr_t image=0x140000000,graph=0x1000,data=0x2000,node=0x3000;
constexpr uint64_t fixture_uid=0x100000002;
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
namespace npc_census=mt2mp::npc_census;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "npc_history_others.inc"
struct Memory final:mt2mp::npc_census::Memory {
    std::map<uintptr_t,unsigned char> contents;uintptr_t unreadable=0;
    template<class T>void set(uintptr_t address,const T& value){const auto raw=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)contents[address+i]=raw[i];}
    void zero(uintptr_t address,size_t count){for(size_t i=0;i<count;++i)contents[address+i]=0;}
    bool copy(uintptr_t address,void* output,size_t count,std::string& error)override{
        auto raw=static_cast<unsigned char*>(output);for(size_t i=0;i<count;++i){const auto found=contents.find(address+i);
            if(address+i==unreadable||found==contents.end()){error="original fixture memory read failed at "+std::to_string(address+i);return false;}raw[i]=found->second;}
        return true;
    }
    void text(uintptr_t address,const std::string& value){set(address,address+16);set(address+8,uint64_t(value.size()));
        for(size_t i=0;i<value.size();++i)set(address+16+i,value[i]);}
};
bool hash(const std::string& bytes,std::string& out,std::string&){out=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}
void setup(Memory& memory,uintptr_t ap,uintptr_t method){
    memory={};memory.set(graph+0x20,data);memory.set(data,node);memory.zero(node,0x188);
    memory.set(node,image+ap);memory.set(image+ap+0x10,image+method);memory.set(node+0xb8,fixture_uid);
    if(ap==0x16de8a0){memory.text(node+0xc0,"Quest giver");memory.set(node+0x170,image+0x17469a0);}
}
void graph_cases(){
    namespace actual=mt2mp::game::npc_history_graph;Memory memory;std::string error;actual::Graph first,second;
    for(const auto& entry:std::array<std::array<uintptr_t,4>,3>{{{0x16de8a0,0x556ff0,0,1},{0x16de860,0x53e1c0,0,0},{0x1786ff0,0x55ed80,1,0}}}){
        const auto ap=entry[0];setup(memory,ap,entry[1]);
        need(actual::read(memory,image,graph,1,fixture_uid,bool(entry[2]),bool(entry[3]),hash,first,error),error);
        need(actual::read(memory,image,graph,1,fixture_uid,bool(entry[2]),bool(entry[3]),hash,second,error)&&first==second,"stable original graph fingerprint changed");
        memory.set(node+0xb8,fixture_uid^uint64_t(1ull<<32));
        need(!actual::read(memory,image,graph,1,fixture_uid,bool(entry[2]),bool(entry[3]),hash,second,error),"partial full UID accepted");
        setup(memory,ap,entry[1]);memory.unreadable=node+0xb8;
        need(!actual::read(memory,image,graph,1,fixture_uid,bool(entry[2]),bool(entry[3]),hash,second,error)&&error=="Quest NPC history graph unreadable: original fixture memory read failed at "+std::to_string(node+0xb8),"original read cause replaced");
    }
    setup(memory,0x16de8a0,0x556ff0);need(actual::read(memory,image,graph,1,fixture_uid,false,true,hash,first,error),error);
    memory.set(node+0xd0,char('X'));need(actual::read(memory,image,graph,1,fixture_uid,false,true,hash,second,error)&&first!=second,"retained Make heap/name mutation invisible");
    setup(memory,0x16de860,0x53e1c0);memory.set(data+8,node);need(!actual::read(memory,image,graph,2,fixture_uid,false,false,hash,second,error),"duplicate graph operation accepted");
    setup(memory,0x16de860,0x53e1c0);memory.set(data+8,uintptr_t(node+0x1000));memory.zero(node+0x1000,0xc8);
    memory.set(node+0x1000,image+0x1786fb0);memory.set(image+0x1786fb0+0x10,image+0x53ccf0);memory.set(node+0x1000+0xb8,int32_t(1000));
    need(actual::read(memory,image,graph,2,fixture_uid,false,false,hash,second,error)&&second.cash==1000,"actual Cash node amount not retained");
    memory.set(node+0x1000,image+0x1786ff0);need(!actual::read(memory,image,graph,2,fixture_uid,false,false,hash,second,error),"foreign Move node in Destroy graph accepted");
    setup(memory,0x16de8a0,0x556ff0);memory.set(data+8,uintptr_t(node+0x1000));memory.zero(node+0x1000,0xe0);
    memory.set(node+0x1000,image+0x1713330);memory.set(image+0x1713330+0x10,image+0x563f50);memory.set(node+0x1000+0xb8,fixture_uid);memory.set(node+0x1000+0xc0,uint8_t(1));
    need(actual::read(memory,image,graph,2,fixture_uid,false,true,hash,second,error),"own true SetNPCQuests restore rejected: "+error);
    namespace qr=mt2mp::game::monster_quest_retirement;qr::Owner owner;owner.uid=fixture_uid;
    need(!qr::inverse(memory,image,node+0x1000,owner,error),"existing incoming false-only guard weakened");
    memory.set(node+0x1000+0xc0,uint8_t(0));need(actual::read(memory,image,graph,2,fixture_uid,false,true,hash,second,error),"self-target incoming false snapshot rejected");
    memory.set(node+0x1000+0xb8,uint64_t(fixture_uid+1));need(actual::read(memory,image,graph,2,fixture_uid,false,true,hash,second,error),"incoming false inverse rejected");
    memory.set(node+0x1000+0xc0,uint8_t(1));need(!actual::read(memory,image,graph,2,fixture_uid,false,true,hash,second,error),"incoming true inverse accepted");
}
void identity_cases(){
    namespace q=mt2mp::game::npc_saved_identity;namespace actual=mt2mp::game::npc_history_identity;
    q::SavedPool original;original.region=7;original.pool.manager=0x9000;original.pool.records.push_back({0xa000,fixture_uid,0,0,3});
    const auto key=q::key(std::string(64,'a'),7,original.pool.records[0]);mt2mp::npc_census::State census;census.manager=original.pool.manager;
    mt2mp::npc_census::Row restored;restored.local_pointer=0xb000;restored.local_uid=fixture_uid;restored.type=0;census.rows={restored};
    auto find=[&](uint64_t value,uintptr_t& pointer,std::string&){pointer=value==fixture_uid?restored.local_pointer:0;return true;};
    std::string error;auto saved=original;
    need(actual::restore(std::string(64,'a'),key,saved,census,0xa000,fixture_uid,0xb000,find,error)&&saved.pool.records[0].pointer==0xb000,"fresh original full UID did not retain saved key: "+error);
    for(unsigned mutation=0;mutation<7;++mutation){saved=original;auto changed=census;
        if(mutation==0)changed.manager++;if(mutation==1)changed.rows.clear();if(mutation==2)changed.rows[0].local_uid^=1ull<<32;
        if(mutation==3)changed.rows[0].type=1;if(mutation==4)changed.rows.push_back(changed.rows[0]);
        if(mutation==5){auto foreign=restored;foreign.local_pointer=0xa000;foreign.local_uid++;changed.rows.push_back(foreign);}
        if(mutation==6)saved.pool.records[0].pointer++;
        const auto before=saved.pool.records;need(!actual::restore(std::string(64,'a'),key,saved,changed,0xa000,fixture_uid,0xb000,find,error)&&saved.pool.records==before&&!error.empty(),"invalid saved rebind changed original alias");
    }
    saved=original;auto fail_find=[](uint64_t,uintptr_t&,std::string& cause){cause="original lookup exception";return false;};
    need(!actual::restore(std::string(64,'a'),key,saved,census,0xa000,fixture_uid,0xb000,fail_find,error)&&error=="original lookup exception","saved restore lost original lookup cause");
}
void survivor_cases(){
    npc_census::State before,after;before.manager=after.manager=0x9000;before.next_local_tag=after.next_local_tag=4;
    npc_census::Row target,second,last,guard;target.local_pointer=100;target.local_uid=1;target.type=0;
    second=target;second.local_pointer=200;second.local_uid=2;last=target;last.local_pointer=300;last.local_uid=3;
    guard=target;guard.local_pointer=400;guard.local_uid=4;guard.type=1;
    before.rows={target,second,last,guard};after=before;after.rows={last,second,guard};std::string error;
    need(unaffected(before,after,1,false,true,{},error),"original type-local swap-last retirement rejected: "+error);
    auto stable_erase=after;stable_erase.rows={second,last,guard};need(!unaffected(before,stable_erase,1,false,true,{},error),"wrong stable erase order accepted");
    after.rows[1].quest_count=2;need(!unaffected(before,after,1,false,true,{},error),"unproved other Quest count change accepted");
    need(unaffected(before,after,1,false,true,{2},error),"proven incoming Quest owner count change rejected: "+error);
    after.rows[1].level++;need(!unaffected(before,after,1,false,true,{2},error),"incoming Quest cascade changed unrelated NPC level");
    after=before;after.rows[0].position={1,2,3};need(unaffected(before,after,1,false,false,{},error),"Move target pose should be checked separately");
    after.rows[1].position={1,2,3};need(!unaffected(before,after,1,false,false,{},error),"Move changed another NPC pose");
    auto absent=before;absent.rows={second,last,guard};after=absent;after.rows={second,last,target,guard};after.next_local_tag++;
    need(unaffected(absent,after,1,true,false,{},error),"original type-local append and fresh tag restore rejected: "+error);
    after.next_local_tag++;need(!unaffected(absent,after,1,true,false,{},error),"multiple fresh native tags accepted");
}
}
int main()try{npc_history_fixture::graph_cases();npc_history_fixture::identity_cases();npc_history_fixture::survivor_cases();std::cout<<npc_history_fixture::checks<<" actual NPC history graph and restored identity checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
