#include "typed_action/action_session.h"
#include "boss_phase_reader.h"
#include "boss_phase_property.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace boss_phase_reader=mt2mp::game::boss_phase_reader;
using json=nlohmann::json;
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
uint32_t GetCurrentThreadId(){return 17;}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
namespace host {
struct Origin {uint64_t world_epoch=3,generation=4,state=0xc0000;bool operator==(const Origin&)const=default;};
struct WorldState {Origin origin;};
bool same_world(const Origin& a,const Origin& b){return a==b;}
}
namespace witness {
struct UndoWitness {uintptr_t active_graph=0;std::vector<uintptr_t> undo{0x9999};bool operator==(const UndoWitness&)const=default;};
UndoWitness actual_undo;
bool read_undo(int,uintptr_t,UndoWitness& out,std::string&){out=actual_undo;return true;}
}
struct Memory {
    int process=0;
    bool read(uintptr_t p,void* out,size_t n,std::string& e){auto found=fields.find(p);if(found==fields.end()||found->second.size()!=n){e="fixture absent exact field address="+std::to_string(p)+" size="+std::to_string(n);return false;}std::memcpy(out,found->second.data(),n);return true;}
    std::map<uintptr_t,std::vector<uint8_t>> fields;std::map<uintptr_t,std::string> strings;
    template<class T>void put(uintptr_t address,const T& value){std::vector<uint8_t> bytes(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);fields[address]=std::move(bytes);}
    template<class T>bool operator()(uintptr_t base,size_t offset,T& value,std::string& e){auto i=fields.find(base+offset);if(i==fields.end()||i->second.size()!=sizeof value){e="fixture missing exact field";return false;}std::memcpy(&value,i->second.data(),sizeof value);return true;}
    void array(uintptr_t object,size_t offset,uintptr_t data,int32_t count){put(object+offset+8,data);put(object+offset+16,count);put(object+offset+20,count+2);}
};
Memory fixture_memory(){Memory m;const uintptr_t image=0x140000000,object=0x10000;
    m.put(object,image+0x1705990);m.put(object+0x18,uint64_t(0x100000042));m.put(object+0x2c,int32_t(3));m.put(object+0x30,int32_t(0));
    m.put(object+0x4a0,uintptr_t(0));m.put(object+0x490,uint8_t(0));m.put(object+0x491,uint8_t(0));m.put(object+0x492,uint8_t(1));m.put(object+0x2e0,int32_t(2));
    m.strings[object+0xb0]="Boss";m.strings[object+0x248]="definition";m.array(object,0x128,0x20000,2);m.array(object,0x158,0x30000,2);
    for(int32_t i=0;i<2;++i){const auto ability=uintptr_t(0x40000+i*0x1000),phase=uintptr_t(0x60000+i*0x1000),data=uintptr_t(0x80000+i*0x1000),element=uintptr_t(0xa0000+i*0x1000);
        m.put(0x20000+size_t(i)*8,ability);m.put(ability,image+0x171c930);m.strings[ability+0x48]="Attack";m.put(0x30000+size_t(i)*8,phase);m.put(phase,image+0x16ddb00);
        m.put(phase+0x5c,i);m.put(phase+0x58,int32_t(20+i));m.put(phase+0x80,std::array<float,4>{1,0,0,1});m.strings[phase+0x38]=i?"Second":"First";m.strings[phase+0x60]="fire";
        m.array(phase,0x90,data,1);m.put(data,element);m.put(element,image+0x173a3a0);m.put(element+0x18,ability);
    }return m;
}

struct Runtime {uintptr_t image=0x140000000;uint32_t thread=17;bool ready=true;Memory memory;std::optional<int> pending;};
Runtime fixture_current;std::atomic<Runtime*> published{&fixture_current};
host::WorldState actual_world;action::Ledger actual_money;action::Binding actual_binding{3,4};
std::string fixture_snapshot="22222222-2222-4222-8222-222222222222";
json fixture_controller;
bool world(Runtime&,host::WorldState& out,std::string&){out=actual_world;return true;}
bool ledger(action::Ledger& out,std::string&){out=actual_money;return true;}
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string& id,action::Binding& out,std::string& e){
    if(id!=fixture_snapshot||fixture_controller.value("pending",true)||fixture_controller.value("locked",true)){e="public admission requires idle Controller";return false;}out=actual_binding;return true;
}
}
namespace prop_text {
bool pending(){return false;}
bool text(Runtime& r,uintptr_t p,std::string& value,std::string& e){auto found=r.memory.strings.find(p);if(found==r.memory.strings.end()){e="fixture missing native string";return false;}value=found->second;return true;}
}
struct Sample {
    host::WorldState world;uintptr_t manager=0,data=0;int32_t capacity=0;
    std::vector<boss_phase_reader::LocalType> local;
    action::BossPhaseState value;action::Ledger money;witness::UndoWitness undo;
    std::array<uint64_t,4> random{};
};
struct Pending {action::Binding binding;std::string fixture_snapshot;action::BossPhaseIntent intent;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
bool installed=true;std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0;
uint32_t named_window(Runtime&,uintptr_t& window){window=0x120000;return 0;}
#include "boss_phase_list_actual.inc"
#include "boss_phase_sample_actual.inc"
#include "boss_phase_property_capture_actual.inc"
#include "boss_phase_create_capture_actual.inc"
uint32_t refresh(Runtime&,uintptr_t,uintptr_t){return 0;}

unsigned calls=0;int corruption=0;uint32_t exception_code=0;
uint32_t invoke(const boss_phase_reader::LocalType& owner,const action::BossPhaseIntent& intent,bool& returned){
    ++calls;
    if(intent.kind==action::BossPhaseKind::Create||intent.kind==action::BossPhaseKind::Destroy){
        if(exception_code)return exception_code;auto& m=fixture_current.memory;
        auto phases=owner.phases;auto data=owner.phase_data;auto capacity=owner.phase_capacity;
        if(intent.kind==action::BossPhaseKind::Create){
            const uintptr_t phase=0x300000;boss_phase_reader::LocalPhase added;added.pointer=phase;added.data=0x310000;added.capacity=4;phases.push_back(added);
            if(phases.size()>size_t(capacity)){data+=0x400000;capacity=std::max(4,capacity*2);m.put(owner.object+0x160,data);m.put(owner.object+0x16c,capacity);}
            m.put(phase,fixture_current.image+0x16ddb00);m.put(phase+0x58,int32_t(100));m.put(phase+0x80,std::array<float,4>{1,0,0,1});
            m.strings[phase+0x38]=intent.text;m.strings[phase+0x60]="";m.put(phase+0x98,added.data);m.put(phase+0xa0,int32_t(0));m.put(phase+0xa4,int32_t(4));
        }else phases.erase(phases.begin()+intent.phase);
        m.put(owner.object+0x168,int32_t(phases.size()));for(size_t i=0;i<phases.size();++i){m.put(data+i*8,phases[i].pointer);m.put(phases[i].pointer+0x5c,int32_t(i));}
        returned=true;return 0;
    }
    if(intent.kind!=action::BossPhaseKind::Reorder){
        struct Calls {
            uint32_t event(uintptr_t,uintptr_t,bool){return exception_code;}
            uint32_t add(uintptr_t,uintptr_t p,uintptr_t prototype){
                if(exception_code)return exception_code;auto& m=fixture_current.memory;std::string error;uintptr_t data=0;int32_t count=0,capacity=0;
                need(m(p,0x98,data,error)&&m(p,0xa0,count,error)&&m(p,0xa4,capacity,error),error);
                if(count==capacity){const auto next=data+0x100000;for(int32_t i=0;i<count;++i){uintptr_t old=0;need(m(data,size_t(i)*8,old,error),error);m.put(next+size_t(i)*8,old);}data=next;capacity=std::max(4,capacity*2);m.put(p+0x98,data);m.put(p+0xa4,capacity);}
                const uintptr_t element=0x200000+size_t(count)*0x100;m.put(element,fixture_current.image+0x173a3a0);m.put(element+0x18,prototype);m.put(data+size_t(count)*8,element);m.put(p+0xa0,count+1);return 0;
            }
            uint32_t remove(uintptr_t,uintptr_t p,int32_t index){
                auto& m=fixture_current.memory;std::string error;uintptr_t data=0;int32_t count=0;need(m(p,0x98,data,error)&&m(p,0xa0,count,error),error);
                for(int32_t i=index;i+1<count;++i){uintptr_t next=0;need(m(data,size_t(i+1)*8,next,error),error);m.put(data+size_t(i)*8,next);}m.put(p+0xa0,count-1);return 0;
            }
            uint32_t text(uintptr_t,uintptr_t p,const std::string& value){fixture_current.memory.strings[p]=value;return 0;}
            uint32_t bytes(uintptr_t p,const void* value,size_t n){const auto* data=static_cast<const uint8_t*>(value);fixture_current.memory.fields[p]=std::vector<uint8_t>(data,data+n);return 0;}
        } native;
        if(intent.kind==action::BossPhaseKind::AddAbility)return mt2mp::game::boss_phase_property::add(native,fixture_current.image,owner.phases[size_t(intent.phase)].pointer,owner.abilities,intent,returned);
        return mt2mp::game::boss_phase_property::apply(native,fixture_current.image,owner.phases[size_t(intent.phase)].pointer,intent,returned);
    }
    auto phases=owner.phases;auto phase=phases[size_t(intent.phase)];phases.erase(phases.begin()+intent.phase);
    phases.insert(phases.begin()+(intent.boundary>intent.phase?intent.boundary-1:intent.boundary),phase);
    for(size_t i=0;i<phases.size();++i){fixture_current.memory.put(owner.phase_data+i*8,phases[i].pointer);fixture_current.memory.put(phases[i].pointer+0x5c,int32_t(i));}
    if(corruption==1)++actual_money.cash;
    if(corruption==2)fixture_current.memory.put(0x10018,uint64_t(0x200000042));
    if(corruption==3)fixture_current.memory.put(0xa0018,uintptr_t(0x41000));
    if(corruption==4)witness::actual_undo.undo.push_back(42);
    returned=exception_code==0;return exception_code;
}
#include "boss_phase_perform_actual.inc"
action::Request request(){action::Request r;r.action=action::Action::ReorderBossPhase;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.snapshot_uuid=fixture_snapshot;r.canonical_record_key="character:3:0";r.boss_phase={0,0,2};return r;}
void reset(){fixture_current=Runtime{};fixture_current.memory=fixture_memory();auto& m=fixture_current.memory;const auto image=fixture_current.image;
    m.put(image+0x13df7a0,uintptr_t(0xd0000));m.put(0xc0168,uintptr_t(0xd0000));m.put(0xd0000,image+0x1732430);m.array(0xd0000,0x170,0xe0000,1);m.put(0xe0000,uintptr_t(0x10000));
    m.put(image+0x164b250,uintptr_t(0xf0000));m.put(0xf0000,std::array<uint64_t,4>{1,2,3,4});
    m.put(image+0x13df4f0,uintptr_t(0x100000));m.put(0x100000,image+0x1714ed0);m.put(0x100038,uintptr_t(0x110000));m.put(0x110000,image+0x16c12b0);m.put(0x110120,int32_t(7));m.put(image+0x164c5c0,uintptr_t(0x900000));m.put(0x900000,std::array<float,4>{1,0,0,1});
    m.put(0x120000,image+0x173a7f0);m.put(0x1204e8,uintptr_t(0x130000));m.put(0x130000,image+0x1736770);m.put(0x130460,uintptr_t(0x10000));
    m.put(0x1303c8,uintptr_t(0x140000));m.put(0x140000,image+0x1712800);m.put(0x140318,uintptr_t(0x10000));m.put(0x140320,uintptr_t(0x150000));
    m.put(0x150000,image+0x1721a00);m.put(0x150318,uintptr_t(0x10000));m.put(0x150350,uintptr_t(0x160000));m.put(0x160348,uintptr_t(0x60000));m.put(0x150358,int32_t(2));m.put(0x150348,int32_t(0));
    m.put(0x160000,image+0x1721750);m.put(0x160340,uintptr_t(0x10000));m.put(0x160350,uintptr_t(0x150000));m.put(0x160280,uintptr_t(0x120000));
    actual_world={};actual_money={};witness::actual_undo={};actual_binding={3,4};Config::instance().is_host=true;
    fixture_controller={{"started",true},{"pending",false},{"locked",false},{"action_uuid",request().action_uuid}};
    calls=0;corruption=0;exception_code=0;operations.clear();captured.reset();captures=0;
}
int main(){try{unsigned cases=0;std::string e;const auto r=request();
    reset();need(capture(fixture_current,0x10000,0x60000,2,e),e);need(captured&&captures==1&&calls==0,"Capture must suppress native mutation");++cases;
    reset();fixture_current.memory.put(0x150358,int32_t(1));need(capture(fixture_current,0x10000,0x60000,1,e)&&!captured,"No-op drag must not queue");++cases;
    for(int fault=0;fault<7;++fault){reset();if(fault==0)fixture_controller["pending"]=true;if(fault==1)actual_world.origin.generation=5;if(fault==2)fixture_current.memory.put(0xa0018,uintptr_t(0x12345));if(fault==3)witness::actual_undo.active_graph=42;if(fault==4)fixture_current.memory.put(0xc0168,uintptr_t(0x12345));if(fault==5)fixture_current.memory.put(0x150318,uintptr_t(0x12345));if(fault==6)fixture_current.memory.put(0x160348,uintptr_t(0x61000));
        e.clear();need(!capture(fixture_current,0x10000,0x60000,2,e)&&!captured&&calls==0&&!e.empty(),"Capture admission/membership guard missing");++cases;}
    reset();fixture_controller["pending"]=true;need(perform(actual_binding,r,nullptr,e)==action::Outcome::Confirmed,e);need(calls==1,"Host original once");
    action::Evidence authority;need(observe(actual_binding,r,authority,e),e);need(retire(actual_binding,r,e),e);need(observe(actual_binding,r,authority,e)&&authority.retirement_confirmed,e);++cases;
    need(perform(actual_binding,r,nullptr,e)==action::Outcome::Unknown&&calls==1,"No replay after attempted operation");++cases;
    reset();fixture_controller["pending"]=true;Config::instance().is_host=false;need(perform(actual_binding,r,&authority,e)==action::Outcome::Confirmed,e);need(calls==1,"Client original once while Controller pending");++cases;
    reset();fixture_controller["pending"]=true;need(perform(actual_binding,r,&authority,e)==action::Outcome::RejectedNoEffect&&calls==0,"Wrong role accepted");++cases;
    reset();fixture_controller["pending"]=true;fixture_controller["action_uuid"]="wrong";need(perform(actual_binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&calls==0,"Wrong private action accepted");++cases;
    for(int fault=1;fault<=4;++fault){reset();fixture_controller["pending"]=true;corruption=fault;need(perform(actual_binding,r,nullptr,e)==action::Outcome::Unknown&&calls==1,"Partial pointer/UID/Ability/Undo/Ledger effect accepted");++cases;}
    reset();fixture_controller["pending"]=true;exception_code=5;need(perform(actual_binding,r,nullptr,e)==action::Outcome::Unknown&&calls==1,"Original exception lost");++cases;
    for(auto kind:{action::BossPhaseKind::Name,action::BossPhaseKind::Health,action::BossPhaseKind::Vfx,action::BossPhaseKind::Color}){
        auto property=r;property.boss_phase={0,0,-1};auto& intent=property.boss_phase;intent.kind=kind;
        if(kind==action::BossPhaseKind::Name)intent.text="Renamed phase";
        if(kind==action::BossPhaseKind::Vfx)intent.text="ice";
        if(kind==action::BossPhaseKind::Health)intent.health=35;
        if(kind==action::BossPhaseKind::Color)intent.color={0,1,1,0.5f};
        reset();need(property_capture(fixture_current,0x160000,intent,e),e);need(captured&&captured->intent==intent&&calls==0,"Property original UI capture failed");++cases;
        reset();fixture_controller["pending"]=true;need(perform(actual_binding,property,nullptr,e)==action::Outcome::Confirmed,e);
        action::Evidence required;need(observe(actual_binding,property,required,e)&&calls==1,e);need(retire(actual_binding,property,e),e);++cases;
        need(perform(actual_binding,property,nullptr,e)==action::Outcome::Unknown&&calls==1,"Property operation replayed");++cases;
        reset();fixture_controller["pending"]=true;Config::instance().is_host=false;need(perform(actual_binding,property,&required,e)==action::Outcome::Confirmed,e);++cases;
        reset();fixture_current.memory.put(0x160350,uintptr_t(0x12345));need(!property_capture(fixture_current,0x160000,intent,e)&&!captured&&calls==0,"Property wrong Item parent accepted");++cases;
        reset();fixture_controller["pending"]=true;exception_code=0xc0000005;need(perform(actual_binding,property,nullptr,e)==action::Outcome::Unknown&&calls==1,"Property original failure lost");++cases;
    }
    struct OrderedCalls {
        std::vector<int> order;int failure=0;uintptr_t destination=0;size_t size=0;
        uint32_t remove(uintptr_t,uintptr_t p,int32_t index){destination=p;size=size_t(index);order.push_back(2);return failure==2?42:0;}
        uint32_t event(uintptr_t,uintptr_t,bool post){const int step=post?3:1;order.push_back(step);return failure==step?42:0;}
        uint32_t text(uintptr_t,uintptr_t p,const std::string& value){destination=p;size=value.size();order.push_back(2);return failure==2?42:0;}
        uint32_t bytes(uintptr_t p,const void*,size_t n){destination=p;size=n;order.push_back(2);return failure==2?42:0;}
    };
    for(int failure=0;failure<=3;++failure){OrderedCalls native;native.failure=failure;action::BossPhaseIntent intent;intent.kind=action::BossPhaseKind::Color;intent.color={1,2,3,4};bool returned=false;
        const auto code=mt2mp::game::boss_phase_property::apply(native,0,0x60000,intent,returned);
        const size_t expected=failure?size_t(failure):3;need(native.order.size()==expected&&returned==(failure==0)&&code==(failure?42u:0u),"Original pre/write/post failure ordering differs");
        if(failure!=1)need(native.destination==0x60080&&native.size==16,"Original phase color field differs");++cases;
    }
    for(auto kind:{action::BossPhaseKind::RemoveAbility,action::BossPhaseKind::AddAbility}){
        auto member=r;member.boss_phase={0,0,-1};auto& intent=member.boss_phase;intent.kind=kind;
        if(kind==action::BossPhaseKind::RemoveAbility)intent.element=0;else{intent.text="Attack";intent.ability_indices={0,1};}
        reset();need(property_capture(fixture_current,0x160000,intent,e)&&captured&&captured->intent==intent&&calls==0,e);++cases;
        reset();fixture_controller["pending"]=true;need(perform(actual_binding,member,nullptr,e)==action::Outcome::Confirmed,e);
        action::Evidence required;need(observe(actual_binding,member,required,e)&&calls==1,e);need(retire(actual_binding,member,e),e);++cases;
        reset();fixture_controller["pending"]=true;Config::instance().is_host=false;need(perform(actual_binding,member,&required,e)==action::Outcome::Confirmed,e);++cases;
        reset();fixture_controller["pending"]=true;
        if(kind==action::BossPhaseKind::RemoveAbility)intent.element=1;else intent.ability_indices={0};
        need(perform(actual_binding,member,nullptr,e)==action::Outcome::RejectedNoEffect&&calls==0,"Invalid current membership passed preflight");++cases;
    }
    reset();action::BossPhaseIntent add;add.character_id=0;add.phase=0;add.kind=action::BossPhaseKind::AddAbility;add.text="Absent";
    need(property_capture(fixture_current,0x160000,add,e)&&!captured&&calls==0,"Unmatched original menu must have no effect");++cases;
    reset();fixture_current.memory.put(0x600a4,int32_t(1));auto member=r;member.boss_phase={0,0,-1};member.boss_phase.kind=action::BossPhaseKind::AddAbility;
    member.boss_phase.text="Attack";member.boss_phase.ability_indices={0,1};fixture_controller["pending"]=true;
    need(perform(actual_binding,member,nullptr,e)==action::Outcome::Confirmed,e);++cases;
    for(auto kind:{action::BossPhaseKind::Create,action::BossPhaseKind::Destroy}){
        auto phase=r;phase.boss_phase={0,kind==action::BossPhaseKind::Create?2:0,-1};auto& intent=phase.boss_phase;intent.kind=kind;
        if(kind==action::BossPhaseKind::Create)intent.text="{design_boss_defaultphasename} 3";
        reset();need(kind==action::BossPhaseKind::Create?create_capture(fixture_current,0x150000,e):property_capture(fixture_current,0x160000,intent,e),e);
        need(captured&&captured->intent==intent&&calls==0,"Phase membership capture mutated state");++cases;
        reset();fixture_controller["pending"]=true;need(perform(actual_binding,phase,nullptr,e)==action::Outcome::Confirmed,e);
        action::Evidence required;need(observe(actual_binding,phase,required,e)&&calls==1,e);need(retire(actual_binding,phase,e),e);++cases;
        reset();fixture_controller["pending"]=true;Config::instance().is_host=false;need(perform(actual_binding,phase,&required,e)==action::Outcome::Confirmed,e);++cases;
        reset();fixture_controller["pending"]=true;intent.phase=7;need(perform(actual_binding,phase,nullptr,e)==action::Outcome::RejectedNoEffect&&calls==0,"Stale phase membership accepted");++cases;
        reset();fixture_current.memory.put(0x150348,int32_t(1));need(!create_capture(fixture_current,0x150000,e)&&!captured&&calls==0,"Queued original UI removal accepted");++cases;
    }
    reset();fixture_current.memory.put(0x1016c,int32_t(2));auto create=r;create.boss_phase={0,2,-1};create.boss_phase.kind=action::BossPhaseKind::Create;create.boss_phase.text="{design_boss_defaultphasename} 3";
    fixture_controller["pending"]=true;need(perform(actual_binding,create,nullptr,e)==action::Outcome::Confirmed,e);++cases;
    std::cout<<cases<<" actual Boss sample/capture/perform/ACK cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
