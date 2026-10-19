#include "typed_action/action_session.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
struct Memory{std::map<uintptr_t,std::vector<uint8_t>> bytes;std::map<uintptr_t,std::string> strings;
    template<class T>void set(uintptr_t p,const T& v){auto& b=bytes[p];b.resize(sizeof v);std::memcpy(b.data(),&v,sizeof v);}};
struct Runtime{uintptr_t image=0x140000000;Memory memory;};
struct LocalView{uintptr_t region=0x2000;std::vector<uint64_t> building_uids,scenery_uids,path_uids,path_node_uids,network_uids;};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(Runtime& r,uintptr_t p,size_t o,T& value,std::string& e){auto found=r.memory.bytes.find(p+o);
    if(found==r.memory.bytes.end()||found->second.size()!=sizeof value)return fail(e,"fixture field absent");
    std::memcpy(&value,found->second.data(),sizeof value);return true;}
bool text(Runtime& r,uintptr_t p,std::string& out,std::string& e){auto found=r.memory.strings.find(p);
    if(found==r.memory.strings.end())return fail(e,"fixture string absent");out=found->second;return true;}
namespace sale_native {
struct UndoWitness{std::vector<uintptr_t> undo;};
struct GraphWitness{uint32_t operations=0;};
bool read_graph(Memory& m,uintptr_t,uintptr_t p,GraphWitness& out,std::string& e){auto found=m.bytes.find(p+0x28);
    if(found==m.bytes.end())return fail(e,"fixture graph absent");std::memcpy(&out.operations,found->second.data(),4);return true;}
bool validate_undo_commit(const UndoWitness& before,const UndoWitness& after,const GraphWitness&,std::string& e){
    return after.undo.size()==before.undo.size()+1||fail(e,"fixture graph count changed");}
}
#include "region_sale_inverse_actual.inc"
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
int main(){try{unsigned count=0;Runtime r;LocalView view;action::RegionPropertyState before;
    before.type=1;before.level=2;before.max_level=3;before.appearance=action::RegionAppearance{};
    before.appearance->tint={1,0.5f,0.25f,1};before.content.styles={"one","two","three","four"};
    sale_native::UndoWitness a,b;b.undo={0x3000};const std::array<float,3> hit{10,20,30};
    auto& m=r.memory;m.set(0x3020,uintptr_t{0x4000});m.set(0x3028,uint32_t{2});m.set(0x4000,uintptr_t{0x5000});m.set(0x4008,uintptr_t{0x6000});
    m.set(0x5000,r.image+0x1723230);m.set(0x50b8,view.region);m.set(0x50c0,int32_t{1});m.set(0x50c4,std::array<int32_t,2>{2,3});
    m.set(0x5160,uint8_t{1});m.set(0x5150,before.appearance->tint);
    for(size_t i=0;i<4;++i)m.strings[0x50d0+i*0x20]=before.content.styles[i];
    m.set(0x6000,r.image+0x1786fb0);m.set(0x60b8,int32_t{-500});m.set(0x60bc,hit);
    std::string error;need(sale_inverse(r,view,before,a,b,500,hit,error),error);++count;
    auto reject=[&](auto mutate){auto bad=r;mutate(bad);need(!sale_inverse(bad,view,before,a,b,500,hit,error),"invalid original sale inverse accepted");++count;};
    reject([](auto& bad){bad.memory.set(0x60b8,int32_t{500});});
    reject([](auto& bad){bad.memory.set(0x50b8,uintptr_t{0x2008});});
    reject([](auto& bad){bad.memory.set(0x5160,uint8_t{0});});
    reject([](auto& bad){bad.memory.set(0x50c4,std::array<int32_t,2>{0,0});});
    reject([](auto& bad){bad.memory.strings[0x50d0]="changed";});
    reject([](auto& bad){bad.memory.set(0x6000,bad.image+0x1786fb8);});
    reject([](auto& bad){bad.memory.set(0x3028,uint32_t{1});});
    m.set(0x3028,uint32_t{1});need(sale_inverse(r,view,before,a,b,0,hit,error),error);++count;
    view.building_uids={11};view.scenery_uids={12};view.path_uids={13};view.path_node_uids={14};view.network_uids={15};
    need(sale_members(view,view,error),error);++count;
    for(unsigned family=0;family<5;++family){auto changed=view;
        if(family==0)changed.building_uids.clear();if(family==1)changed.scenery_uids.clear();if(family==2)changed.path_uids.clear();
        if(family==3)changed.path_node_uids.clear();if(family==4)changed.network_uids.clear();
        need(!sale_members(view,changed,error),"sale lost existing family");++count;}
    std::cout<<count<<" actual sale inverse/member cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
