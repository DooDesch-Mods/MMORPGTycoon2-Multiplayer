#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;namespace a=mt2mp::action_session;
namespace {
struct Origin{uintptr_t world=1000,mode=1001;};struct World{Origin origin;};struct Region{uintptr_t region=900;};struct Pin{World world;Region region;};struct Memory{};
struct ToolPin{uintptr_t item=300,type=400,behaviour=100,cursor=200,window=500,character=0;std::string definition_name="EliteMonster";int32_t rank=1,level=1;uint8_t loose=0;std::array<int32_t,2> character_id{-1,-1};};
struct Operation{unsigned attempted=0,returned=0;json preparation;};
struct Runtime{uintptr_t image=0x140000000;}state;
struct Model{std::string fault,definition="EliteMonster",lookup_name="EliteMonster",passed_name;uintptr_t active=999;unsigned tool_reads=0,active_reads=0,lookups=0,activations=0,selections=0,select_calls=0,updates=0;bool selected=false;}model;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool tutorial_clear(Memory&,uintptr_t,const World&,json&,bool& clear,std::string&){clear=true;return true;}
template<class T>bool field(Memory&,uintptr_t,size_t offset,T& value,std::string& e){
 if constexpr(std::is_same_v<T,uintptr_t>){if(offset==0x13df650)value=1000+0x180;else if(offset==0x13df9f0)value=1002;else if(offset==0x190){++model.active_reads;if(model.active_reads==2&&model.fault=="active_read")return fail(e,"native Cursor read failed at +190");value=model.active;}}
 return true;}
uint32_t prepare_region_call(uintptr_t,uintptr_t,uintptr_t){return model.fault=="region_seh"?5:0;}
bool tools(Memory&,uintptr_t,ToolPin& out,json&,std::string& e){++model.tool_reads;out={};out.definition_name=model.definition;out.character=model.selected?600:0;out.character_id=model.selected?std::array<int32_t,2>{1,0}:std::array<int32_t,2>{-1,-1};if(model.tool_reads==2&&model.fault=="inventory_changed")++out.item;return model.fault!="inventory_read"||fail(e,"actual inventory owner changed");}
#include "activation_readback.inc"
uint32_t activate(uintptr_t,const char* name,uintptr_t expected,ActivationReadback& out){++model.lookups;model.passed_name=name;out={};out.lookup_returned=true;
 out.lookup_item=name==model.lookup_name?expected:0;if(model.fault=="lookup_pointer")++out.lookup_item;
 if(out.lookup_item==expected){out.attempted=true;++model.activations;out.returned=model.fault!="activate_seh";if(model.fault!="active_differs")model.active=100;}
 return model.fault=="activate_seh"?5:0;}
bool selection(Memory&,uintptr_t,uintptr_t,ToolPin& out,json& options,std::string& e,bool=false){++model.selections;out.window=500;options=json::array({{1,0}});
 if(model.fault=="window_owner")return fail(e,"Elite ThemeWindow owner membership actual=0 required=1");
 if(model.fault=="window_class")return fail(e,"Elite active ThemeWindow/List unavailable or wrong class");
 out.character=model.selected?600:0;out.character_id=model.selected?std::array<int32_t,2>{1,0}:std::array<int32_t,2>{-1,-1};return true;}
namespace d{struct ReadMemory{Memory& memory;};}
namespace monster_zone_selection{struct Catalogue{int32_t id=0;uintptr_t character=600;};bool read(d::ReadMemory&,uintptr_t,int32_t, Catalogue& out,std::string&){out={};return true;}}
uint32_t original_select(uintptr_t,uintptr_t,uintptr_t){++model.select_calls;model.selected=true;return 0;}
uint32_t update_call(uintptr_t,uintptr_t){++model.updates;return 0;}
bool region_state(Memory&,uintptr_t,const World&,const Region&,const ToolPin&,json& out,std::string&){out={{"ready",true}};return true;}
struct LevelState{uintptr_t widget=700;json output;};
bool level_state(Memory&,uintptr_t,const World&,const Region&,const ToolPin&,LevelState& out,std::string&){out.output={{"rows",json::array({{{"level",1},{"enabled",model.fault!="level_disabled"}}})},{"ready",true},{"selection",{{"minimum",1}}}};return true;}
uint32_t level_command(uintptr_t,uintptr_t,const char*){return 0;}
#include "monster_prepare.inc"
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
a::MonsterIntent intent(){a::MonsterIntent i;i.region=67;i.tool="item_elitemonster";i.character_slot=1;i.character_type=0;i.rank=1;i.level=1;return i;}
bool run(Operation& out,std::string& e){Memory m;Pin p;ToolPin tool;return prepare(m,p,intent(),tool,out,e);}
void reset(){model={};}
}
int main(){try{unsigned checks=0;std::string e;Operation op;
 reset();need(run(op,e),e);need(model.passed_name=="EliteMonster"&&model.lookups==1&&model.activations==1&&model.select_calls==1&&model.updates==1,"actual ItemType definition key not passed to original lookup/activation");need(op.preparation["phase"]=="ready"&&op.preparation["activation"]["lookup_item"]==300&&op.attempted==op.returned,"prepared readback not confirmed");++checks;
 reset();model.definition=model.lookup_name="ObservedAlternateDefinition";need(run(op,e)&&model.passed_name==model.definition,"definition key guessed instead of observed");++checks;
 reset();model.definition="item_elitemonster";need(!run(op,e)&&!model.activations&&!model.selections&&e.find("GetEditItem definition lookup")!=std::string::npos&&op.preparation["activation"]["lookup_item"]==0,"localization-key lookup miss became activation success");++checks;
 reset();model.fault="lookup_pointer";need(!run(op,e)&&!model.activations&&!model.selections&&op.preparation["phase"]=="activation","wrong returned Item identity admitted");++checks;
 reset();model.fault="active_differs";need(!run(op,e)&&model.activations==1&&!model.selections&&op.preparation["phase"]=="active-behaviour"&&op.preparation["active_tool"]["actual_active"]==999&&e.find("expected=100 actual=999")!=std::string::npos,"active mismatch hidden behind ThemeWindow failure");++checks;
 reset();model.fault="active_read";need(!run(op,e)&&e.find("native Cursor read failed at +190")!=std::string::npos&&op.preparation["phase"]=="active-behaviour","Cursor original read failure cause lost");++checks;
 for(const auto* fault:{"window_owner","window_class"}){reset();model.fault=fault;need(!run(op,e)&&op.preparation["phase"]=="theme-window"&&op.preparation["theme_window"]["confirmed"]==false&&op.preparation["theme_window"]["local_window"]==500&&e.find(fault==std::string("window_owner")?"actual=0 required=1":"wrong class")!=std::string::npos,"ThemeWindow original owner/class cause lost");++checks;}
 reset();model.fault="inventory_changed";need(!run(op,e)&&!model.selections&&op.preparation["active_tool"]["actual_item"]==301&&e.find("binding changed")!=std::string::npos,"activation inventory replacement accepted");++checks;
 reset();model.fault="activate_seh";need(!run(op,e)&&op.preparation["activation"]["attempted"]==true&&op.preparation["activation"]["returned"]==false&&!model.selections&&e.find("SEH=5")!=std::string::npos,"activation Unknown hidden");++checks;
 reset();model.active=100;need(run(op,e)&&!model.activations&&!model.lookups&&op.preparation["activation"]["needed"]==false,"already active original tool replayed activation");++checks;
 reset();model.fault="level_disabled";need(!run(op,e)&&op.preparation["phase"]=="level"&&e.find("enabled row")!=std::string::npos,"actual disabled level guard weakened");++checks;
 std::cout<<checks<<" actual Monster prepare-body key/owner/cause checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
