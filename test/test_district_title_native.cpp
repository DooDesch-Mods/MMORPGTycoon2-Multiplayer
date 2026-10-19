#define main district_text_fixture_main
#include "test_district_native.cpp"
#undef main
#include "district_title_reader.h"
namespace district_title_reader=mt2mp::game::district_title_reader;
namespace district_text {using ::sample;bool pending(){return bool(::captured);}}
namespace title_fixture {
bool installed=true;
constexpr uintptr_t cursor=0x910000,fixture_behaviour=0x920000,proxy=0x930000;
struct Pending {action::Binding binding;std::string snapshot,key;action::DistrictTitleIntent intent;uintptr_t behaviour=0;
    district_saved_identity::Current target;action::DistrictState before;action::Ledger money;witness::UndoWitness undo;};
struct Operation {
    action::Binding binding;action::Request request;district_saved_identity::Current target;
    action::Evidence evidence;witness::UndoWitness undo;
    bool attempted=false,returned=false,confirmed=false,retired=false;uint32_t original_steps=0;std::string cause;
};
std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0;
bool fixture_point_inside=true;unsigned title_originals=0,presentations=0;int failure=0;
bool inside(Runtime&,uintptr_t object,const action::DistrictTitleIntent&,std::string& e){
    need(object==object_ptr,"Contains shape owner changed");if(!fixture_point_inside)e="original Contains rejected";return fixture_point_inside;
}
#include "district_title_capture_actual.inc"
unsigned restores=0;bool restore_refuse=false;
uint32_t restore_flag_call(uintptr_t address,bool& exchanged){
    ++restores;need(address==object_ptr+0x350,"restore wrong scalar");uint8_t actual=9;std::string e;
    need(fixture_runtime.memory.read(address,&actual,1,e),e);exchanged=!restore_refuse&&actual==0;
    if(exchanged)fixture_runtime.memory.put(address,uint8_t(1));return 0;
}
#include "district_title_restore_actual.inc"
uint32_t invoke(uintptr_t base,uintptr_t object,const action::DistrictTitleIntent& intent,uint32_t& steps){
    need(base==image&&object==object_ptr,"title original receiver changed");++title_originals;
    if(failure==1)return 0xc0000005;
    for(size_t i=0;i<3;++i)fixture_runtime.memory.put(object+0x344+i*4,intent.position[i]);
    fixture_runtime.memory.put(object+0x350,uint8_t(1));steps|=1;++presentations;
    fixture_inside=true;
    if(failure==2)return 0xc0000005;
    if(failure==3)fixture_money.line_counts[0]++;
    if(failure==4)fixture_runtime.memory.put(object+0x328,uint32_t(1));
    if(failure==5)witness::fixture_undo.undo.push_back(0x1234);
    steps|=2;return 0;
}
#include "district_title_perform_actual.inc"
void setup(){
    ::reset();captured.reset();operations.clear();captures=0;title_originals=presentations=0;failure=0;installed=true;fixture_point_inside=true;
    restores=0;restore_refuse=false;
    auto& m=fixture_runtime.memory;m.put(image+0x13df9d0,cursor);m.put(cursor,image+0x1786dd0);m.put(cursor+0x190,fixture_behaviour);
    m.put(fixture_behaviour,image+0x1741aa0);m.put(fixture_behaviour+0x150,object_ptr);m.put(fixture_behaviour+0x158,proxy);m.put(proxy,object_ptr+0x20);m.put(fixture_behaviour+0x16c,uint8_t(1));
    m.put(fixture_behaviour+0x160,std::array<uint32_t,3>{0x3f800000,0x40000000,0x40400000});
}
action::Request title_request(){auto r=::request();r.action=action::Action::MoveDistrictTitle;r.district_text={};r.district_title.position={0x3f800000,0x40000000,0x40400000};return r;}
void run(){
    setup();std::string e;fixture_controller["pending"]=false;
    need(capture(fixture_runtime,object_ptr,fixture_behaviour+0x160,e)&&captured&&captures==1&&title_originals==0,e);
    need(!district_title_reader::cleaned(fixture_runtime.memory,image,fixture_behaviour,e),"live title tool reported cleanup");
    fixture_runtime.memory.put(cursor+0x190,uintptr_t(0));fixture_runtime.memory.put(fixture_behaviour+0x150,uintptr_t(0));fixture_runtime.memory.put(fixture_behaviour+0x158,uintptr_t(0));
    need(district_title_reader::cleaned(fixture_runtime.memory,image,fixture_behaviour,e),e);
    need(restore_source(fixture_runtime,*captured,e)&&restores==0,e);
    setup();fixture_controller["pending"]=false;fixture_inside=false;fixture_runtime.memory.put(object_ptr+0x350,uint8_t(1));
    need(capture(fixture_runtime,object_ptr,fixture_behaviour+0x160,e),e);
    fixture_runtime.memory.put(object_ptr+0x350,uint8_t(0));need(restore_source(fixture_runtime,*captured,e)&&restores==1,e);
    need(fixture_runtime.memory.bytes.at(object_ptr+0x350)==1,"old manual flag not restored");
    fixture_runtime.memory.put(object_ptr+0x350,uint8_t(0));restore_refuse=true;need(!restore_source(fixture_runtime,*captured,e),"refused source CAS accepted");
    restore_refuse=false;fixture_runtime.memory.put(object_ptr+0x328,uint32_t(1));need(!restore_source(fixture_runtime,*captured,e)&&restores==2,"unrelated cleanup change entered CAS");
    auto bad_capture=[&](auto edit){setup();fixture_controller["pending"]=false;edit();need(!capture(fixture_runtime,object_ptr,fixture_behaviour+0x160,e)&&!captured&&title_originals==0,"invalid original title source accepted");};
    bad_capture([]{fixture_runtime.memory.put(fixture_behaviour+0x16c,uint8_t(0));});
    bad_capture([]{fixture_runtime.memory.put(proxy,object_ptr+0x28);});
    bad_capture([]{fixture_runtime.memory.put(fixture_behaviour+0x150,object_ptr+8);});
    bad_capture([]{fixture_runtime.memory.put(fixture_behaviour,image+0x1741aa1);});
    bad_capture([]{fixture_runtime.memory.put(fixture_behaviour+0x160,uint32_t(0x7f800000));});
    bad_capture([]{fixture_point_inside=false;});
    setup();fixture_controller["pending"]=false;need(!capture(fixture_runtime,object_ptr,fixture_behaviour+0x164,e),"foreign title point accepted");
    setup();auto r=title_request();need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);
    action::Evidence authority;need(observe(fixture_binding,r,authority,e)&&title_originals==1&&presentations==1&&operations.at(r.action_uuid).original_steps==3,e);
    need(authority.district_after->values[30]==r.district_title.position[0]&&authority.district_after->flags[11]==1,"custom title full result differs");
    need(retire(fixture_binding,r,e)&&!retire(fixture_binding,r,e),"title retirement repeated");
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&title_originals==1,"title confirmed replayed");
    setup();Config::instance().is_host=false;need(perform(fixture_binding,r,&authority,e)==action::Outcome::Confirmed&&title_originals==1&&presentations==1,e);
    auto reject=[&](auto edit){setup();auto v=r;edit(v);need(perform(fixture_binding,v,nullptr,e)==action::Outcome::RejectedNoEffect&&title_originals==0&&operations.empty(),"invalid title request mutated");};
    reject([](auto& v){v.snapshot_uuid="33333333-3333-4333-8333-333333333333";});
    reject([](auto&){fixture_controller["pending"]=false;});
    reject([](auto&){Config::instance().is_host=false;});
    reject([](auto&){fixture_point_inside=false;});
    reject([](auto&){fixture_world.origin.generation++;});
    for(int kind=1;kind<=5;++kind){setup();failure=kind;
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&title_originals==1&&!e.empty(),"partial title failure not Unknown");
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&title_originals==1,"Unknown title replayed");
    }
    setup();Config::instance().is_host=false;authority.district_before->shape.push_back({{1,2},{3,4}});
    need(perform(fixture_binding,r,&authority,e)==action::Outcome::RejectedNoEffect&&title_originals==0,"foreign complete title shape accepted");
}
}
int main()try{title_fixture::run();std::cout<<checks<<" actual District title capture/cleanup/Host/Client/readback cases passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
