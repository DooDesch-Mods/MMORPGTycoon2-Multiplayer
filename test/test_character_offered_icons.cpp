#include "typed_action/action_session.h"
#define main fixture_controls_regression
#include "test_character_offered_controls.cpp"
#undef main
namespace a=mt2mp::action_session;
std::map<uintptr_t,a::AbilityRecord> fixture_ability_records;
namespace ability_edit {
bool read_record(Memory&,uintptr_t object,a::AbilityRecord& record,std::string& error){
    const auto found=fixture_ability_records.find(object);if(found==fixture_ability_records.end())return fail(error,"fixture native record absent");record=found->second;return true;
}}
#define state fixture_native_state
#include "character_offered_icons_actual.inc"
#undef state
void icon_setup(){
    setup();fixture_ability_records.clear();pane(0xa000,0x1717ed0,0x6000,0x1000);
    put(0xa000,0x320,0x55000);put(0xa000,0x328,0x45000);put(0x55000,0,fixture_native_state.image+0x171ca70);put(0x55000,0x340,0x45000);put(0x55000,0x378,0);
    fixture_text[{0xa000,0x1b0}]="{design_character_tab_abilities}";
    put(fixture_native_state.image+0x1717ed0,0x178,fixture_native_state.image+0x8167c0);
    pane(0x6000,0x16dfe90,0x5000,0x1000);put(0x6000,0x410,0xa000);put(0x6000,0x418,0xa000);
    put(0xa000,0x2d0,0x49000);put(0xa000,0x2d8,2);put(0xa000,0x2dc,2);put(0xa000,0x300,0);put(0xa000,0x308,0);put(0xa000,0x30c,0);
    put(0x49000,0,0x51000);put(0x49000,8,0x52000);
    put(0x45000,0x130,0x46000);put(0x45000,0x138,2);put(0x45000,0x13c,2);put(0x46000,0,0x47000);put(0x46000,8,0x48000);
    put(0x47000,0x18,101);put(0x48000,0x18,102);
    a::AbilityRecord first;first.text[0]="Actual first ability";first.level=3;first.effects.push_back({});fixture_ability_records[0x47000]=first;
    a::AbilityRecord second;second.text[0]="Actual zero-effect ability";second.level=5;fixture_ability_records[0x48000]=second;
    for(auto icon:{uintptr_t(0x51000),uintptr_t(0x52000)}){
        pane(icon,0x1717790,0xa000,0x1000);put(icon,0x174,1);put(icon,0x395,1);put(icon,0x396,0);put(icon,0x39a,0);put(icon,0x330,0x45000);
    }
    put(0x51000,0x320,0x48000);put(0x51000,0x328,0x53000);put(0x53000,0,0x48038);put(0x53000,8,1);
    put(0x52000,0x320,0x47000);put(0x52000,0x328,0x54000);put(0x54000,0,0x47038);put(0x54000,8,1);
}
int main()try{
    if(fixture_controls_regression())return 1;
    Memory memory;json rows;OfferedControl icon;std::string error;unsigned checks=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);error.clear();};
    icon_setup();check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error)&&rows.size()==2&&rows.at(0).at("slot")==0&&rows.at(0).at("uid")==101&&
        rows.at(0).at("name")=="Actual first ability"&&rows.at(0).at("level")==3&&rows.at(0).at("positive_effects").is_boolean()&&rows.at(0).at("positive_effects").get<bool>()&&
        !rows.at(1).at("positive_effects").get<bool>(),"actual icon order was mistaken for canonical slot order");
    check(ability_icon(memory,0x2000,0x1000,0x45000,0,icon,error)&&icon.pane==0x52000,"requested actual slot selected another UI icon");
    icon_setup();put(0x51000,0x395,0);check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error)&&rows.size()==1&&rows.at(0).at("slot")==0,"disabled icon was exposed as an offer");
    check(!ability_icon(memory,0x2000,0x1000,0x45000,1,icon,error),"disabled canonical slot opened an editor");
    put(0x52000,0x395,0);check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error)&&rows.empty(),"zero eligible icons did not return successful empty catalogue");
    icon_setup();put(0x51000,0x330,0x99999);check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error)&&rows.size()==1&&rows.at(0).at("slot")==0,"foreign-owner icon was mapped to selected owner");
    icon_setup();put(0x54000,0,0x47000);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"wrong weak-target base admitted");
    icon_setup();put(0x54000,8,0);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"unowned native icon lease admitted");
    icon_setup();put(0x46000,8,0x47000);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"duplicate canonical pointer admitted");
    icon_setup();put(0x51000,0x320,0x47000);put(0x53000,0,0x47038);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"duplicate offered slot was ambiguous");
    icon_setup();put(0x51000,0x320,0x47001);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"noncanonical icon pointer was offered");
    icon_setup();put(0x46000,0,0);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"null owned slot admitted");
    icon_setup();put(0x6000,0x410,0x9000);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"unshown Ability page was used as an offer");
    icon_setup();put(0xa000,0x280,0x99999);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error),"foreign actual Ability page was accepted");
    icon_setup();put(0x47000,0x18,0);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error)&&!error.empty(),"missing UID became success-shaped default");
    icon_setup();fixture_ability_records.erase(0x47000);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error)&&error=="fixture native record absent","native record read failure lost its cause");
    json census;icon_setup();put(0x55000,0x378,1);
    check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error,&census)&&rows.empty()&&!census.at("catalogue_ready").get<bool>()&&census.at("rebuild_pending").get<bool>()&&census.at("canonical_ability_count")==2,"pending original rebuild became final empty catalogue");
    check(!ability_icon(memory,0x2000,0x1000,0x45000,0,icon,error),"pending original rebuild opened stale icon");
    put(0x55000,0x378,0);
    check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error,&census)&&rows.size()==2&&census.at("catalogue_ready").get<bool>()&&census.at("eligible_icon_count")==2,"original rebuild completion did not expose real offers");
    put(0x51000,0x395,0);put(0x52000,0x395,0);
    check(ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error,&census)&&rows.empty()&&census.at("catalogue_ready").get<bool>()&&census.at("disabled_icon_count")==2&&census.at("visible_icon_count")==2,"ready disabled icons became indefinite pending");
    icon_setup();put(0x55000,0x378,2);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error,&census),"invalid rebuild flag admitted");
    icon_setup();put(0x55000,0x340,0x99999);check(!ability_catalogue(memory,0x2000,0x1000,0x45000,rows,error,&census),"foreign Slots owner admitted");
    std::cout<<checks<<" actual offered icon-to-canonical-slot assertions passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
