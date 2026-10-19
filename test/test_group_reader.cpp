#include "group_reader.h"
#include "group_current_identity.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace f {
namespace gr=mt2mp::game::group_reader;
size_t checks=0;void need(bool v,const std::string& e){++checks;if(!v)throw std::runtime_error(e);}
struct Memory {std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t p,const T& v){const auto* data=reinterpret_cast<const uint8_t*>(&v);for(size_t i=0;i<sizeof v;++i)bytes[p+i]=data[i];}
    bool read(uintptr_t p,void* out,size_t n,std::string& e){auto* data=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){const auto found=bytes.find(p+i);if(found==bytes.end()){e="fixture missing address="+std::to_string(p+i);return false;}data[i]=found->second;}return true;}
};
constexpr uintptr_t image=0x140000000,root=0x10000,region=0x20000,manager=0x30000,selection=0x40000,group=0x50000,member=0x60000,scenery=0x70000,free_prop=0x80000;
constexpr uint64_t group_uid=0x100000007,member_uid=0x200000007,free_uid=0x300000007;
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
void array(Memory& m,uintptr_t p,uintptr_t ap,std::vector<uintptr_t> rows){const auto data=p+0x2000;m.put(p,image+ap);m.put(p+8,data);m.put(p+16,int32_t(rows.size()));m.put(p+20,int32_t(rows.size()));for(size_t i=0;i<rows.size();++i)m.put(data+i*8,rows[i]);}
Memory setup(){Memory m;m.put(image+0x13df3e0,manager);m.put(manager,image+0x1700890);array(m,manager+0x18,0x16f0060,{group});
    m.put(group,image+0x1749de0);m.put(group+0x18,group_uid);m.put(group+0x170,root);m.put(group+0x178,region);m.put(group+0x180,uintptr_t(0));m.put(group+0x100,pose());array(m,group+0x210,0x16ea660,{member});
    m.put(member,image+0x16fbc40);m.put(member+0x18,scenery);m.put(member+0x64,pose());
    for(auto p:{scenery,free_prop}){m.put(p,image+0x16c1390);m.put(p+0x18,p==scenery?member_uid:free_uid);m.put(p+0x170,root);m.put(p+0x180,p==scenery?group:uintptr_t(0));m.put(p+0x100,pose());}
    m.put(image+0x13df630,selection);m.put(selection,image+0x17205d0);m.put(selection+0xd1,uint8_t(0));m.put(selection+0xd3,uint8_t(0));array(m,selection+0x20,0x1748c50,{group,free_prop});array(m,selection+0x38,0x1748c50,{group,free_prop});return m;
}
bool bind(uintptr_t p,uint64_t uid,std::string& key,int32_t& id,std::string& e){if((p==scenery&&uid==member_uid)||(p==free_prop&&uid==free_uid)){key=p==scenery?"action:member":"action:free";id=67;return true;}e="foreign full scenery identity";return false;}
bool owner(uintptr_t p,int32_t& id,std::string& e){id=67;if(p==region)return true;e="foreign region";return false;}
bool find(uint64_t uid,uintptr_t& p,std::string&){p=uid==group_uid?group:0;return true;}
void run(){auto m=setup();gr::Local value;std::string e;
    auto read=[&](){return gr::read(m,image,root,selection,{free_prop},bind,owner,find,value,e);};
    need(read(),e);need(value.state.groups.size()==1&&value.state.props.size()==2,"complete groups/extra Scenery lost");
    need(value.state.groups[0].key=="group:action:member"&&value.handles.at("group:action:member").uid==group_uid,"full saved/current Group identity lost");
    auto bad=[&](auto change){m=setup();change(m);need(!read(),"invalid actual Group reader accepted");};
    bad([](auto& x){x.put(group+0x18,uint64_t(7));});
    bad([](auto& x){x.put(group+0x170,root+1);});
    bad([](auto& x){x.put(group+0x178,region+1);});
    bad([](auto& x){x.put(group+0x180,group);});
    bad([](auto& x){x.put(group+0x220,int32_t(0));});
    bad([](auto& x){x.put(scenery+0x180,uintptr_t(0));});
    bad([](auto& x){x.put(scenery+0x18,uint64_t(7));});
    bad([](auto& x){x.put(member+0x64,uint32_t(0x7fc00000));});
    bad([](auto& x){array(x,group+0x210,0x16ea660,{member,member});});
    bad([](auto& x){array(x,manager+0x18,0x16f0060,{group,group});});
    bad([](auto& x){x.put(free_prop+0x180,group);});
    bad([](auto& x){x.put(selection+0xd1,uint8_t(1));});
    bad([](auto& x){x.put(selection+0xd3,uint8_t(1));});
    bad([](auto& x){x.put(manager+0x2c,int32_t(0));});
    m=setup();array(m,manager+0x18,0x16f0060,{});need(gr::read(m,image,root,selection,{},bind,owner,find,value,e)&&value.state.groups.empty(),"empty GroupManager rejected");
    m=setup();const uintptr_t multi=0x90000;m.put(selection+0xc0,multi);m.put(multi,image+0x171b260);m.put(image+0x171b260+0x168,image+0x74bc00);m.put(multi+0x45f,uint8_t(1));need(gr::ui(m,image,selection,0,e),e);
    m.put(multi+0x45f,uint8_t(0));need(!gr::ui(m,image,selection,0,e),"hidden Group UI accepted");
    const uintptr_t handler=0xa0000,window=0xb0000;m.put(selection+0x80,group);m.put(selection+0xb8,handler);m.put(handler+0x10,window);m.put(window,image+0x1719fd0);m.put(window+0x4e8,group);m.put(window+0x45f,uint8_t(1));m.put(image+0x1719fd0+0x168,image+0x718c80);need(gr::ui(m,image,selection,group,e),e);
    m.put(window+0x4e8,free_prop);need(!gr::ui(m,image,selection,group,e),"foreign Group UI target accepted");
    namespace identity=mt2mp::game::group_current_identity;
    identity::Registry canonical;const identity::Scope scope{"current-snapshot",3,4,root};
    auto stable=[&](uintptr_t,uint64_t uid,const std::string& derived,std::string& key,std::string& error){
        return canonical.bind(scope,uid,derived,key,error);};
    m=setup();need(gr::read(m,image,root,selection,{free_prop},bind,owner,find,stable,value,e),e);
    const auto original_key=value.state.groups[0].key;
    array(m,group+0x210,0x16ea660,{});m.put(scenery+0x180,uintptr_t(0));
    need(gr::read(m,image,root,selection,{free_prop},bind,owner,find,stable,value,e)&&value.state.groups.size()==1&&
        value.state.groups[0].key==original_key&&value.state.groups[0].members.empty(),"known native empty Group lost its canonical full UID");
    std::string actual;need(canonical.bind(scope,group_uid,"group:action:new-first",actual,e)&&actual==original_key,
        "current first member changed retained Group identity");
    need(!canonical.birth(scope,group_uid,"group:action:new-first",e),"Group birth replaced another retained full UID binding");
    need(!canonical.bind(scope,group_uid+1,original_key,actual,e),"two Group UIDs claimed the same canonical key");
    const auto next=identity::Scope{"next-snapshot",3,5,root};
    need(!canonical.bind(next,group_uid,"",actual,e),"unknown empty Group reused stale snapshot identity");
    need(canonical.birth(next,group_uid,"group:action:request",e)&&canonical.bind(next,group_uid,"",actual,e)&&actual=="group:action:request",
        "new Group birth did not retain its request identity");
}
}
int main()try{f::run();std::cout<<f::checks<<" actual Group full UID/manager/member/Selection reader cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
