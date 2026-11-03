namespace policy_ui_test {
namespace gp=actual;
struct Runtime {uintptr_t image=0x10000000;};
Runtime fixture_runtime;
std::map<std::pair<uintptr_t,size_t>,uintptr_t> fixture_fields;
std::map<uintptr_t,std::string> fixture_text;
std::map<uintptr_t,std::vector<uintptr_t>> fixture_children;
bool fixture_registered=true,fixture_visible=true;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool at(Runtime&,uintptr_t object,size_t offset,T& out,std::string& error){const auto found=fixture_fields.find({object,offset});if(found==fixture_fields.end())return fail(error,"fixture UI field absent");out=T(found->second);return true;}
bool text(Runtime&,uintptr_t object,std::string& out,size_t,std::string& error){const auto found=fixture_text.find(object);if(found==fixture_text.end())return fail(error,"fixture UI text absent");out=found->second;return true;}
bool ap(Runtime& runtime,uintptr_t object,uintptr_t expected,std::string& error){uintptr_t actual=0;return at(runtime,object,0,actual,error)&&(actual==runtime.image+expected||fail(error,"fixture UI AP differs"));}
bool gm_fixture_window(Runtime&,uintptr_t& target,bool,std::string& error){target=100;return fixture_registered||fail(error,"fixture registered GM window absent");}
bool rules_numeric_visible(Runtime&,uintptr_t,uintptr_t,bool& visible,std::string&){visible=fixture_visible;return true;}
bool service_ui_children(Runtime&,uintptr_t pane,uintptr_t,uintptr_t,std::vector<uintptr_t>& pending,std::string&){for(auto child:fixture_children[pane])pending.push_back(child);return true;}
#include "policy_ui_children_actual.inc"
#include "policy_ui_find_actual.inc"
void reset(){fixture_runtime={};fixture_registered=fixture_visible=true;
    fixture_fields={{{100,0},fixture_runtime.image+0x172c6d0},{{100,0x45f},1},{{500,0},fixture_runtime.image+0x17015b0},{{500,0x280},100},{{500,0x3b8},600},{{600,0},fixture_runtime.image+0x16dfe90},{{700,0},fixture_runtime.image+0x16d98c0},{{700,0x3c8},11000},{{700,0x346},0},{{700,0x174},1},{{11000,0},fixture_runtime.image+0x1749610},{{11000,8},5000+0x48},{{11000,0x38},5000+0x68}};
    fixture_text={{500+0x1b0,"multi"},{700+0x1b0,"checklogs"},{11000+0x18,"checkLog"}};fixture_fields[{600,0x280}]=100;fixture_fields[{600,0x410}]=800;fixture_fields[{600,0x418}]=800;fixture_fields[{600,0x408}]=0;
    fixture_fields[{600,0x3e0}]=0x656c6f7370;fixture_fields[{800,0}]=fixture_runtime.image+0x1700000;
    fixture_text[800+0x1b0]="{gamemaster_tab_policy}";fixture_text[600+0x3e8]="{gamemaster_tab_policy}";
    fixture_fields[{600,0x278}]=500;fixture_fields[{800,0x278}]=600;fixture_fields[{800,0x280}]=100;fixture_text[500+0x360]="";
    fixture_children={{100,{500}},{800,{700}}};
}
void run(){std::string error;PolicyUi actual;reset();need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&actual.visible&&actual.ready&&actual.tabs==500&&actual.checkbox==700&&!actual.checked,"actual offered Chat CheckLog finder failed");
    fixture_text[600+0x3e8]="";need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&actual.ready&&actual.checkbox==700,"Native empty default lost actual settled Policy controls");
    fixture_fields[{800,0x278}]=999;need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"Foreign default Policy parent accepted");reset();
    fixture_fields[{100,0x45f}]=0;need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.visible&&!actual.ready&&!actual.checkbox,"hidden policy control offered");
    reset();fixture_visible=false;need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.ready,"hidden ancestor policy control offered");
    reset();fixture_fields[{700,0x174}]=0;need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.ready,"nonclickable CheckLog offered");
    reset();fixture_fields[{11000,8}]=5000+0x70;need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.checkbox,"cheat CheckLog substituted for chat");
    reset();fixture_fields[{11000,0x38}]++;need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.checkbox,"foreign member substituted for CheckLog");
    reset();fixture_text[11000+0x18]="enabled";need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"wrong bound property accepted");
    reset();fixture_fields[{700,0x346}]=2;need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"invalid checkbox flag accepted");
    for(int state=1;state<=2;++state){reset();fixture_fields[{600,0x408}]=uintptr_t(state);need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.checkbox,"transitioning native page offered a control");}
    reset();fixture_fields[{600,0x418}]=801;need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.checkbox,"different next page offered a current control");
    reset();fixture_fields[{600,0x408}]=0x100;need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"32-bit invalid transition silently truncated to ready");
    reset();fixture_text[600+0x3e8]="foreign";need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.checkbox,"mismatched requested page offered controls");
    reset();fixture_fields[{500,0x280}]=101;need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"foreign actual tab owner accepted");
    reset();fixture_children[100].push_back(700);need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"active pane cycle accepted");
    reset();fixture_registered=false;need(!policy_ui_find(fixture_runtime,5000,false,actual,error),"absent original GM window synthesized");
    reset();fixture_fields[{11000,0x38}]=5000+0x60;fixture_text[11000+0x18]="enabled";fixture_text[700+0x1b0]="enable_toxic_complaints";fixture_fields[{700,0x346}]=1;
    need(policy_ui_find(fixture_runtime,5000,true,actual,error)&&actual.ready&&actual.checked&&actual.checkbox==700,"actual offered Chat Enabled finder failed");
    need(policy_ui_find(fixture_runtime,5000,false,actual,error)&&!actual.checkbox,"Enabled substituted for CheckLog");
    for(uint8_t p=0;p<2;++p)for(size_t f=0;f<gp::names.size();++f){reset();const auto field=gp::Field(f);const bool integer=gp::integer(field);const auto owner=5000+(p?0x70:0x48);
        fixture_fields[{700,0}]=fixture_runtime.image+(integer?0x16e7070:0x16d98c0);fixture_fields[{700,integer?0x420:0x3c8}]=11000;
        fixture_fields[{11000,0}]=fixture_runtime.image+(integer?0x17496b0:0x1749610);fixture_fields[{11000,8}]=owner;fixture_fields[{11000,0x38}]=owner+gp::offsets[f];fixture_text[11000+0x18]=gp::names[f];fixture_text[700+0x1b0]=field==gp::Field::Enabled?"enable_toxic_complaints":"checklogs";
        fixture_fields[{700,0x320}]=0;fixture_fields[{700,0x324}]=gp::maximum(field);fixture_fields[{700,0x428}]=gp::maximum(field);fixture_fields[{700,0x3f1}]=1;
        need(policy_ui_find(fixture_runtime,5000,p,field,actual,error)&&actual.ready&&(integer?actual.spinner:actual.checkbox)==700,"offered Chat/Cheat control not found");
        if(field==gp::Field::Enabled||field==gp::Field::CheckLog){const auto name=fixture_text[700+0x1b0];fixture_text[700+0x1b0]="foreign";need(!policy_ui_find(fixture_runtime,5000,p,field,actual,error),"original identified checkbox ID guard weakened");fixture_text[700+0x1b0]=name;}
        need(policy_ui_find(fixture_runtime,5000,uint8_t(1-p),field,actual,error)&&!actual.ready,"opposite policy control substituted");
        if(integer){fixture_fields[{700,0x324}]++;need(!policy_ui_find(fixture_runtime,5000,p,field,actual,error),"unexpected spinner range accepted");fixture_fields[{700,0x324}]=gp::maximum(field);fixture_fields[{700,0x3f1}]=0;need(policy_ui_find(fixture_runtime,5000,p,field,actual,error)&&!actual.ready,"disabled policy spinner offered");}
    }
    need(!policy_ui_find(fixture_runtime,5000,2,gp::Field::Enabled,actual,error),"foreign policy row offered");
    need(!policy_ui_find(fixture_runtime,5000,0,gp::Field(7),actual,error),"foreign policy property offered");
}
}
