#include "npc_quest_npc_destination.h"
#include "typed_action/npc_quest_action.h"
#include "typed_action/action_session.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace fixture {
unsigned cases=0;
void need(bool ok,const std::string& cause){++cases;if(!ok)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> values;
    uintptr_t failed=0;
    template<class T>void set(uintptr_t p,T value){auto& bytes=values[p];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);}
    bool copy(uintptr_t p,void* out,size_t size,std::string& e){
        if(p==failed){e="retained exact native read failure";return false;}
        const auto found=values.find(p);
        if(found==values.end()||found->second.size()!=size){e="fixture exact field absent size="+std::to_string(size);return false;}
        std::memcpy(out,found->second.data(),size);return true;
    }
};
constexpr uintptr_t image=0x10000000,object=0x10000,region=0x20000,proxy=0x30000;
const std::string key=std::string(64,'a')+":world:npc:36";
Memory ordinary(){Memory m;m.set(object,image+0x1742980);m.set(object+0x18,uint64_t(700));
    m.set(object+0x178,region);m.set(object+0x28,proxy);m.set(proxy,object+0x20);return m;}
}
namespace actual {
namespace a=mt2mp::action_session;
using fixture::Memory;
struct State {uintptr_t image=fixture::image;} state;
template<class T>bool field(Memory& m,uintptr_t p,size_t offset,T& out,std::string& e){
    return mt2mp::game::npc_quest_npc_destination::at(m,p,offset,out,e);
}
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
struct NativeCalls {unsigned defaults=0,bindings=0;int32_t selected_action=7;bool fail_binding=false,wrong_pointer=false;} calls;
bool direct_destination(uintptr_t,std::string& key,std::string&){key="action:11111111-1111-4111-8111-111111111111";return true;}
bool npc_destination(uintptr_t,std::string& key,std::string& e){
    ++calls.bindings;if(calls.fail_binding)return fail(e,"retained NPC canonical binding failure");key=fixture::key;return true;
}
namespace npc_quest_target_runtime {
bool bind_destination(int32_t,uintptr_t,std::string& key,std::string&){key="action:22222222-2222-4222-8222-222222222222";return true;}
}
bool destination(Memory&,const a::Binding&,const std::string&,int32_t,a::QuestDestinationFamily,const std::string&,
                 uintptr_t& pointer,uint64_t& uid,std::string&){pointer=fixture::object+unsigned(calls.wrong_pointer);uid=700;return true;}
uint32_t default_action(uintptr_t,int32_t& action){++calls.defaults;action=calls.selected_action;return 0;}
#include "quest_target_intent_body.inc"
}
int main()try{
    using fixture::need;namespace npc=mt2mp::game::npc_quest_npc_destination;
    std::string e;npc::Witness result;
    auto m=fixture::ordinary();
    auto resolve=[](uintptr_t p,uint64_t uid,uintptr_t region,std::string& key,std::string& cause){
        if(p!=fixture::object||uid!=700||region!=fixture::region){cause="current owner differs";return false;}
        key=fixture::key;return true;
    };
    need(npc::bind(m,fixture::image,fixture::object,resolve,result,e)&&result.key==fixture::key&&result.uid==700,
         "actual NPC weak/current-binding helper rejected owned NPC: "+e);
    for(unsigned which=0;which<5;++which){m=fixture::ordinary();
        if(which==0)m.set(fixture::object,fixture::image+0x16c0770);
        if(which==1)m.set(fixture::object+0x18,uint64_t(0));
        if(which==2)m.set(fixture::object+0x178,uintptr_t(0));
        if(which==3)m.set(fixture::proxy,fixture::object+0x18);
        if(which==4)m.set(fixture::object+0x28,uintptr_t(0));
        need(!npc::bind(m,fixture::image,fixture::object,resolve,result,e)&&result==npc::Witness{},
             "foreign class/UID/Region/weak pair escaped NPC admission");
    }
    m=fixture::ordinary();m.failed=fixture::object+0x18;
    need(!npc::bind(m,fixture::image,fixture::object,resolve,result,e)&&e=="retained exact native read failure",
         "native field cause was replaced");
    m=fixture::ordinary();
    auto fail=[](uintptr_t,uint64_t,uintptr_t,std::string&,std::string& cause){cause="actual current UID missing";return false;};
    need(!npc::bind(m,fixture::image,fixture::object,fail,result,e)&&e=="actual current UID missing",
         "canonical current-owner failure was replaced");
    auto raw=[](uintptr_t,uint64_t,uintptr_t,std::string& key,std::string&){key="local:123";return true;};
    need(!npc::bind(m,fixture::image,fixture::object,raw,result,e),"local pointer label entered shared key");
    for(unsigned which=0;which<3;++which){m=fixture::ordinary();
        auto drift=[&](uintptr_t,uint64_t,uintptr_t,std::string& key,std::string&){key=fixture::key;
            if(which==0)m.set(fixture::object+0x18,uint64_t(701));
            if(which==1)m.set(fixture::object+0x178,fixture::region+8);
            if(which==2)m.set(fixture::object+0x28,fixture::proxy+8);
            return true;
        };
        need(!npc::bind(m,fixture::image,fixture::object,drift,result,e)&&result==npc::Witness{},
             "post-resolver identity/weak lifetime drift accepted");
    }
    using namespace actual;
    for(auto family:{a::QuestDestinationFamily::Building,a::QuestDestinationFamily::Zone,
                     a::QuestDestinationFamily::DirectMonster,a::QuestDestinationFamily::NPC}){
        calls={};m=fixture::ordinary();a::NpcQuestIntent intent;intent.before.monsters_to_kill=3;
        uintptr_t ap=state.image+(family==a::QuestDestinationFamily::NPC?0x1742980:
            family==a::QuestDestinationFamily::Zone?0x1745ce0:family==a::QuestDestinationFamily::DirectMonster?0x16c0770:0x17183c0);
        m.set(fixture::object,ap);m.set(fixture::region+0x834,int32_t(67));
        m.set(ap+0x2d0,state.image+(family==a::QuestDestinationFamily::Zone?0xe3f5c0:0xdf1670));
        m.set(fixture::object+0x24c,int32_t(1));m.set(fixture::object+0x560,int32_t(4));
        calls.selected_action=family==a::QuestDestinationFamily::NPC?7:family==a::QuestDestinationFamily::Zone?1:
            family==a::QuestDestinationFamily::DirectMonster?6:2;
        need(target_intent(m,{3,4},"snapshot",fixture::object,intent,e)&&intent.family==family&&
             intent.action==calls.selected_action&&intent.kills==(family==a::QuestDestinationFamily::Zone?3:
             family==a::QuestDestinationFamily::DirectMonster?1:0)&&calls.defaults==1,
             "actual native target derivation changed an existing family or rejected NPC: "+e);
    }
    for(unsigned which=0;which<3;++which){calls={};m=fixture::ordinary();a::NpcQuestIntent intent;
        m.set(fixture::region+0x834,int32_t(67));m.set(state.image+0x1742980+0x2d0,state.image+0xdf1670);
        if(which==0)calls.fail_binding=true;
        if(which==1)calls.wrong_pointer=true;
        if(which==2)m.set(state.image+0x1742980+0x2d0,state.image+0xe3f5c0);
        need(!target_intent(m,{3,4},"snapshot",fixture::object,intent,e),"NPC derivation accepted foreign binding/vtable");
        if(which==0)need(e=="retained NPC canonical binding failure","derivation lost original binding cause");
    }
    std::cout<<fixture::cases<<" actual NPC destination weak/canonical guard and four-family target derivation checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
