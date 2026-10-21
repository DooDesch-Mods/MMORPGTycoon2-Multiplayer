namespace witness {
struct GraphWitness {uintptr_t graph=0;};
bool read_graph(int,uintptr_t,uintptr_t graph,GraphWitness& out,std::string&){out.graph=graph;return true;}
bool validate_undo_commit(const UndoWitness& before,const UndoWitness& after,const GraphWitness& graph,std::string& error){
    auto expected=before;expected.undo.push_back(graph.graph);
    if(expected!=after){error="fixture original Undo append differs";return false;}return true;
}
}
namespace district_edit_fixture {
namespace property=mt2mp::game::district_property;
namespace history=mt2mp::game::district_edit_history;
namespace contract=mt2mp::game::ui_move_runtime::contract;
namespace district_text {using ::sample;}
bool installed=true,replaying=false,route_valid=true;
unsigned edit_originals=0,remembered=0;int edit_failure=0;
constexpr uintptr_t edit_graph=0xb10000,edit_node=0xb20000;
struct Operation {
    action::Binding binding;action::Request request;district_saved_identity::Current target;
    action::Evidence evidence;witness::UndoWitness undo;uintptr_t graph=0;
    std::map<uintptr_t,history::Stored> inverse;
    uint32_t steps=0;bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,Operation> operations;
bool code(Runtime&,std::string& error){if(!route_valid)error="fixture native body changed";return route_valid;}
bool continent(Runtime&,uintptr_t target,uint32_t& bits,std::string&){need(target==object_ptr,"continent receiver changed");bits=0;return true;}
namespace building_history_slice {
bool remember_district(const std::string& id,uintptr_t graph,std::string&){need(id=="11111111-1111-4111-8111-111111111111"&&graph==edit_graph,"retained history source differs");++remembered;return true;}
}
void make_inverse(){
    auto& memory=fixture_runtime.memory;
    memory.zero(edit_graph,0x120);memory.zero(edit_node,0x200);memory.zero(0xb40000,24);
    memory.put(edit_graph,image+0x16e32d0);memory.put(edit_graph+0x18,image+0x16e2dc0);memory.put(edit_graph+0x20,uintptr_t(0xb30000));
    memory.put(edit_graph+0x28,int32_t(1));memory.put(edit_graph+0x2c,int32_t(1));memory.put(0xb30000,edit_node);
    memory.put(edit_graph+0x48,image+0x1748c50);memory.put(edit_graph+0x10c,uint8_t(1));
    memory.put(edit_node,image+0x17132b0);memory.put(edit_node+0xb8,object_uid);memory.put(edit_node+0xc0,region_ptr);memory.put(edit_node+0xc8,map_ptr+0x1c0);
    memory.put(edit_node+0xf0,uintptr_t(0xb40000));memory.text(edit_node+0x160,0xb50000,"Old");memory.text(edit_node+0x180,0xb60000,"Description");
}
uint32_t invoke(uintptr_t base,uintptr_t object,const property::Value& value,uint32_t& steps){
    need(base==image&&object==object_ptr&&replaying,"District native execution lost exact receiver/replay scope");++edit_originals;
    if(edit_failure==1){steps=1;return 0xc0000005;}
    const auto* field=property::field(value.kind);
    if(field->original_history){make_inverse();if(edit_failure!=2)witness::fixture_undo.undo.push_back(edit_graph);}
    auto& memory=fixture_runtime.memory;
    if(field->type==property::Type::Flag)memory.put(object+field->offset,uint8_t(value.bits[0]));
    else if(field->type==property::Type::Color)memory.put(object+field->offset,value.bits);
    else memory.put(object+field->offset,value.bits[0]);
    if(edit_failure==3)memory.put(edit_node+0xb8,object_uid+1);
    if(edit_failure==4)fixture_money.line_counts[0]++;
    if(edit_failure==5)memory.put(object+0x351,uint8_t(1));
    steps=15;return 0;
}
#include "district_edit_perform_actual.inc"
void setup(){
    ::reset();operations.clear();installed=route_valid=true;replaying=false;edit_originals=remembered=0;edit_failure=0;
    fixture_controller["host_revision"]=uint64_t(1);
}
void run(){
    setup();auto request_value=::request();request_value.action=action::Action::EditDistrict;request_value.district_text={};
    request_value.district_property={property::Kind::Height,{std::bit_cast<uint32_t>(2.f),0,0,0}};
    std::string error;action::Evidence authority;
    need(perform(fixture_binding,request_value,nullptr,error)==action::Outcome::Confirmed,error);
    need(edit_originals==1&&observe(fixture_binding,request_value,authority,error)&&authority.undo_confirmed,error);
    need(retire(fixture_binding,request_value,error)&&remembered==1&&!retire(fixture_binding,request_value,error),"District original history registered more than once");
    need(perform(fixture_binding,request_value,nullptr,error)==action::Outcome::Unknown&&edit_originals==1,"District confirmed native edit replayed");
    setup();Config::instance().is_host=false;
    need(perform(fixture_binding,request_value,&authority,error)==action::Outcome::Confirmed&&edit_originals==1,error);
    setup();Config::instance().is_host=false;auto wrong=authority;wrong.district_before->values[23]=std::bit_cast<uint32_t>(3.f);
    need(perform(fixture_binding,request_value,&wrong,error)==action::Outcome::RejectedNoEffect&&edit_originals==0,"foreign Host before state reached native original");
    setup();route_valid=false;need(perform(fixture_binding,request_value,nullptr,error)==action::Outcome::RejectedNoEffect&&edit_originals==0,"changed native body reached original");
    setup();fixture_controller["action_uuid"]="other";need(perform(fixture_binding,request_value,nullptr,error)==action::Outcome::RejectedNoEffect&&edit_originals==0,"foreign pending action reached original");
    for(int failure=1;failure<=5;++failure){setup();edit_failure=failure;
        need(perform(fixture_binding,request_value,nullptr,error)==action::Outcome::Unknown&&edit_originals==1&&!error.empty(),"District partial original failure success-shaped");
        need(perform(fixture_binding,request_value,nullptr,error)==action::Outcome::Unknown&&edit_originals==1,"District unknown native edit replayed");
    }
    setup();auto no_history=request_value;no_history.district_property={property::Kind::WithGrass,{1,0,0,0}};
    need(perform(fixture_binding,no_history,nullptr,error)==action::Outcome::Confirmed,error);
    action::Evidence observed;need(observe(fixture_binding,no_history,observed,error)&&!observed.undo_confirmed&&retire(fixture_binding,no_history,error)&&remembered==0,"property invented native history");
    fixture_runtime.memory.put(object_ptr+0x350,uint8_t(1));
    need(!observe(fixture_binding,no_history,observed,error),"changed complete District state accepted at ACK");
}
}
