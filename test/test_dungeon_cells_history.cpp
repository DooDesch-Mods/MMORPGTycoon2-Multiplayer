#include "dungeon_cell_graph.h"
#include "zone_edit_stored.h"
#include "dungeon_gizmo_restore.h"
#include "dungeon_gizmo_identity.h"
#include "direct_monster_identity.h"
#include "zone_quest_history.h"
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
Memory fixture_kinds(const std::vector<size_t>& kinds){Memory memory;std::vector<uintptr_t> nodes;
    for(size_t i=0;i<kinds.size();++i)nodes.push_back(0x10000+i*0x1000);
    array(memory,graph+0x18,0x16e2dc0,0x2000,nodes);auto sorted=nodes;std::reverse(sorted.begin(),sorted.end());array(memory,graph+0x30,0x1748aa0,0x3000,sorted);
    for(size_t i=0;i<nodes.size();++i){const auto& descriptor=cg::descriptors[kinds[i]];const auto node=nodes[i];memory.zero(node,0x200);
        memory.put(node,image+descriptor.ap);memory.put(image+descriptor.ap+0x10,image+descriptor.method);memory.put(node+0xb8,uint64_t(0x100000001+i));
        for(size_t set=0;set<3;++set)uid_set(memory,node+8+set*0x28,0x100000+0x5000*(i*3+set),set==0?0:uint64_t(0x200000001+set));
        array(memory,node+0xa0,0x1749040,0x7000+i*0x100,{fixture_region});
    }return memory;
}
Memory fixture(size_t third){std::vector<size_t> kinds{0,1};if(third<10)kinds.push_back(third);return fixture_kinds(kinds);}
#include "zone_edit_graph_cases.inc"
#include "zone_quest_history_cases.inc"
int main()try{
    unsigned checks=0;const auto check=[&](bool ok,const std::string& error){++checks;if(!ok)throw std::runtime_error(error+" case="+std::to_string(checks));};std::string error;
    const auto read=[&](Memory& memory,cg::Graph& out,cg::Policy policy=cg::Policy{}){auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){return memory.read(p+offset,&value,sizeof value,e);};
        auto copy=[&](uintptr_t p,void* bytes,size_t count,std::string& e){return memory.read(p,bytes,count,e);};
        auto values=[](cg::Node& node,std::string&){node.values.push_back("complete-owned-value");return true;};
        auto owner=[](const cg::Node&,uintptr_t p,std::string& e){if(p!=fixture_region){e="foreign original Region";return false;}return true;};
        return cg::read(memory,at,copy,values,owner,image,graph,out,error,policy);};
    for(size_t kind=2;kind<10;++kind){auto memory=fixture(kind);cg::Graph value;check(read(memory,value),error);
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
    const auto tool_fixture=[&](const std::vector<size_t>& kinds){auto memory=fixture_kinds(kinds);
        for(size_t i=0;i<kinds.size();++i)if(cg::descriptors[kinds[i]].kind==cg::Kind::Cash){const auto node=uintptr_t(0x10000+i*0x1000);
            memory.put(node+0xb8,int32_t(50));memory.put(node+0xbc,std::array<uint32_t,3>{});
            for(size_t set=0;set<3;++set)uid_set(memory,node+8+set*0x28,0x100000+0x5000*(i*3+set),0);}
        return memory;};
    {auto memory=fixture_kinds({0,1,11});cg::Graph parsed;
        check(!read(memory,parsed),"Existing Cells policy must not admit a new Gizmo Move node");}
    for(const auto& [kinds,policy]:std::vector<std::pair<std::vector<size_t>,cg::Policy>>{
        {{11,11},{cg::Shape::SceneMove,0,2}},{{2,6,10,10},{cg::Shape::SceneControls,2,2}},{{2,10},{cg::Shape::SceneControls,1,1}},{{8,8,10,10},{cg::Shape::SceneControls,2,2}},
        {{11,11},{cg::Shape::GizmoMove,0,2}},{{10,10,8,8},{cg::Shape::Gizmos,2,2}},{{6,6,10,10},{cg::Shape::Gizmos,2,2}}}){
        auto memory=tool_fixture(kinds);cg::Graph parsed;check(read(memory,parsed,policy),error);
        auto wrong=policy;++wrong.targets;check(!read(memory,parsed,wrong),"Gizmo graph omitted a complete target");
        wrong=policy;++wrong.cash;check(!read(memory,parsed,wrong),"Gizmo graph accepted a changed Cash count");
        if(policy.shape==cg::Shape::GizmoMove){memory.put(0x10000,image+cg::descriptors[8].ap);check(!read(memory,parsed,policy),"Move admitted a Destroy instead of original Move node");}
    }
    for(const auto& [kinds,policy]:std::vector<std::pair<std::vector<size_t>,cg::Policy>>{
        {{10,8},{cg::Shape::Door,1}},{{8},{cg::Shape::Key,0}},{{8,10},{cg::Shape::Key,1}},
        {{8},{cg::Shape::Monster,0}},{{0},{cg::Shape::Paint,0}},{{10,7},{cg::Shape::Door,1}},{{10,6},{cg::Shape::Key,1}},{{3},{cg::Shape::Monster,0}}}){
        auto memory=tool_fixture(kinds);cg::Graph value;check(read(memory,value,policy),error);
        check(value.nodes.size()==kinds.size(),"Tool graph omitted an original operation");
        for(const auto& node:value.nodes)if(node.kind==cg::Kind::Cash)check(!node.uid&&node.values==std::vector<std::string>{"50","0","0","0"},"Cash amount/position misread as a target UID");
    }
    for(const auto& [kinds,policy]:std::vector<std::pair<std::vector<size_t>,cg::Policy>>{
        {{2,10},{cg::Shape::Gizmos,1,1}},{{0,6,10},{cg::Shape::SceneControls,1,2}},{{2},{cg::Shape::SceneMove,0,1}},
        {{8},{cg::Shape::Door,1}},{{8,10},{cg::Shape::Monster,0}},{{8,10,10},{cg::Shape::Key,1}},
        {{0,1},{cg::Shape::Paint,0}},{{0,1,10},{cg::Shape::Cells,0}},{{3},{cg::Shape::Key,0}},{{6},{cg::Shape::Monster,0}}}){
        auto memory=tool_fixture(kinds);cg::Graph value;check(!read(memory,value,policy)&&!error.empty(),"Wrong original tool graph shape accepted");
    }
    {auto memory=tool_fixture({10,8});cg::Graph value;memory.put(0x10000+0xbc,uint32_t(0x7fc00000));
        check(!read(memory,value,{cg::Shape::Door,1})&&error.find("Cash position")!=std::string::npos,"Nonfinite Cash position accepted");}
    {auto memory=tool_fixture({10,8});cg::Graph value;memory.bytes.erase(0x10000+0xbc);
        check(!read(memory,value,{cg::Shape::Door,1})&&error.find("fixture unmapped")!=std::string::npos,"Original Cash read cause lost");}
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
    checks+=zone_edit_graph_cases();
    checks+=zone_quest_history_cases();
    std::cout<<checks<<" complete Dungeon original graph, exact Make return and canonical actor restoration comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
