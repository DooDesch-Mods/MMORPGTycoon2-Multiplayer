#include "dungeon_cell_graph.h"
#include "dungeon_gizmo_restore.h"
#include "dungeon_gizmo_identity.h"
#include "direct_monster_identity.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace cg=mt2mp::game::dungeon_cell_graph;
namespace restore=mt2mp::game::dungeon_gizmo_restore;
namespace gizmo=mt2mp::game::dungeon_gizmo_identity;
namespace monster=mt2mp::game::direct_monster_identity;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    void zero(uintptr_t pointer,size_t count){for(size_t i=0;i<count;++i)bytes[pointer+i]=0;}
    template<class T>void put(uintptr_t pointer,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[pointer+i]=data[i];}
    bool read(uintptr_t pointer,void* target,size_t count,std::string& error){auto* out=static_cast<unsigned char*>(target);for(size_t i=0;i<count;++i){const auto found=bytes.find(pointer+i);
        if(found==bytes.end()){error="fixture unmapped original graph read="+std::to_string(pointer+i);return false;}out[i]=found->second;}return true;}
};
constexpr uintptr_t image=0x140000000,graph=0x1000,fixture_region=0x9000;
void array(Memory& memory,uintptr_t at,uintptr_t ap,uintptr_t data,const std::vector<uintptr_t>& rows){
    memory.put(at,image+ap);memory.put(at+8,data);memory.put(at+0x10,int32_t(rows.size()));memory.put(at+0x14,int32_t(rows.size()+4));
    for(size_t i=0;i<rows.size();++i)memory.put(data+i*8,rows[i]);
}
void uid_set(Memory& memory,uintptr_t object,uintptr_t buffer,uint64_t uid){
    const auto table=buffer,values=buffer+0x1000,node=buffer+0x2000;memory.zero(table,32*24);
    memory.put(object,table);memory.put(object+8,int32_t(32));memory.put(object+0xc,int32_t(27));memory.put(object+0x18,values);
    memory.put(object+0x20,int32_t(uid?1:0));memory.put(object+0x24,int32_t(4));memory.zero(values,32);
    if(uid){const auto hash=uint32_t(uid);memory.put(values,uid);memory.put(table+size_t((hash*0x9e3779ffu)>>27)*24+0x10,node);
        memory.put(node,uid);memory.put(node+8,hash);memory.put(node+0x10,uintptr_t(0));}
}
Memory fixture(size_t third){Memory memory;std::vector<uintptr_t> nodes{0x10000,0x11000};if(third<cg::descriptors.size())nodes.push_back(0x12000);
    array(memory,graph+0x18,0x16e2dc0,0x2000,nodes);auto sorted=nodes;std::reverse(sorted.begin(),sorted.end());array(memory,graph+0x30,0x1748aa0,0x3000,sorted);
    for(size_t i=0;i<nodes.size();++i){const auto& descriptor=cg::descriptors[i<2?i:third];const auto node=nodes[i];memory.zero(node,0x200);
        memory.put(node,image+descriptor.ap);memory.put(image+descriptor.ap+0x10,image+descriptor.method);memory.put(node+0xb8,uint64_t(0x100000001+i));
        for(size_t set=0;set<3;++set)uid_set(memory,node+8+set*0x28,0x100000+0x5000*(i*3+set),set==0?0:uint64_t(0x200000001+set));
        array(memory,node+0xa0,0x1749040,0x7000+i*0x100,{fixture_region});
    }return memory;
}
int main()try{
    unsigned checks=0;const auto check=[&](bool ok,const std::string& error){++checks;if(!ok)throw std::runtime_error(error+" case="+std::to_string(checks));};std::string error;
    const auto read=[&](Memory& memory,cg::Graph& out){auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){return memory.read(p+offset,&value,sizeof value,e);};
        auto copy=[&](uintptr_t p,void* bytes,size_t count,std::string& e){return memory.read(p,bytes,count,e);};
        auto values=[](cg::Node& node,std::string&){node.values.push_back("complete-owned-value");return true;};
        auto owner=[](const cg::Node&,uintptr_t p,std::string& e){if(p!=fixture_region){e="foreign original Region";return false;}return true;};
        return cg::read(memory,at,copy,values,owner,image,graph,out,error);};
    for(size_t kind=2;kind<cg::descriptors.size();++kind){auto memory=fixture(kind);cg::Graph value;check(read(memory,value),error);
        check(value.nodes.size()==3&&value.nodes[2].kind==cg::descriptors[kind].kind&&value.execution==std::vector<uintptr_t>{0x12000,0x11000,0x10000},"Complete concrete original node or native sorted order lost");
        check(value.nodes[2].dependencies[1]==std::vector<uint64_t>{0x200000002}&&value.nodes[2].dependencies[2]==std::vector<uint64_t>{0x200000003}&&value.nodes[2].values.size()==1,"Full UID dependency or complete owned value omitted");}
    for(int mode=0;mode<10;++mode){auto memory=fixture(6);cg::Graph value;
        if(mode==0)memory.put(0x12000,uintptr_t(image+0x123));
        if(mode==1)memory.put(image+cg::descriptors[6].ap+0x10,uintptr_t(image+0x124));
        if(mode==2)memory.put(0x12000+0xb8,uint64_t(0));
        if(mode==3)memory.put(0x3000,uintptr_t(0xdead));
        if(mode==4)memory.put(0x3000+8,uintptr_t(0x12000));
        if(mode==5)memory.put(0x7000+0x200,uintptr_t(fixture_region+1));
        if(mode==6)memory.put(0x10000+8+0x28+0x20,int32_t(2));
        if(mode==7)memory.put(0x100000+0x5000+0x1000,uint64_t(0x300000002));
        if(mode==8)memory.put(0x10000+8+0x28+0xc,int32_t(26));
        if(mode==9)memory.put(graph+0x18+0x10,int32_t(0));
        check(!read(memory,value)&&!error.empty(),"Invalid original complete graph accepted");}
    restore::Receipts receipts;std::vector<restore::Created> made;uintptr_t node=0;
    check(receipts.prepare({{0x10000,0x70000},{0x11000,0x71000}},error),error);
    check(!receipts.complete(made,error),"Missing original return receipts accepted");
    for(int mode=0;mode<5;++mode)check(!receipts.associate(mode==0?0x70001:0x70000,mode==1?0x100c1:0x100c0,mode==2?0x100e1:0x100e0,mode==3?0x10109:mode==4?1:0x10108,node,error),"Foreign Make argument or receiver admitted");
    check(receipts.associate(0x70000,0x100c0,0x100e0,0x10108,node,error)&&node==0x10000&&receipts.record(node,0x80000,error),error);
    check(!receipts.associate(0x70000,0x100c0,0x100e0,0x10108,node,error),"Original Make return replay accepted");
    check(!receipts.record(0x11000,0x80000,error),"Two Make nodes aliased the same returned object");
    check(receipts.record(0x11000,0x81000,error)&&receipts.complete(made,error)&&made.size()==2,error);
    const std::string sha(64,'a'),uuid="11111111-1111-4111-8111-111111111111";gizmo::Scope scope{uuid,sha,2,5,0x1000};
    const auto key=gizmo::record(sha,67,3,-1,0);gizmo::Member old{sha+":world:building:17/template",0x2000,0x3000,0x4000,0x5000,0x100000012};
    auto actual=old;actual.object=0x2100;actual.uid=0x200000013;gizmo::Registry registry;
    check(registry.seal(scope,{{key,old}},nullptr,error),error);check(!registry.restore_retired(scope,key,old,actual,error),"Live Gizmo alias restored without retirement");
    check(registry.retire(scope,key,old,error)&&registry.restore_retired(scope,key,old,actual,error)&&registry.verify_roster(scope,{actual},error),error);
    std::string resolved;check(registry.resolve(scope,actual.object,actual.uid,resolved,error)&&resolved==key,"Fresh native UID changed canonical Gizmo identity");
    check(!registry.resolve(scope,old.object,old.uid,resolved,error),"Retired native pointer still resolves after actual restoration");
    auto other=actual;other.uid^=uint64_t(1)<<32;check(!registry.restore_retired(scope,key,old,other,error),"Replayed old Gizmo return receipt admitted");
    monster::Registry monsters;monster::Scope monster_scope{uuid,2,5,0x1000,0x5000,0x6000};monster::Entry original{monster_scope,0x7000,0x300000012,true,false};
    const std::string monster_id="22222222-2222-4222-8222-222222222222";check(monsters.stage(monster_id,monster_scope,original.pointer,original.uid,error)&&monsters.confirm(monster_id,error)&&monsters.retire(monster_id,original,error),error);
    auto returned=original;returned.pointer=0x7100;auto wrong=returned;wrong.uid^=uint64_t(1)<<32;
    check(!monsters.restore_retired(monster_id,original,wrong,error),"Monster restoration accepted only matching low32 UID");
    check(monsters.restore_retired(monster_id,original,returned,error),error);
    check(!monsters.restore_retired(monster_id,original,returned,error),"Monster original restoration replay accepted");
    std::cout<<checks<<" complete Dungeon original graph, exact Make return and canonical actor restoration comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
