#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <vendor/nlohmann/json.hpp>
using json=nlohmann::json;
struct Memory {};
struct FixtureState {uintptr_t image=0x140000000;} fixture_native_state;
std::map<std::pair<uintptr_t,size_t>,uint64_t> fixture_values;
std::map<std::pair<uintptr_t,size_t>,std::string> fixture_text;
bool fail(std::string& error,const std::string& why){error=why;return false;}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& value,std::string& error){
    const auto found=fixture_values.find({object,offset});if(found==fixture_values.end())return fail(error,"fixture field absent");
    value=static_cast<T>(found->second);return true;
}
bool field(Memory& memory,uintptr_t object,size_t offset,std::array<uint32_t,8>& value,std::string& error){
    for(size_t i=0;i<value.size();++i)if(!field(memory,object,offset+i*4,value[i],error))return false;return true;
}
namespace npc_quest_records {
bool read_string(Memory&,uintptr_t object,size_t offset,const char*,std::string& value,std::string& error){
    const auto found=fixture_text.find({object,offset});if(found==fixture_text.end())return fail(error,"fixture string absent");value=found->second;return true;
}}
#define state fixture_native_state
#include "character_open_array_actual.inc"
#include "../src/game/character_offered_controls.inc"
#undef state
#include "character_offered_control_state_actual.inc"
void put(uintptr_t object,size_t offset,uint64_t value){fixture_values[{object,offset}]=value;}
void pane(uintptr_t object,uintptr_t ap,uintptr_t parent,uintptr_t owner){
    put(object,0,fixture_native_state.image+ap);put(fixture_native_state.image+ap,0x178,fixture_native_state.image+0xc35730);
    put(object,0x278,parent);put(object,0x280,owner);put(object,0x69,1);fixture_text[{object,0x1b0}]="";
}
void setup(){
    fixture_values.clear();fixture_text.clear();put(0x1000,0x45f,1);
    pane(0x2000,0x1111,0x1000,0x1000);put(fixture_native_state.image+0x1111,0x178,fixture_native_state.image+0x8167c0);
    put(0x2000,0x2d0,0x3000);put(0x2000,0x2d8,2);put(0x2000,0x2dc,2);
    put(0x2000,0x300,0);put(0x2000,0x308,0);put(0x2000,0x30c,0);put(0x3000,0,0x4000);put(0x3000,8,0x5000);
    pane(0x4000,0x16d98c0,0x2000,0x1000);put(0x4000,0x174,1);put(0x4000,0x344,0);put(0x4000,0x346,0);
    put(0x4000,0x320,0xe000);put(0x4000,0x328,0xe100);put(0xe000,0x51,0);put(0xe100,0x51,0);
    fixture_text[{0x4000,0x348}]="MessageTo Parent WeaponCategory Staff";
    pane(0x5000,0x17015b0,0x2000,0x1000);fixture_text[{0x5000,0x1b0}]="multi";put(0x5000,0x3b8,0x6000);
    put(0x6000,0,fixture_native_state.image+0x16dfe90);put(0x6000,0x388,0x7000);put(0x6000,0x390,2);put(0x6000,0x394,2);
    put(0x6000,0x3a0,0x8000);put(0x6000,0x3a8,2);put(0x6000,0x3ac,2);put(0x8000,0,1);put(0x8000,1,1);
    put(0x7000,0,0x9000);put(0x7000,8,0xa000);fixture_text[{0x9000,0x1b0}]="General";fixture_text[{0xa000,0x1b0}]="Costume";
    put(0x6000,0x410,0x9000);put(0x6000,0x418,0x9000);
}
int main()try{
    Memory memory;std::string error;std::vector<OfferedControl> controls;OfferedControl control,tab;OfferedPage page;unsigned checks=0;
    const auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::string(why)+" cause="+error);error.clear();};
    const std::string command="MessageTo Parent WeaponCategory Staff";
    setup();check(offered_controls(memory,0x2000,0x1000,controls,error)&&controls.size()==2,"actual control traversal");
    check(offered_unique(controls,command,true,control,error)&&control.pane==0x4000&&!control.checked,"actual eligible checkbox");
    check(offered_tab(controls,"multi",tab,error)&&offered_page(memory,tab,"Costume",page,error)&&page.page==0xa000&&page.current==0x9000,"enabled catalogue page");
    auto duplicate=controls;duplicate.push_back(control);check(!offered_unique(duplicate,command,true,control,error),"duplicate command admitted");
    check(!offered_unique(controls,"MessageTo Parent WeaponCategory Sword",true,control,error),"unoffered category admitted");
    for(auto offset:{size_t(0x344),size_t(0x174),size_t(0x69),size_t(0x280)}){
        setup();put(0x4000,offset,offset==0x344?1:0);check(offered_controls(memory,0x2000,0x1000,controls,error)&&
            !offered_unique(controls,command,true,control,error),"unready/hidden/foreign checkbox admitted");}
    setup();put(0x4000,0x278,0x4000);check(!offered_controls(memory,0x2000,0x1000,controls,error),"cyclic ancestry accepted");
    setup();put(0x8000,1,0);check(!offered_page(memory,tab,"Costume",page,error),"locked catalogue page admitted");
    setup();put(0x8000,1,2);check(!offered_page(memory,tab,"Costume",page,error),"nonBoolean enable accepted");
    setup();put(0x6000,0x3a8,1);check(!offered_page(memory,tab,"Costume",page,error),"mismatched enabled count accepted");
    setup();put(0x6000,0x394,1);check(!offered_page(memory,tab,"Costume",page,error),"invalid catalogue bounds accepted");
    setup();fixture_text[{0x9000,0x1b0}]="Costume";check(!offered_page(memory,tab,"Costume",page,error),"duplicate catalogue page admitted");
    setup();put(fixture_native_state.image+0x16d98c0,0x178,0x123);check(!offered_controls(memory,0x2000,0x1000,controls,error),"unknown native gather accepted");
    setup();put(0x3000,8,0x4000);check(!offered_controls(memory,0x2000,0x1000,controls,error),"duplicate content traversal accepted");
    setup();bool visible=false;check(offered_visibility(memory,0x4000,0x1000,0x1000,visible,error)&&visible,"window-root offered control was rejected");
    setup();put(0x4000,0x328,0);check(!offered_controls(memory,0x2000,0x1000,controls,error),"missing native Press visual admitted");
    setup();put(0xe000,0x51,1);check(!offered_controls(memory,0x2000,0x1000,controls,error),"checkbox visual and current value differ");
    setup();OfferedControl text;text.pane=0xf000;text.id="actual_unsent";text.ready=text.text=true;
    fixture_text[{text.pane,0x2d0}]="Unsent Ä text";fixture_text[{text.pane,0x4a8}]="Unsent input cache";fixture_text[{text.pane,0x4d8}]="Earlier baseline";
    put(text.pane,0x584,1);for(size_t i=0;i<8;++i)put(text.pane,0x544+i*4,i+3);
    json rows;check(ability_controls(memory,{text},rows,error)&&rows.at(0).at("input_buffer")=="Unsent input cache"&&
        rows.at(0).at("commit_baseline")=="Earlier baseline"&&rows.at(0).at("selection").at(7)==10&&rows.at(0).at("editing")==1,"actual unsent buffer/selection readback");
    fixture_values.erase({text.pane,0x544});check(!ability_controls(memory,{text},rows,error),"missing presentation evidence became a success-shaped default");
    std::cout<<checks<<" Character offered-control assertions passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
